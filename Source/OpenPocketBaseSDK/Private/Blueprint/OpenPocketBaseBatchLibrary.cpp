#include "OpenPocketBaseBatchLibrary.h"

namespace
{
bool ResolveBatchCollection(
    FOpenPocketBaseBatchRequest& Batch,
    const FOpenPocketBaseCollection& Collection,
    FOpenPocketBaseWritableCollectionRef& OutCollection)
{
    Batch.BindClient(Collection.Client);
    if (!Batch.IsValid())
    {
        return false;
    }
    if (!Collection.Reference.ResolveCurrentAs(OutCollection))
    {
        Batch.Invalidate(TEXT("Choose a writable PocketBase collection for every batch operation."));
        return false;
    }
    return true;
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
    if (ResolveBatchCollection(Batch, Collection, Current))
    {
        Batch.AddCreate(MoveTemp(Current), MoveTemp(Body), MoveTemp(ResponseOptions));
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
    if (ResolveBatchCollection(Batch, Collection, Current))
    {
        Batch.AddUpdate(
            MoveTemp(Current),
            RecordId,
            MoveTemp(Body),
            MoveTemp(ResponseOptions));
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
    if (ResolveBatchCollection(Batch, Collection, Current))
    {
        Batch.AddUpsert(MoveTemp(Current), MoveTemp(Body), MoveTemp(ResponseOptions));
    }
    return Batch;
}

FOpenPocketBaseBatchRequest UOpenPocketBaseBatchLibrary::WithDelete(
    FOpenPocketBaseBatchRequest Batch,
    FOpenPocketBaseCollection Collection,
    const FString& RecordId)
{
    FOpenPocketBaseWritableCollectionRef Current;
    if (ResolveBatchCollection(Batch, Collection, Current))
    {
        Batch.AddDelete(MoveTemp(Current), RecordId);
    }
    return Batch;
}
