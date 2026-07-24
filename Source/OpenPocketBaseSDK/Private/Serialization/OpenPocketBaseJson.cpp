#include "Serialization/OpenPocketBaseJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "OpenPocketBaseDate.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FOpenPocketBaseError MakeTransportError(const FOpenPocketBaseHttpResponse& Response)
{
    FOpenPocketBaseError Error;
    Error.Kind = Response.bTimedOut
        ? EOpenPocketBaseErrorKind::Timeout
        : EOpenPocketBaseErrorKind::Transport;
    Error.HttpStatus = Response.HttpStatus;
    if (Response.bTimedOut)
    {
        Error.Message = TEXT("PocketBase did not respond before the request timeout. Check that the server is reachable or increase the request timeout.");
    }
    else if (Response.ErrorMessage.IsEmpty())
    {
        Error.Message = TEXT("PocketBase did not return a response. Check the server URL, confirm the server is running, and verify network access.");
    }
    else
    {
        Error.Message = FString::Printf(
            TEXT("PocketBase did not return a response. Transport reported: %s"),
            *Response.ErrorMessage);
    }
    Error.bMayRetry = true;
    Error.RequestId = Response.RequestId;
    return Error;
}

bool IsSuccessStatus(const int32 Status)
{
    return Status >= 200 && Status < 300;
}

FString DecodeBody(const TArray<uint8>& Body)
{
    if (Body.IsEmpty())
    {
        return FString();
    }

    const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Body.GetData()), Body.Num());
    return FString(Converted.Length(), Converted.Get());
}

bool ParseObject(const TArray<uint8>& Body, TSharedPtr<FJsonObject>& OutObject)
{
    const FString Json = DecodeBody(Body);
    if (Json.IsEmpty())
    {
        return false;
    }

    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}

bool ParseArray(const TArray<uint8>& Body, TArray<TSharedPtr<FJsonValue>>& OutArray)
{
    const FString Json = DecodeBody(Body);
    if (Json.IsEmpty())
    {
        return false;
    }

    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    return FJsonSerializer::Deserialize(Reader, OutArray);
}

FOpenPocketBaseError MakeHttpError(const FOpenPocketBaseHttpResponse& Response)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::Http;
    Error.HttpStatus = Response.HttpStatus;
    Error.Message = FString::Printf(
        TEXT("PocketBase returned HTTP %d without a valid PocketBase error message."),
        Response.HttpStatus);
    Error.bMayRetry = Response.HttpStatus >= 500 && Response.HttpStatus < 600;
    Error.RequestId = Response.RequestId;

    TSharedPtr<FJsonObject> Object;
    if (!ParseObject(Response.Body, Object))
    {
        return Error;
    }

    Error.Kind = EOpenPocketBaseErrorKind::PocketBase;
    FString PocketBaseMessage;
    if (Object->TryGetStringField(TEXT("message"), PocketBaseMessage) &&
        !PocketBaseMessage.TrimStartAndEnd().IsEmpty())
    {
        Error.Message = MoveTemp(PocketBaseMessage);
    }
    Object->TryGetStringField(TEXT("code"), Error.Code);

    const TSharedPtr<FJsonObject>* Data = nullptr;
    if (Object->TryGetObjectField(TEXT("data"), Data) && Data != nullptr)
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Data)->Values)
        {
            const TSharedPtr<FJsonObject>* FieldObject = nullptr;
            if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(FieldObject) || FieldObject == nullptr)
            {
                continue;
            }

            FOpenPocketBaseFieldError FieldError;
            (*FieldObject)->TryGetStringField(TEXT("code"), FieldError.Code);
            (*FieldObject)->TryGetStringField(TEXT("message"), FieldError.Message);
            Error.FieldErrors.Add(Pair.Key, MoveTemp(FieldError));
        }
    }

    return Error;
}

