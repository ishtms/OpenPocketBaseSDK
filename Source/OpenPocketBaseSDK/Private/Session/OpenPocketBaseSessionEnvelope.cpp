#include "Session/OpenPocketBaseSessionEnvelope.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/OpenPocketBaseJson.h"

namespace
{
bool IsSafeCollection(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 255 || Value == TEXT(".") || Value == TEXT(".."))
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (Character == TEXT('/') || Character == TEXT('\\') || Character == TEXT('%') ||
            Character == TEXT('?') || Character == TEXT('#') || FChar::IsControl(Character))
        {
            return false;
        }
    }
    return true;
}

bool IsJwtSegment(const FString& Segment)
{
    if (Segment.IsEmpty())
    {
        return false;
    }
    for (const TCHAR Character : Segment)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_'))
        {
            return false;
        }
    }
    return true;
}

bool IsTokenShapeValid(const FString& Token)
{
    TArray<FString> Segments;
    Token.ParseIntoArray(Segments, TEXT("."), false);
    return Token.Len() <= 8192 && Segments.Num() == 3 &&
        IsJwtSegment(Segments[0]) && IsJwtSegment(Segments[1]) && IsJwtSegment(Segments[2]);
}

FOpenPocketBaseError MakeEnvelopeError(const TCHAR* Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::SecureStorage;
    Error.ServerMessage = Message;
    return Error;
}
}

namespace OpenPocketBase::SessionEnvelope
{
bool Serialize(
    const FString& Origin,
    const FString& Profile,
    const FString& AuthCollection,
    const FString& Token,
    const FOpenPocketBaseRecord& Record,
    TArray<uint8>& OutBytes,
    FOpenPocketBaseError& OutError)
{
    OutBytes.Reset();
    if (!IsSafeCollection(AuthCollection) || !IsTokenShapeValid(Token) ||
        !Record.Data.JsonObject.IsValid())
    {
        OutError = MakeEnvelopeError(TEXT("The authenticated session cannot be serialized safely."));
        return false;
    }

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("schema"), SchemaVersion);
    Root->SetStringField(TEXT("origin"), Origin);
    Root->SetStringField(TEXT("profile"), Profile);
    Root->SetStringField(TEXT("authCollection"), AuthCollection);
    Root->SetStringField(TEXT("token"), Token);
    Root->SetObjectField(TEXT("record"), Record.Data.JsonObject);
    OutBytes = OpenPocketBase::Json::SerializeObject(Root);
    if (OutBytes.IsEmpty() || OutBytes.Num() > MaxEnvelopeBytes)
    {
        OutBytes.Reset();
        OutError = MakeEnvelopeError(TEXT("The secure session envelope exceeds its size bound."));
        return false;
    }

    OutError = FOpenPocketBaseError();
    return true;
}

EReadResult Deserialize(
    const TConstArrayView<uint8> Bytes,
    const FString& ExpectedOrigin,
    const FString& ExpectedProfile,
    FString& OutAuthCollection,
    FString& OutToken,
    FOpenPocketBaseRecord& OutRecord)
{
    OutAuthCollection.Reset();
    OutToken.Reset();
    OutRecord = FOpenPocketBaseRecord();
    if (Bytes.IsEmpty() || Bytes.Num() > MaxEnvelopeBytes)
    {
        return EReadResult::Corrupt;
    }

    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),
        Bytes.Num());
    const FString Json(Converted.Length(), Converted.Get());
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    double Schema = 0;
    FString Origin;
    FString Profile;
    const TSharedPtr<FJsonObject>* RecordObject = nullptr;
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
        !Root->TryGetNumberField(TEXT("schema"), Schema) || Schema != SchemaVersion ||
        !Root->TryGetStringField(TEXT("origin"), Origin) ||
        !Root->TryGetStringField(TEXT("profile"), Profile) ||
        !Root->TryGetStringField(TEXT("authCollection"), OutAuthCollection) ||
        !Root->TryGetStringField(TEXT("token"), OutToken) ||
        !Root->TryGetObjectField(TEXT("record"), RecordObject) || RecordObject == nullptr ||
        !IsSafeCollection(OutAuthCollection) || !IsTokenShapeValid(OutToken) ||
        !OpenPocketBase::Json::TryParseRecordObject(RecordObject->ToSharedRef(), OutRecord))
    {
        return EReadResult::Corrupt;
    }

    if (Origin != ExpectedOrigin || Profile != ExpectedProfile)
    {
        return EReadResult::PolicyRejected;
    }
    return EReadResult::Valid;
}
}
