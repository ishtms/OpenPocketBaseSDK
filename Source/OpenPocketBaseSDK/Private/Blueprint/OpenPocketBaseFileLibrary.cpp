#include "OpenPocketBaseFileLibrary.h"

bool UOpenPocketBaseFileLibrary::TryBuildFileUrl(
    FOpenPocketBaseCollection Collection,
    FString RecordId,
    FString FileName,
    FOpenPocketBaseFileUrlOptions Options,
    FString& Url,
    FOpenPocketBaseError& Error)
{
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> NativeClient =
        Collection.Client != nullptr ? Collection.Client->GetNativeClient() : nullptr;
    if (!NativeClient.IsValid() || NativeClient->IsShutdown())
    {
        Url.Reset();
        Error = FOpenPocketBaseError();
        Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
        Error.ServerMessage = TEXT("A ready PocketBase client is required.");
        return false;
    }

    FOpenPocketBaseFileUrlResult Result = NativeClient->Files().BuildUrl(
        MoveTemp(Collection.Reference.Name),
        MoveTemp(RecordId),
        MoveTemp(FileName),
        MoveTemp(Options));
    if (!Result.IsSuccess())
    {
        Url.Reset();
        Error = Result.GetError();
        return false;
    }
    Url = Result.TakeValue();
    Error = {};
    return true;
}
