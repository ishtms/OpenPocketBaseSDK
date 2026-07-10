#include "OpenPocketBaseBatch.h"

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddCreate(
    FString Collection,
    FOpenPocketBaseRecordBody Body,
    TArray<FString> Expand,
    TArray<FString> Fields)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Create;
    Entry.Collection = MoveTemp(Collection);
    Entry.Body = MoveTemp(Body);
    Entry.Expand = MoveTemp(Expand);
    Entry.Fields = MoveTemp(Fields);
    Entries.Add(MoveTemp(Entry));
    return *this;
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddUpdate(
    FString Collection,
    FString RecordId,
    FOpenPocketBaseRecordBody Body,
    TArray<FString> Expand,
    TArray<FString> Fields)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Update;
    Entry.Collection = MoveTemp(Collection);
    Entry.RecordId = MoveTemp(RecordId);
    Entry.Body = MoveTemp(Body);
    Entry.Expand = MoveTemp(Expand);
    Entry.Fields = MoveTemp(Fields);
    Entries.Add(MoveTemp(Entry));
    return *this;
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddUpsert(
    FString Collection,
    FOpenPocketBaseRecordBody Body,
    TArray<FString> Expand,
    TArray<FString> Fields)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Upsert;
    Entry.Collection = MoveTemp(Collection);
    Entry.Body = MoveTemp(Body);
    Entry.Expand = MoveTemp(Expand);
    Entry.Fields = MoveTemp(Fields);
    Entries.Add(MoveTemp(Entry));
    return *this;
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddDelete(
    FString Collection,
    FString RecordId)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Delete;
    Entry.Collection = MoveTemp(Collection);
    Entry.RecordId = MoveTemp(RecordId);
    Entries.Add(MoveTemp(Entry));
    return *this;
}
