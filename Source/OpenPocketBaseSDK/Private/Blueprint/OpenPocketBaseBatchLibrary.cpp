#include "OpenPocketBaseBatchLibrary.h"

void UOpenPocketBaseBatchLibrary::AddCreate(
    FOpenPocketBaseBatchRequest& Batch,
    const FString& Collection,
    FOpenPocketBaseRecordBody Body,
    const TArray<FString>& Expand,
    const TArray<FString>& Fields)
{
    Batch.AddCreate(Collection, MoveTemp(Body), Expand, Fields);
}

void UOpenPocketBaseBatchLibrary::AddUpdate(
    FOpenPocketBaseBatchRequest& Batch,
    const FString& Collection,
    const FString& RecordId,
    FOpenPocketBaseRecordBody Body,
    const TArray<FString>& Expand,
    const TArray<FString>& Fields)
{
    Batch.AddUpdate(Collection, RecordId, MoveTemp(Body), Expand, Fields);
}

void UOpenPocketBaseBatchLibrary::AddUpsert(
    FOpenPocketBaseBatchRequest& Batch,
    const FString& Collection,
    FOpenPocketBaseRecordBody Body,
    const TArray<FString>& Expand,
    const TArray<FString>& Fields)
{
    Batch.AddUpsert(Collection, MoveTemp(Body), Expand, Fields);
}

void UOpenPocketBaseBatchLibrary::AddDelete(
    FOpenPocketBaseBatchRequest& Batch,
    const FString& Collection,
    const FString& RecordId)
{
    Batch.AddDelete(Collection, RecordId);
}

void UOpenPocketBaseBatchLibrary::Clear(FOpenPocketBaseBatchRequest& Batch)
{
    Batch.Entries.Reset();
}
