#include "Serialization/OpenPocketBaseJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
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

FDateTime ParsePocketBaseDate(const FString& Value)
{
    FString IsoValue = Value;
    if (IsoValue.Len() > 10 && IsoValue[10] == TEXT(' '))
    {
        IsoValue[10] = TEXT('T');
    }

    FDateTime Result;
    FDateTime::ParseIso8601(*IsoValue, Result);
    return Result;
}

bool ParseRecordObject(const TSharedRef<FJsonObject>& Object, FOpenPocketBaseRecord& OutRecord)
{
    if (!Object->TryGetStringField(TEXT("id"), OutRecord.Id))
    {
        return false;
    }

    Object->TryGetStringField(TEXT("collectionId"), OutRecord.CollectionId);
    Object->TryGetStringField(TEXT("collectionName"), OutRecord.CollectionName);

    FString DateValue;
    if (Object->TryGetStringField(TEXT("created"), DateValue))
    {
        OutRecord.Created = ParsePocketBaseDate(DateValue);
    }
    if (Object->TryGetStringField(TEXT("updated"), DateValue))
    {
        OutRecord.Updated = ParsePocketBaseDate(DateValue);
    }

    OutRecord.Data = WrapObject(Object);
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
