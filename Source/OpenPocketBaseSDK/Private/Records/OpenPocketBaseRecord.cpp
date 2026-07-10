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

FOpenPocketBaseRecordBody::FOpenPocketBaseRecordBody(const FOpenPocketBaseRecordBody& Other)
{
    *this = Other;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::operator=(
    const FOpenPocketBaseRecordBody& Other)
{
    if (this == &Other)
    {
        return *this;
    }

    Data.JsonString = Other.Data.JsonString;
    if (Other.Data.JsonObject.IsValid())
    {
        Data.JsonObject = MakeShared<FJsonObject>();
        FJsonObject::Duplicate(Other.Data.JsonObject, Data.JsonObject);
    }
    else
    {
        Data.JsonObject.Reset();
    }
    return *this;
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

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetStringField(
    const FString& FieldName,
    const FString& Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    GetOrCreateBodyObject(*this)->SetStringField(MakeModifiedFieldName(FieldName, Modifier), Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetNumberField(
    const FString& FieldName,
    const double Value,
    const EOpenPocketBaseFieldModifier Modifier)
{
    GetOrCreateBodyObject(*this)->SetNumberField(MakeModifiedFieldName(FieldName, Modifier), Value);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetBooleanField(
    const FString& FieldName,
    const bool bValue,
    const EOpenPocketBaseFieldModifier Modifier)
{
    GetOrCreateBodyObject(*this)->SetBoolField(MakeModifiedFieldName(FieldName, Modifier), bValue);
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetNullField(
    const FString& FieldName,
    const EOpenPocketBaseFieldModifier Modifier)
{
    GetOrCreateBodyObject(*this)->SetField(
        MakeModifiedFieldName(FieldName, Modifier),
        MakeShared<FJsonValueNull>());
    return *this;
}

FOpenPocketBaseRecordBody& FOpenPocketBaseRecordBody::SetStringArrayField(
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
    return *this;
}

FOpenPocketBaseRecordOptions& FOpenPocketBaseRecordOptions::WithExpand(TArray<FString> InExpand)
{
    Expand = MoveTemp(InExpand);
    return *this;
}

FOpenPocketBaseRecordOptions& FOpenPocketBaseRecordOptions::WithFields(TArray<FString> InFields)
{
    Fields = MoveTemp(InFields);
    return *this;
}

FOpenPocketBaseRecordOptions& FOpenPocketBaseRecordOptions::WithRequestOptions(
    FOpenPocketBaseRequestOptions InOptions)
{
    RequestOptions = MoveTemp(InOptions);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::AtPage(const int32 InPage)
{
    Page = FMath::Max(1, InPage);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::PageSize(const int32 InPerPage)
{
    PerPage = FMath::Max(1, InPerPage);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::Where(FOpenPocketBaseFilter InFilter)
{
    Filter = MoveTemp(InFilter);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::WithSort(TArray<FString> InSort)
{
    Sort = MoveTemp(InSort);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::WithExpand(TArray<FString> InExpand)
{
    Expand = MoveTemp(InExpand);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::WithFields(TArray<FString> InFields)
{
    Fields = MoveTemp(InFields);
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::SkipTotals(const bool bInSkipTotal)
{
    bSkipTotal = bInSkipTotal;
    return *this;
}

FOpenPocketBaseListOptions& FOpenPocketBaseListOptions::WithRequestOptions(
    FOpenPocketBaseRequestOptions InOptions)
{
    RequestOptions = MoveTemp(InOptions);
    return *this;
}

FOpenPocketBaseFullListOptions& FOpenPocketBaseFullListOptions::WithListOptions(
    FOpenPocketBaseListOptions InOptions)
{
    ListOptions = MoveTemp(InOptions);
    return *this;
}

FOpenPocketBaseFullListOptions& FOpenPocketBaseFullListOptions::LimitItems(const int32 InMaxItems)
{
    MaxItems = FMath::Max(0, InMaxItems);
    return *this;
}

FOpenPocketBaseFullListOptions& FOpenPocketBaseFullListOptions::LimitPages(const int32 InMaxPages)
{
    MaxPages = FMath::Max(0, InMaxPages);
    return *this;
}
