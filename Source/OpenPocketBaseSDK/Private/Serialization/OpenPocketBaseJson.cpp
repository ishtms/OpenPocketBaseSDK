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
    Error.ServerMessage = Response.ErrorMessage.IsEmpty()
        ? TEXT("The PocketBase request did not complete.")
        : Response.ErrorMessage;
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
    Error.ServerMessage = TEXT("PocketBase returned an HTTP error.");
    Error.bMayRetry = Response.HttpStatus >= 500 && Response.HttpStatus < 600;
    Error.RequestId = Response.RequestId;

    TSharedPtr<FJsonObject> Object;
    if (!ParseObject(Response.Body, Object))
    {
        return Error;
    }

    Error.Kind = EOpenPocketBaseErrorKind::PocketBase;
    Object->TryGetStringField(TEXT("message"), Error.ServerMessage);
    Object->TryGetStringField(TEXT("code"), Error.ServerCode);

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
        Error.ServerMessage.Contains(TEXT("Batch requests are not allowed"), ESearchCase::IgnoreCase))
    {
        Error.Kind = EOpenPocketBaseErrorKind::Unsupported;
        Error.ServerCode = TEXT("batch_disabled");
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

        if (Error.ServerCode.IsEmpty())
        {
            (*RequestFailure)->TryGetStringField(TEXT("code"), Error.ServerCode);
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
    const TCHAR* Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::Serialization;
    Error.HttpStatus = Response.HttpStatus;
    Error.ServerMessage = Message;
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

bool ParseRecordObject(const TSharedRef<FJsonObject>& Object, FOpenPocketBaseRecord& OutRecord)
{
    OutRecord = FOpenPocketBaseRecord();
    if (!Object->TryGetStringField(TEXT("id"), OutRecord.Id))
    {
        return false;
    }

    Object->TryGetStringField(TEXT("collectionId"), OutRecord.CollectionId);
    Object->TryGetStringField(TEXT("collectionName"), OutRecord.CollectionName);

    FString DateValue;
    if (Object->TryGetStringField(TEXT("created"), DateValue) &&
        !OpenPocketBase::Date::TryParse(DateValue, OutRecord.Created))
    {
        return false;
    }
    if (Object->TryGetStringField(TEXT("updated"), DateValue) &&
        !OpenPocketBase::Date::TryParse(DateValue, OutRecord.Updated))
    {
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
    if (!ParseObject(Response.Body, Object) || !ParseRecordObject(Object.ToSharedRef(), Record))
    {
        return TOpenPocketBaseResult<FOpenPocketBaseRecord>::Failure(
            MakeSerializationError(Response, TEXT("PocketBase returned an invalid record.")));
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
            MakeSerializationError(Response, TEXT("PocketBase returned an invalid record page.")));
    }

    FOpenPocketBaseRecordPage Page;
    Page.Page = Object->GetIntegerField(TEXT("page"));
    Page.PerPage = Object->GetIntegerField(TEXT("perPage"));

    double TotalItems = -1;
    if (Object->TryGetNumberField(TEXT("totalItems"), TotalItems) && TotalItems >= 0)
    {
        Page.bHasTotalItems = true;
        Page.TotalItems = static_cast<int64>(TotalItems);
    }

    double TotalPages = -1;
    if (Object->TryGetNumberField(TEXT("totalPages"), TotalPages) && TotalPages >= 0)
    {
        Page.bHasTotalPages = true;
        Page.TotalPages = static_cast<int32>(TotalPages);
    }

    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Object->TryGetArrayField(TEXT("items"), Items) || Items == nullptr)
    {
        return TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Failure(
            MakeSerializationError(Response, TEXT("PocketBase record page is missing items.")));
    }

    Page.Items.Reserve(Items->Num());
    for (const TSharedPtr<FJsonValue>& Item : *Items)
    {
        const TSharedPtr<FJsonObject>* ItemObject = nullptr;
        FOpenPocketBaseRecord Record;
        if (!Item.IsValid() || !Item->TryGetObject(ItemObject) || ItemObject == nullptr ||
            !ParseRecordObject(ItemObject->ToSharedRef(), Record))
        {
            return TOpenPocketBaseResult<FOpenPocketBaseRecordPage>::Failure(
                MakeSerializationError(Response, TEXT("PocketBase record page contains an invalid item.")));
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
            MakeSerializationError(Response, TEXT("PocketBase returned an invalid authentication response.")));
    }

    FOpenPocketBaseAuthResult Result;
    if (!ParseRecordObject(RecordObject->ToSharedRef(), Result.Record))
    {
        return TOpenPocketBaseResult<FOpenPocketBaseAuthResult>::Failure(
            MakeSerializationError(Response, TEXT("PocketBase returned an invalid authenticated record.")));
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
            MakeSerializationError(Response, TEXT("PocketBase returned invalid auth methods.")));
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
            MakeSerializationError(Response, TEXT("PocketBase returned unbounded auth methods.")));
    }
    Result.Mfa.DurationSeconds = static_cast<int32>(MfaDuration);
    Result.Otp.DurationSeconds = static_cast<int32>(OtpDuration);

    for (const TSharedPtr<FJsonValue>& Value : *IdentityFields)
    {
        FString Field;
        if (!Value.IsValid() || !Value->TryGetString(Field) || Field.IsEmpty() || Field.Len() > 128)
        {
            return TOpenPocketBaseResult<FOpenPocketBaseAuthMethods>::Failure(
                MakeSerializationError(Response, TEXT("PocketBase returned an invalid auth identity field.")));
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
                MakeSerializationError(Response, TEXT("PocketBase returned an invalid OAuth provider.")));
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
            MakeSerializationError(Response, TEXT("PocketBase returned an invalid OTP request ID.")));
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
            MakeSerializationError(Response, TEXT("PocketBase returned an invalid batch result.")));
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
                MakeSerializationError(Response, TEXT("PocketBase returned an invalid batch operation result.")));
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
                    MakeSerializationError(Response, TEXT("PocketBase batch result contains an invalid record.")));
            }
            OperationResult.bHasRecord = true;
        }
        else if (BodyValue != nullptr && BodyValue->IsValid() && !(*BodyValue)->IsNull())
        {
            return TOpenPocketBaseResult<FOpenPocketBaseBatchResult>::Failure(
                MakeSerializationError(Response, TEXT("PocketBase batch delete returned an unexpected body.")));
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
