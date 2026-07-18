#include "OpenPocketBaseBatchLibrary.h"

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::NewBatch()
{
    return {};
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithCreate(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseWritableCollectionRef Collection,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    Batch.AddCreate(MoveTemp(Collection), MoveTemp(Body), MoveTemp(ResponseOptions));
    return Batch;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithUpdate(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseWritableCollectionRef Collection,
    const FString& RecordId,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    Batch.AddUpdate(
        MoveTemp(Collection),
        RecordId,
        MoveTemp(Body),
        MoveTemp(ResponseOptions));
    return Batch;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithUpsert(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseWritableCollectionRef Collection,
    FOpenPocketBaseRecordBody Body,
    FOpenPocketBaseRecordOptions ResponseOptions)
{
    Batch.AddUpsert(MoveTemp(Collection), MoveTemp(Body), MoveTemp(ResponseOptions));
    return Batch;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithDelete(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseWritableCollectionRef Collection,
    const FString& RecordId)
{
    Batch.AddDelete(MoveTemp(Collection), RecordId);
    return Batch;
}