FOpenPocketBaseError MakeBatchHttpError(const FOpenPocketBaseHttpResponse& Response)
{
    FOpenPocketBaseError Error = MakeHttpError(Response);
    if (Response.HttpStatus == 403 &&
        Error.Message.Contains(TEXT("Batch requests are not allowed"), ESearchCase::IgnoreCase))
    {
        Error.Kind = EOpenPocketBaseErrorKind::Unsupported;
        Error.Code = TEXT("batch_disabled");
        Error.bMayRetry = false;
        return Error;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedPtr<FJsonObject>* Data = nullptr;
    const TSharedPtr<FJsonObject>* Requests = nullptr;
    if (!ParseObject(Response.Body, Root) ||
        !Root->TryGetObjectField(TEXT("data"), Data) || Data == nullptr ||
        !(*Data)->TryGetObjectField(TEXT("requests"), Requests) || Requests == nullptr)
    {
        return Error;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& RequestPair : (*Requests)->Values)
    {
        const TSharedPtr<FJsonObject>* RequestFailure = nullptr;
        if (!RequestPair.Value.IsValid() ||
            !RequestPair.Value->TryGetObject(RequestFailure) || RequestFailure == nullptr)
        {
            continue;
        }

        if (Error.Code.IsEmpty())
        {
            (*RequestFailure)->TryGetStringField(TEXT("code"), Error.Code);
        }
        const TSharedPtr<FJsonObject>* FailureResponse = nullptr;
        const TSharedPtr<FJsonObject>* FieldData = nullptr;
        if (!(*RequestFailure)->TryGetObjectField(TEXT("response"), FailureResponse) ||
            FailureResponse == nullptr ||
            !(*FailureResponse)->TryGetObjectField(TEXT("data"), FieldData) || FieldData == nullptr)
        {
            continue;
        }

        for (const TPair<FString, TSharedPtr<FJsonValue>>& FieldPair : (*FieldData)->Values)
        {
            const TSharedPtr<FJsonObject>* FieldObject = nullptr;
            if (!FieldPair.Value.IsValid() ||
                !FieldPair.Value->TryGetObject(FieldObject) || FieldObject == nullptr)
            {
                continue;
            }

            FOpenPocketBaseFieldError FieldError;
            (*FieldObject)->TryGetStringField(TEXT("code"), FieldError.Code);
            (*FieldObject)->TryGetStringField(TEXT("message"), FieldError.Message);
            Error.FieldErrors.Add(
                FString::Printf(TEXT("requests.%s.%s"), *RequestPair.Key, *FieldPair.Key),
                MoveTemp(FieldError));
        }
    }
    return Error;
}

FOpenPocketBaseError MakeSerializationError(
    const FOpenPocketBaseHttpResponse& Response,
    FString Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::Serialization;
    Error.HttpStatus = Response.HttpStatus;
    Error.Message = MoveTemp(Message);
    Error.RequestId = Response.RequestId;
    return Error;
}

FJsonObjectWrapper WrapObject(const TSharedRef<FJsonObject>& Object)
{
    FJsonObjectWrapper Wrapper;
    Wrapper.JsonObject = Object;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Wrapper.JsonString);
    FJsonSerializer::Serialize(Object, Writer);
    return Wrapper;
}

bool IsRecordSystemField(const FString& Name)
{
    return Name == TEXT("id") || Name == TEXT("collectionId") ||
        Name == TEXT("collectionName") || Name == TEXT("created") ||
        Name == TEXT("updated") || Name == TEXT("expand");
}

FJsonObjectWrapper WrapRecordData(const TSharedRef<FJsonObject>& Object)
{
    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
    {
        if (!IsRecordSystemField(Field.Key))
        {
            Data->SetField(Field.Key, Field.Value);
        }
    }
    return WrapObject(Data);
}

bool ParseRecordObject(
    const TSharedRef<FJsonObject>& Object,
    FOpenPocketBaseRecord& OutRecord,
    FString* OutFailureReason = nullptr)
{
    OutRecord = FOpenPocketBaseRecord();
    if (!Object->TryGetStringField(TEXT("id"), OutRecord.Id) || OutRecord.Id.IsEmpty())
    {
        if (OutFailureReason != nullptr)
        {
            *OutFailureReason = TEXT("The record is missing its non-empty string 'id' field.");
        }
        return false;
    }

    Object->TryGetStringField(TEXT("collectionId"), OutRecord.CollectionId);
    Object->TryGetStringField(TEXT("collectionName"), OutRecord.CollectionName);

    FString DateValue;
    if (Object->TryGetStringField(TEXT("created"), DateValue) &&
        !OpenPocketBase::Date::TryParse(DateValue, OutRecord.Created))
    {
        if (OutFailureReason != nullptr)
        {
            *OutFailureReason = TEXT("The record's 'created' field is not a valid PocketBase date.");
        }
        return false;
    }
    if (Object->TryGetStringField(TEXT("updated"), DateValue) &&
        !OpenPocketBase::Date::TryParse(DateValue, OutRecord.Updated))
    {
        if (OutFailureReason != nullptr)
        {
            *OutFailureReason = TEXT("The record's 'updated' field is not a valid PocketBase date.");
        }
        return false;
    }

    OutRecord.Data = WrapRecordData(Object);
    const TSharedPtr<FJsonObject>* Expanded = nullptr;
    if (Object->TryGetObjectField(TEXT("expand"), Expanded) && Expanded != nullptr)
    {
        OutRecord.Expanded = WrapObject(Expanded->ToSharedRef());
    }
    return true;
}

