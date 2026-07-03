#include "OpenPocketBaseRecord.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
TSharedRef<FJsonObject> GetOrCreateBodyObject(FOpenPocketBaseRecordBody& Body)
{
    if (!Body.Data.JsonObject.IsValid())
    {
        Body.Data.JsonObject = MakeShared<FJsonObject>();
    }
    Body.Data.JsonString.Reset();
    return Body.Data.JsonObject.ToSharedRef();
}
}

FOpenPocketBaseRecordBody::FOpenPocketBaseRecordBody()
{
    Data.JsonObject = MakeShared<FJsonObject>();
}

FString FOpenPocketBaseRecordBody::MakeModifiedFieldName(
    const FString& FieldName,
    const EOpenPocketBaseFieldModifier Modifier)
{
    switch (Modifier)
    {
    case EOpenPocketBaseFieldModifier::Append:
        return FieldName + TEXT("+");
    case EOpenPocketBaseFieldModifier::Prepend:
        return TEXT("+") + FieldName;
    case EOpenPocketBaseFieldModifier::Remove:
        return FieldName + TEXT("-");
    default:
        return FieldName;
    }
}

void FOpenPocketBaseRecordBody::SetStringField(
    const FString& FieldName,
    const FString& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    GetOrCreateBodyObject(*this)->SetStringField(MakeModifiedFieldName(FieldName, Modifier), Value);
}

void FOpenPocketBaseRecordBody::SetNumberField(
    const FString& FieldName,
    const double Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    GetOrCreateBodyObject(*this)->SetNumberField(MakeModifiedFieldName(FieldName, Modifier), Value);
}

void FOpenPocketBaseRecordBody::SetBooleanField(
    const FString& FieldName,
    const bool bValue,
    const EOpenPocketBaseFieldModifier Modifier)
{
    GetOrCreateBodyObject(*this)->SetBoolField(MakeModifiedFieldName(FieldName, Modifier), bValue);
}

void FOpenPocketBaseRecordBody::SetNullField(
    const FString& FieldName,
    const EOpenPocketBaseFieldModifier Modifier)
{
    GetOrCreateBodyObject(*this)->SetField(
        MakeModifiedFieldName(FieldName, Modifier),
        MakeShared<FJsonValueNull>());
}

void FOpenPocketBaseRecordBody::SetStringArrayField(
    const FString& FieldName,
    const TArray<FString>& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    JsonValues.Reserve(Value.Num());
    for (const FString& Item : Value)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Item));
    }
    GetOrCreateBodyObject(*this)->SetArrayField(
        MakeModifiedFieldName(FieldName, Modifier),
        MoveTemp(JsonValues));
}
