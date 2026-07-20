#include "OpenPocketBaseBatchLibrary.h"

namespace
{
enum class EResolvedBatchCollection : uint8
{
    Invalid,
    Typed,
    Dynamic
};

EResolvedBatchCollection ResolveBatchCollection(
    FOpenPocketBaseBatchRequest& Batch,
    const FOpenPocketBaseCollection& Collection,
    FOpenPocketBaseWritableCollectionRef& OutCollection)
{
    Batch.BindClient(Collection.Client);
    if (!Batch.IsValid())
    {
        return EResolvedBatchCollection::Invalid;
    }
    if (Collection.Reference.IsSet())
    {
        if (!Collection.Reference.ResolveCurrentAs(OutCollection))
        {
            Batch.Invalidate(TEXT("Choose a writable PocketBase collection for every batch operation."));
            return EResolvedBatchCollection::Invalid;
        }
        return EResolvedBatchCollection::Typed;
    }
    if (Collection.Reference.Name.IsEmpty())
    {
        Batch.Invalidate(TEXT("Choose a PocketBase collection for every batch operation."));
        return EResolvedBatchCollection::Invalid;
    }
    return EResolvedBatchCollection::Dynamic;
}

template <typename ValueType>
TArray<FString> ToDynamicQueryValues(const TArray<ValueType>& Values)
{
    TArray<FString> Result;
    Result.Reserve(Values.Num());
    for (const ValueType& Value : Values)
    {
        Result.Add(Value.ToQueryValue());
    }
    return Result;
}
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::NewBatch()
{
    return {};
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithCreate(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    FOpenPocketBaseWritableCollectionRef Current;
    const EResolvedBatchCollection Resolved = ResolveBatchCollection(Batch, Collection, Current);
    if (Resolved == EResolvedBatchCollection::Typed)
    {
        Batch.AddCreate(MoveTemp(Current), MoveTemp(Body), MoveTemp(ResponseOptions));
    }
    else if (Resolved == EResolvedBatchCollection::Dynamic)
    {
        Batch.AddDynamicCreate(
            Collection.Reference.Name,
            MoveTemp(Body),
            ToDynamicQueryValues(ResponseOptions.Expand),
            ToDynamicQueryValues(ResponseOptions.Fields));
    }
    return Batch;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithUpdate(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    const FString& RecordId,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    FOpenPocketBaseWritableCollectionRef Current;
    const EResolvedBatchCollection Resolved = ResolveBatchCollection(Batch, Collection, Current);
    if (Resolved == EResolvedBatchCollection::Typed)
    {
        Batch.AddUpdate(
            MoveTemp(Current),
            RecordId,
            MoveTemp(Body),
            MoveTemp(ResponseOptions));
    }
    else if (Resolved == EResolvedBatchCollection::Dynamic)
    {
        Batch.AddDynamicUpdate(
            Collection.Reference.Name,
            RecordId,
            MoveTemp(Body),
            ToDynamicQueryValues(ResponseOptions.Expand),
            ToDynamicQueryValues(ResponseOptions.Fields));
    }
    return Batch;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithUpsert(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    FOpenPocketBaseWritableCollectionRef Current;
    const EResolvedBatchCollection Resolved = ResolveBatchCollection(Batch, Collection, Current);
    if (Resolved == EResolvedBatchCollection::Typed)
    {
        Batch.AddUpsert(MoveTemp(Current), MoveTemp(Body), MoveTemp(ResponseOptions));
    }
    else if (Resolved == EResolvedBatchCollection::Dynamic)
    {
        Batch.AddDynamicUpsert(
            Collection.Reference.Name,
            MoveTemp(Body),
            ToDynamicQueryValues(ResponseOptions.Expand),
            ToDynamicQueryValues(ResponseOptions.Fields));
    }
    return Batch;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithDelete(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    const FString& RecordId)
{
    FOpenPocketBaseWritableCollectionRef Current;
    const EResolvedBatchCollection Resolved = ResolveBatchCollection(Batch, Collection, Current);
    if (Resolved == EResolvedBatchCollection::Typed)
    {
        Batch.AddDelete(MoveTemp(Current), RecordId);
    }
    else if (Resolved == EResolvedBatchCollection::Dynamic)
    {
        Batch.AddDynamicDelete(Collection.Reference.Name, RecordId);
    }
    return Batch;
}