template <typename ValueType>
TOptional<TOpenPocketBaseResult<ValueType>> ValidateResponse(
    const FOpenPocketBaseHttpResponse& Response)
{
    if (!Response.bTransportSucceeded)
    {
        return TOpenPocketBaseResult<ValueType>::Failure(MakeTransportError(Response));
    }
    if (!IsSuccessStatus(Response.HttpStatus))
    {
        return TOpenPocketBaseResult<ValueType>::Failure(MakeHttpError(Response));
    }
    return {};
}
}

namespace OpenPocketBase::Json
{
bool TryParseRecordObject(
    const TSharedRef<FJsonObject>& Object,
    FOpenPocketBaseRecord& OutRecord)
{
    return ParseRecordObject(Object, OutRecord);
}

TSharedRef<FJsonObject> MakeRecordObject(const FOpenPocketBaseRecord& Record)
{
    const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    if (Record.Data.JsonObject.IsValid())
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Record.Data.JsonObject->Values)
        {
            if (!IsRecordSystemField(Field.Key))
            {
                Object->SetField(Field.Key, Field.Value);
            }
        }
    }
    Object->SetStringField(TEXT("id"), Record.Id);
    if (!Record.CollectionId.IsEmpty())
    {
        Object->SetStringField(TEXT("collectionId"), Record.CollectionId);
    }
    if (!Record.CollectionName.IsEmpty())
    {
        Object->SetStringField(TEXT("collectionName"), Record.CollectionName);
    }
    if (Record.Created.GetTicks() > 0)
    {
        Object->SetStringField(TEXT("created"), OpenPocketBase::Date::Format(Record.Created));
    }
    if (Record.Updated.GetTicks() > 0)
    {
        Object->SetStringField(TEXT("updated"), OpenPocketBase::Date::Format(Record.Updated));
    }
    if (Record.Expanded.JsonObject.IsValid())
    {
        Object->SetObjectField(TEXT("expand"), Record.Expanded.JsonObject);
    }
    return Object;
}

TOpenPocketBaseResult<FOpenPocketBaseRecord> ParseRecordResponse(
    const FOpenPocketBaseHttpResponse& Response)
{
    if (TOptional<TOpenPocketBaseResult<FOpenPocketBaseRecord>> Failure =
            ValidateResponse<FOpenPocketBaseRecord>(Response))
    {
        return MoveTemp(Failure.GetValue());
    }

    TSharedPtr<FJsonObject> Object;
    FOpenPocketBaseRecord Record;
    if (!ParseObject(Response.Body, Object))
    {
        return TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase returned a successful record response that is not a JSON object. Confirm the route is a standard records endpoint and check any server hook that replaces its response.")));
    }
    FString FailureReason;
    if (!ParseRecordObject(Object.ToSharedRef(), Record, &FailureReason))
    {
        return TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase returned a record the SDK could not parse. ") + FailureReason));
    }
    return TOpenPocketBaseResult<FOpenPocketBaseRecord>::Success(MoveTemp(Record));
}

