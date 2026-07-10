#include "OpenPocketBaseBatchLibrary.h"

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::NewBatch()
{
    return {};
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithCreate(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    FOpenPocketBaseRecordBody Body,
    const TArray<FString>& Expand,
    const TArray<FString>& Fields)
{
    Batch.AddCreate(MoveTemp(Collection.Name), MoveTemp(Body), Expand, Fields);
    return Batch;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithUpdate(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    const FString& RecordId,
    FOpenPocketBaseRecordBody Body,
    const TArray<FString>& Expand,
    const TArray<FString>& Fields)
{
    Batch.AddUpdate(
        MoveTemp(Collection.Name),
        RecordId,
        MoveTemp(Body),
        Expand,
        Fields);
    return Batch;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithUpsert(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    FOpenPocketBaseRecordBody Body,
    const TArray<FString>& Expand,
    const TArray<FString>& Fields)
{
    Batch.AddUpsert(MoveTemp(Collection.Name), MoveTemp(Body), Expand, Fields);
    return Batch;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithDelete(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    const FString& RecordId)
{
    Batch.AddDelete(MoveTemp(Collection.Name), RecordId);
    return Batch;
}
