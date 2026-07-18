#include "OpenPocketBaseBatch.h"

namespace
{
template <typename ValueType>
FString JoinQueryValues(const TArray<ValueType>& Values)
{
    TArray<FString> QueryValues;
    QueryValues.Reserve(Values.Num());
    for (const ValueType& Value : Values)
    {
        QueryValues.Add(Value.ToQueryValue());
    }
    return FString::Join(QueryValues, TEXT(","));
}
}

FString FOpenPocketBaseBatchEntry::GetCollectionName() const
{
    return Collection.IsSet() ? Collection.Name : DynamicCollection;
}

FString FOpenPocketBaseBatchEntry::GetExpandQuery() const
{
    return Collection.IsSet()
        ? JoinQueryValues(ResponseOptions.Expand)
        : FString::Join(DynamicExpand, TEXT(","));
}

FString FOpenPocketBaseBatchEntry::GetFieldsQuery() const
{
    return Collection.IsSet()
        ? JoinQueryValues(ResponseOptions.Fields)
        : FString::Join(DynamicFields, TEXT(","));
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddCreate(
    FOpenPocketBaseWritableCollectionRef Collection,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Create;
    Entry.Collection = MoveTemp(Collection);
    Entry.Body = MoveTemp(Body);
    Entry.ResponseOptions = MoveTemp(ResponseOptions);
    Entries.Add(MoveTemp(Entry));
    return *this;
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddUpdate(
    FOpenPocketBaseWritableCollectionRef Collection,
    FString RecordId,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Update;
    Entry.Collection = MoveTemp(Collection);
    Entry.RecordId = MoveTemp(RecordId);
    Entry.Body = MoveTemp(Body);
    Entry.ResponseOptions = MoveTemp(ResponseOptions);
    Entries.Add(MoveTemp(Entry));
    return *this;
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddUpsert(
    FOpenPocketBaseWritableCollectionRef Collection,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Upsert;
    Entry.Collection = MoveTemp(Collection);
    Entry.Body = MoveTemp(Body);
    Entry.ResponseOptions = MoveTemp(ResponseOptions);
    Entries.Add(MoveTemp(Entry));
    return *this;
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddDelete(
    FOpenPocketBaseWritableCollectionRef Collection,
    FString RecordId)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Delete;
    Entry.Collection = MoveTemp(Collection);
    Entry.RecordId = MoveTemp(RecordId);
    Entries.Add(MoveTemp(Entry));
    return *this;
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddDynamicCreate(
    FString Collection,
    FOpenPocketBaseRecordBody Body,
    TArray<FString> Expand,
    TArray<FString> Fields)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Create;
    Entry.DynamicCollection = MoveTemp(Collection);
    Entry.Body = MoveTemp(Body);
    Entry.DynamicExpand = MoveTemp(Expand);
    Entry.DynamicFields = MoveTemp(Fields);
    Entries.Add(MoveTemp(Entry));
    return *this;
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddDynamicUpdate(
    FString Collection,
    FString RecordId,
    FOpenPocketBaseRecordBody Body,
    TArray<FString> Expand,
    TArray<FString> Fields)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Update;
    Entry.DynamicCollection = MoveTemp(Collection);
    Entry.RecordId = MoveTemp(RecordId);
    Entry.Body = MoveTemp(Body);
    Entry.DynamicExpand = MoveTemp(Expand);
    Entry.DynamicFields = MoveTemp(Fields);
    Entries.Add(MoveTemp(Entry));
    return *this;
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddDynamicUpsert(
    FString Collection,
    FOpenPocketBaseRecordBody Body,
    TArray<FString> Expand,
    TArray<FString> Fields)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Upsert;
    Entry.DynamicCollection = MoveTemp(Collection);
    Entry.Body = MoveTemp(Body);
    Entry.DynamicExpand = MoveTemp(Expand);
    Entry.DynamicFields = MoveTemp(Fields);
    Entries.Add(MoveTemp(Entry));
    return *this;
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddDynamicDelete(
    FString Collection,
    FString RecordId)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Delete;
    Entry.DynamicCollection = MoveTemp(Collection);
    Entry.RecordId = MoveTemp(RecordId);
    Entries.Add(MoveTemp(Entry));
    return *this;
}