TOpenPocketBaseResult<FOpenPocketBaseRecordPage> ParseRecordPageResponse(
    const FOpenPocketBaseHttpResponse& Response)
{
    if (TOptional<TOpenPocketBaseResult<FOpenPocketBaseRecordPage>> Failure =
            ValidateResponse<FOpenPocketBaseRecordPage>(Response))
    {
        return MoveTemp(Failure.GetValue());
    }

    TSharedPtr<FJsonObject> Object;
    if (!ParseObject(Response.Body, Object))
    {
        return TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase returned a successful list response that is not a JSON object. Confirm the route is a standard records list endpoint and check any server hook that replaces its response.")));
    }

    FOpenPocketBaseRecordPage Page;
    double NumericPage = 0;
    double NumericPerPage = 0;
    if (!Object->TryGetNumberField(TEXT("page"), NumericPage) ||
        !Object->TryGetNumberField(TEXT("perPage"), NumericPerPage) ||
        !FMath::IsFinite(NumericPage) || !FMath::IsFinite(NumericPerPage) ||
        NumericPage != FMath::RoundToDouble(NumericPage) ||
        NumericPerPage != FMath::RoundToDouble(NumericPerPage) ||
        NumericPage < 1 || NumericPage > MAX_int32 ||
        NumericPerPage < 1 || NumericPerPage > MAX_int32)
    {
        return TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase record page must contain integer 'page' and 'perPage' values greater than zero. Check any custom list-response hook and PocketBase version compatibility.")));
    }
    Page.Page = static_cast<int32>(NumericPage);
    Page.PerPage = static_cast<int32>(NumericPerPage);

    double TotalItems = -1;
    if (Object->HasField(TEXT("totalItems")) &&
        (!Object->TryGetNumberField(TEXT("totalItems"), TotalItems) ||
         !FMath::IsFinite(TotalItems) ||
         TotalItems != FMath::RoundToDouble(TotalItems) || TotalItems < -1 ||
         TotalItems > 9007199254740991.0))
    {
        return TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase record page 'totalItems' must be -1 when totals were skipped, or a non-negative safe integer. Check any custom list-response hook.")));
    }
    if (TotalItems >= 0)
    {
        Page.bHasTotalItems = true;
        Page.TotalItems = static_cast<int64>(TotalItems);
    }

    double TotalPages = -1;
    if (Object->HasField(TEXT("totalPages")) &&
        (!Object->TryGetNumberField(TEXT("totalPages"), TotalPages) ||
         !FMath::IsFinite(TotalPages) ||
         TotalPages != FMath::RoundToDouble(TotalPages) ||
         TotalPages < -1 || TotalPages > MAX_int32))
    {
        return TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase record page 'totalPages' must be -1 when totals were skipped, or a non-negative 32-bit integer. Check any custom list-response hook.")));
    }
    if (TotalPages >= 0)
    {
        Page.bHasTotalPages = true;
        Page.TotalPages = static_cast<int32>(TotalPages);
    }

    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Object->TryGetArrayField(TEXT("items"), Items) || Items == nullptr)
    {
        return TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase record page is missing the 'items' array. Check any custom list-response hook and PocketBase version compatibility.")));
    }

    Page.Items.Reserve(Items->Num());
    for (int32 ItemIndex = 0; ItemIndex < Items->Num(); ++ItemIndex)
    {
        const TSharedPtr<FJsonValue>& Item = (*Items)[ItemIndex];
        const TSharedPtr<FJsonObject>* ItemObject = nullptr;
        FOpenPocketBaseRecord Record;
        if (!Item.IsValid() || !Item->TryGetObject(ItemObject) || ItemObject == nullptr)
        {
            return TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Failure(
                MakeSerializationError(
                    Response,
                    FString::Printf(
                        TEXT("PocketBase record page item %d is not a JSON object. Check any custom list-response hook."),
                        ItemIndex)));
        }
        FString FailureReason;
        if (!ParseRecordObject(ItemObject->ToSharedRef(), Record, &FailureReason))
        {
            return TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Failure(
                MakeSerializationError(
                    Response,
                    FString::Printf(
                        TEXT("PocketBase record page item %d could not be parsed. %s"),
                        ItemIndex,
                        *FailureReason)));
        }
        Page.Items.Add(MoveTemp(Record));
    }

    return TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Success(MoveTemp(Page));
}

