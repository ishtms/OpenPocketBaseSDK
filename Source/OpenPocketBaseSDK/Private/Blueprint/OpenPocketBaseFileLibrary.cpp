#include "OpenPocketBaseFileLibrary.h"

FOpenPocketBaseFileInput UOpenPocketBaseFileLibrary::FileFromPath(
    FOpenPocketBaseFileFieldRef Field,
    FString FilePath,
    FString FileName,
    FString ContentType,
    const EOpenPocketBaseFieldModifier Modifier)
{
    return FOpenPocketBaseFileInput::FromPath(
        MoveTemp(Field),
        MoveTemp(FilePath),
        MoveTemp(FileName),
        MoveTemp(ContentType),
        Modifier);
}

FOpenPocketBaseFileInput UOpenPocketBaseFileLibrary::FileFromBytes(
    FOpenPocketBaseFileFieldRef Field,
    TArray<uint8> Bytes,
    FString FileName,
    FString ContentType,
    const EOpenPocketBaseFieldModifier Modifier)
{
    return FOpenPocketBaseFileInput::FromBytes(
        MoveTemp(Field),
        MoveTemp(Bytes),
        MoveTemp(FileName),
        MoveTemp(ContentType),
        Modifier);
}

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
        MoveTemp(Collection.Reference),
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
