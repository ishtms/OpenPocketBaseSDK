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
    FOpenPocketBaseWritableCollectionRef& OutCollection,
    const TCHAR* Operation)
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
            Batch.Invalidate(FString::Printf(
                TEXT("Batch %s Collection is stale, missing, or read-only. Choose a current writable collection from the imported schema."),
                Operation));
            return EResolvedBatchCollection::Invalid;
        }
        return EResolvedBatchCollection::Typed;
    }
    if (Collection.Reference.Name.IsEmpty())
    {
        Batch.Invalidate(FString::Printf(
            TEXT("Batch %s has no Collection. Connect the Collection output from Use Collection."),
            Operation));
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

FOpenPocketBaseBatchOptions UOpenPocketBaseBatchLibrary::NewBatchOptions(
    const int32 MaxOperations,
    const int64 MaxBodyBytes)
{
    FOpenPocketBaseBatchOptions Options;
    Options.MaxOperations = MaxOperations;
    Options.MaxBodyBytes = MaxBodyBytes;
    return Options;
}

FOpenPocketBaseBatchOptions UOpenPocketBaseBatchLibrary::BatchOptionsWithRequestOptions(
    FOpenPocketBaseBatchOptions Options,
    FOpenPocketBaseRequestOptions RequestOptions)
{
    Options.RequestOptions = MoveTemp(RequestOptions);
    return Options;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithCreate(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    FOpenPocketBaseWritableCollectionRef Current;
    const EResolvedBatchCollection Resolved = ResolveBatchCollection(
        Batch, Collection, Current, TEXT("Create"));
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
    const EResolvedBatchCollection Resolved = ResolveBatchCollection(
        Batch, Collection, Current, TEXT("Update"));
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
    const FString& RecordId,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    FOpenPocketBaseWritableCollectionRef Current;
    const EResolvedBatchCollection Resolved = ResolveBatchCollection(
        Batch, Collection, Current, TEXT("Upsert"));
    if (Resolved == EResolvedBatchCollection::Typed)
    {
        Batch.AddUpsert(MoveTemp(Current), RecordId, MoveTemp(Body), MoveTemp(ResponseOptions));
    }
    else if (Resolved == EResolvedBatchCollection::Dynamic)
    {
        Batch.AddDynamicUpsert(
            Collection.Reference.Name,
            RecordId,
            MoveTemp(Body),
            ToDynamicQueryValues(ResponseOptions.Expand),
            ToDynamicQueryValues(ResponseOptions.Fields));
    }
    return Batch;
}

void UOpenPocketBaseBatchLibrary::BreakBatchResult(
    const FOpenPocketBaseBatchResult& Result,
    TArray<FOpenPocketBaseBatchOperationResult>& Results)
{
    Results = Result.Results;
}

void UOpenPocketBaseBatchLibrary::BreakBatchOperationResult(
    const FOpenPocketBaseBatchOperationResult& Result,
    EOpenPocketBaseBatchOperation& Operation,
    int32& HttpStatus,
    bool& bHasRecord,
    FOpenPocketBaseRecord& Record)
{
    Operation = Result.Operation;
    HttpStatus = Result.HttpStatus;
    bHasRecord = Result.bHasRecord;
    Record = Result.Record;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithDelete(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    const FString& RecordId)
{
    FOpenPocketBaseWritableCollectionRef Current;
    const EResolvedBatchCollection Resolved = ResolveBatchCollection(
        Batch, Collection, Current, TEXT("Delete"));
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
