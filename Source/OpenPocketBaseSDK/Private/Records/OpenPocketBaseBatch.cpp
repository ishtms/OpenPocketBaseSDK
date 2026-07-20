#include "OpenPocketBaseBatch.h"

#include "OpenPocketBaseBlueprintClient.h"

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
    FOpenPocketBaseWritableCollectionRef Current;
    return Collection.ResolveCurrentAs(Current) ? Current.Name : DynamicCollection;
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

bool FOpenPocketBaseBatchRequest::IsValid() const
{
    return bValid;
}

UOpenPocketBaseClient* FOpenPocketBaseBatchRequest::GetClient() const
{
    return Client;
}

void FOpenPocketBaseBatchRequest::BindClient(UOpenPocketBaseClient* InClient)
{
    if (!bValid)
    {
        return;
    }
    if (InClient == nullptr)
    {
        Invalidate(TEXT("Every batch operation requires a PocketBase collection."));
        return;
    }
    if (Client != nullptr && Client != InClient)
    {
        Invalidate(TEXT("Every batch operation must use the same PocketBase client."));
        return;
    }
    Client = InClient;
}

void FOpenPocketBaseBatchRequest::Invalidate(FString Message)
{
    if (bValid)
    {
        bValid = false;
        ErrorMessage = MoveTemp(Message);
    }
}

FOpenPocketBaseBatchRequest& FOpenPocketBaseBatchRequest::AddCreate(
    FOpenPocketBaseWritableCollectionRef Collection,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    FOpenPocketBaseBatchEntry Entry;
    Entry.Operation = EOpenPocketBaseBatchOperation::Create;
    Collection.ResolveCurrentAs(Entry.Collection);
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
    Collection.ResolveCurrentAs(Entry.Collection);
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
    Collection.ResolveCurrentAs(Entry.Collection);
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
    Collection.ResolveCurrentAs(Entry.Collection);
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