TOpenPocketBaseResult<FOpenPocketBaseAuthResult> ParseAuthResponse(
    const FOpenPocketBaseHttpResponse& Response,
    FString& OutToken)
{
    if (TOptional<TOpenPocketBaseResult<FOpenPocketBaseAuthResult>> Failure =
            ValidateResponse<FOpenPocketBaseAuthResult>(Response))
    {
        return MoveTemp(Failure.GetValue());
    }

    TSharedPtr<FJsonObject> Object;
    const TSharedPtr<FJsonObject>* RecordObject = nullptr;
    if (!ParseObject(Response.Body, Object) || !Object->TryGetStringField(TEXT("token"), OutToken) ||
        !Object->TryGetObjectField(TEXT("record"), RecordObject) || RecordObject == nullptr)
    {
        return TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase authentication response must contain a string 'token' and an object 'record'. Confirm the auth collection and check any server hook that replaces the response.")));
    }

    FOpenPocketBaseAuthResult Result;
    if (!ParseRecordObject(RecordObject->ToSharedRef(), Result.Record))
    {
        return TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase authentication succeeded, but its 'record' object is missing a valid ID or contains an invalid date. Check any auth-response hook.")));
    }
    return TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Success(MoveTemp(Result));
}

TOpenPocketBaseResult<FOpenPocketBaseAuthMethods> ParseAuthMethodsResponse(
    const FOpenPocketBaseHttpResponse& Response)
{
    if (TOptional<TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>> Failure =
            ValidateResponse<FOpenPocketBaseAuthMethods>(Response))
    {
        return MoveTemp(Failure.GetValue());
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedPtr<FJsonObject>* Mfa = nullptr;
    const TSharedPtr<FJsonObject>* Otp = nullptr;
    const TSharedPtr<FJsonObject>* Password = nullptr;
    const TSharedPtr<FJsonObject>* OAuth2 = nullptr;
    if (!ParseObject(Response.Body, Root) ||
        !Root->TryGetObjectField(TEXT("mfa"), Mfa) || Mfa == nullptr ||
        !Root->TryGetObjectField(TEXT("otp"), Otp) || Otp == nullptr ||
        !Root->TryGetObjectField(TEXT("password"), Password) || Password == nullptr ||
        !Root->TryGetObjectField(TEXT("oauth2"), OAuth2) || OAuth2 == nullptr)
    {
        return TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase auth-methods response must contain 'mfa', 'otp', 'password', and 'oauth2' objects. Confirm the auth collection and PocketBase version.")));
    }

    FOpenPocketBaseAuthMethods Result;
    double MfaDuration = 0;
    double OtpDuration = 0;
    const TArray<TSharedPtr<FJsonValue>>* IdentityFields = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Providers = nullptr;
    if (!(*Mfa)->TryGetBoolField(TEXT("enabled"), Result.Mfa.bEnabled) ||
        !(*Mfa)->TryGetNumberField(TEXT("duration"), MfaDuration) ||
        !(*Otp)->TryGetBoolField(TEXT("enabled"), Result.Otp.bEnabled) ||
        !(*Otp)->TryGetNumberField(TEXT("duration"), OtpDuration) ||
        !(*Password)->TryGetBoolField(TEXT("enabled"), Result.Password.bEnabled) ||
        !(*Password)->TryGetArrayField(TEXT("identityFields"), IdentityFields) ||
        IdentityFields == nullptr || IdentityFields->Num() > 32 ||
        !(*OAuth2)->TryGetBoolField(TEXT("enabled"), Result.OAuth2.bEnabled) ||
        !(*OAuth2)->TryGetArrayField(TEXT("providers"), Providers) || Providers == nullptr ||
        Providers->Num() > 64 || MfaDuration < 0 || MfaDuration > MAX_int32 ||
        OtpDuration < 0 || OtpDuration > MAX_int32)
    {
        return TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase auth-methods response contains missing, incorrectly typed, or oversized method settings. Confirm the auth collection configuration and PocketBase version.")));
    }
    Result.Mfa.DurationSeconds = static_cast<int32>(MfaDuration);
    Result.Otp.DurationSeconds = static_cast<int32>(OtpDuration);

    for (const TSharedPtr<FJsonValue>& Value : *IdentityFields)
    {
        FString Field;
        if (!Value.IsValid() || !Value->TryGetString(Field) || Field.IsEmpty() || Field.Len() > 128)
        {
            return TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>::Failure(
                MakeSerializationError(
                    Response,
                    TEXT("PocketBase auth-methods response contains an empty, non-string, or oversized password identity field. Check the auth collection's identityFields setting.")));
        }
        Result.Password.IdentityFields.Add(MoveTemp(Field));
    }

    for (const TSharedPtr<FJsonValue>& Value : *Providers)
    {
        const TSharedPtr<FJsonObject>* ProviderObject = nullptr;
        FOpenPocketBaseOAuthProvider Provider;
        if (!Value.IsValid() || !Value->TryGetObject(ProviderObject) || ProviderObject == nullptr ||
            !(*ProviderObject)->TryGetStringField(TEXT("name"), Provider.Name) ||
            !(*ProviderObject)->TryGetStringField(TEXT("displayName"), Provider.DisplayName) ||
            !(*ProviderObject)->TryGetStringField(TEXT("state"), Provider.State) ||
            !(*ProviderObject)->TryGetStringField(TEXT("authURL"), Provider.AuthUrl) ||
            !(*ProviderObject)->TryGetStringField(TEXT("codeVerifier"), Provider.CodeVerifier) ||
            !(*ProviderObject)->TryGetStringField(TEXT("codeChallenge"), Provider.CodeChallenge) ||
            !(*ProviderObject)->TryGetStringField(
                TEXT("codeChallengeMethod"), Provider.CodeChallengeMethod) ||
            Provider.Name.IsEmpty() || Provider.Name.Len() > 128 ||
            Provider.DisplayName.Len() > 256 || Provider.State.Len() > 512 ||
            Provider.AuthUrl.IsEmpty() || Provider.AuthUrl.Len() > 8192 ||
            Provider.CodeVerifier.IsEmpty() || Provider.CodeVerifier.Len() > 1024 ||
            Provider.CodeChallenge.IsEmpty() || Provider.CodeChallenge.Len() > 1024 ||
            Provider.CodeChallengeMethod.Len() > 32)
        {
            return TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>::Failure(
                MakeSerializationError(
                    Response,
                    TEXT("PocketBase auth-methods response contains an incomplete or oversized OAuth provider entry. Check the provider configuration and PocketBase version.")));
        }
        Result.OAuth2.Providers.Add(MoveTemp(Provider));
    }

    return TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>::Success(MoveTemp(Result));
}

TOpenPocketBaseResult<FOpenPocketBaseOtpRequest> ParseOtpResponse(
    const FOpenPocketBaseHttpResponse& Response)
{
    if (TOptional<TOpenPocketBaseResult<FOpenPocketBaseOtpRequest>> Failure =
            ValidateResponse<FOpenPocketBaseOtpRequest>(Response))
    {
        return MoveTemp(Failure.GetValue());
    }

    TSharedPtr<FJsonObject> Object;
    FOpenPocketBaseOtpRequest Result;
    if (!ParseObject(Response.Body, Object) ||
        !Object->TryGetStringField(TEXT("otpId"), Result.OtpId) ||
        Result.OtpId.IsEmpty() || Result.OtpId.Len() > 256)
    {
        return TOpenPocketBaseResult<FOpenPocketBaseOtpRequest>::Failure(
            MakeSerializationError(
                Response,
                TEXT("PocketBase OTP response must contain a non-empty string 'otpId' no longer than 256 characters. Check any auth-response hook and PocketBase version.")));
    }
    return TOpenPocketBaseResult<FOpenPocketBaseOtpRequest>::Success(MoveTemp(Result));
}

TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt> ParseAuthAttemptResponse(
    const FOpenPocketBaseHttpResponse& Response,
    FString& OutToken)
{
    OutToken.Reset();
    if (Response.bTransportSucceeded && Response.HttpStatus == 401)
    {
        TSharedPtr<FJsonObject> Object;
        FOpenPocketBaseAuthAttempt Challenge;
        Challenge.Status = EOpenPocketBaseAuthAttemptStatus::MfaRequired;
        if (ParseObject(Response.Body, Object) &&
            Object->TryGetStringField(TEXT("mfaId"), Challenge.Mfa.Id) &&
            !Challenge.Mfa.Id.IsEmpty() && Challenge.Mfa.Id.Len() <= 256)
        {
            return TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Success(
                MoveTemp(Challenge));
        }
    }

    TOpenPocketBaseResult<FOpenPocketBaseAuthResult> Authentication =
        ParseAuthResponse(Response, OutToken);
    if (!Authentication.IsSuccess())
    {
        return TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Failure(
            Authentication.GetError());
    }

    FOpenPocketBaseAuthAttempt Result;
    Result.Status = EOpenPocketBaseAuthAttemptStatus::Authenticated;
    Result.Authentication = Authentication.TakeValue();
    return TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>::Success(MoveTemp(Result));
}

TOpenPocketBaseResult<bool> ParseEmptyResponse(const FOpenPocketBaseHttpResponse& Response)
{
    if (TOptional<TOpenPocketBaseResult<bool>> Failure = ValidateResponse<bool>(Response))
    {
        return MoveTemp(Failure.GetValue());
    }
    return TOpenPocketBaseResult<bool>::Success(true);
}

TOpenPocketBaseResult<FOpenPocketBaseBatchResult> ParseBatchResponse(
    const FOpenPocketBaseHttpResponse& Response,
    const FOpenPocketBaseBatchRequest& Request)
{
    if (!Response.bTransportSucceeded)
    {
        return TOpenPocketBaseResult<FOpenPocketBaseBatchResult>::Failure(MakeTransportError(Response));
    }
    if (!IsSuccessStatus(Response.HttpStatus))
    {
        return TOpenPocketBaseResult<FOpenPocketBaseBatchResult>::Failure(MakeBatchHttpError(Response));
    }

    TArray<TSharedPtr<FJsonValue>> Items;
    if (!ParseArray(Response.Body, Items) || Items.Num() != Request.Entries.Num())
    {
        return TOpenPocketBaseResult<FOpenPocketBaseBatchResult>::Failure(
            MakeSerializationError(
                Response,
                FString::Printf(
                    TEXT("PocketBase batch response must be a JSON array with %d result(s), one for each submitted operation."),
                    Request.Entries.Num())));
    }

    FOpenPocketBaseBatchResult Result;
    Result.Results.Reserve(Items.Num());
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* ItemObject = nullptr;
        double Status = 0;
        if (!Items[Index].IsValid() || !Items[Index]->TryGetObject(ItemObject) || ItemObject == nullptr ||
            !(*ItemObject)->TryGetNumberField(TEXT("status"), Status) ||
            Status < 200 || Status >= 300)
        {
            return TOpenPocketBaseResult<FOpenPocketBaseBatchResult>::Failure(
                MakeSerializationError(
                    Response,
                    FString::Printf(
                        TEXT("PocketBase batch result %d must be an object with a successful numeric 'status'. Check the server's batch response format."),
                        Index)));
        }

        FOpenPocketBaseBatchOperationResult OperationResult;
        OperationResult.Operation = Request.Entries[Index].Operation;
        OperationResult.HttpStatus = static_cast<int32>(Status);
        const TSharedPtr<FJsonValue>* BodyValue = (*ItemObject)->Values.Find(TEXT("body"));
        if (OperationResult.Operation != EOpenPocketBaseBatchOperation::Delete)
        {
            const TSharedPtr<FJsonObject>* RecordObject = nullptr;
            if (BodyValue == nullptr || !BodyValue->IsValid() ||
                !(*BodyValue)->TryGetObject(RecordObject) || RecordObject == nullptr ||
                !ParseRecordObject(RecordObject->ToSharedRef(), OperationResult.Record))
            {
                return TOpenPocketBaseResult<FOpenPocketBaseBatchResult>::Failure(
                    MakeSerializationError(
                        Response,
                        FString::Printf(
                            TEXT("PocketBase batch result %d for a create or update operation must contain a valid record object in 'body'."),
                            Index)));
            }
            OperationResult.bHasRecord = true;
        }
        else if (BodyValue != nullptr && BodyValue->IsValid() && !(*BodyValue)->IsNull())
        {
            return TOpenPocketBaseResult<FOpenPocketBaseBatchResult>::Failure(
                MakeSerializationError(
                    Response,
                    FString::Printf(
                        TEXT("PocketBase batch result %d is a delete operation, but its 'body' is not null. Check the server's batch response format."),
                        Index)));
        }
        Result.Results.Add(MoveTemp(OperationResult));
    }
    return TOpenPocketBaseResult<FOpenPocketBaseBatchResult>::Success(MoveTemp(Result));
}

TArray<uint8> SerializeObject(const TSharedRef<FJsonObject>& Object)
{
    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Object, Writer);

    FTCHARToUTF8 Converted(*Json);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}
}
