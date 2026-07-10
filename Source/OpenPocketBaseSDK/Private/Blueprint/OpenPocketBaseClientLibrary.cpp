#include "OpenPocketBaseClientLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenPocketBaseSubsystem.h"

namespace
{
FOpenPocketBaseError MakeClientEntryError(const TCHAR* Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    Error.ServerMessage = Message;
    return Error;
}

UOpenPocketBaseSubsystem* GetPocketBaseSubsystem(const UObject* WorldContextObject)
{
    if (WorldContextObject == nullptr || GEngine == nullptr)
    {
        return nullptr;
    }

    const UWorld* World = GEngine->GetWorldFromContextObject(
        WorldContextObject,
        EGetWorldErrorMode::ReturnNull);
    UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
    return GameInstance != nullptr
        ? GameInstance->GetSubsystem<UOpenPocketBaseSubsystem>()
        : nullptr;
}

bool GetOrCreateClient(
    const UObject* WorldContextObject,
    const FName ClientName,
    const FOpenPocketBaseClientConfig& Config,
    UOpenPocketBaseClient*& OutClient,
    FOpenPocketBaseError& OutError)
{
    OutClient = nullptr;
    OutError = {};
    UOpenPocketBaseSubsystem* Subsystem = GetPocketBaseSubsystem(WorldContextObject);
    if (Subsystem == nullptr)
    {
        OutError = MakeClientEntryError(TEXT("A game instance is required to initialize PocketBase."));
        return false;
    }

    FString NormalizedBaseUrl;
    if (!Config.TryGetNormalizedBaseUrl(NormalizedBaseUrl, OutError))
    {
        return false;
    }

    if (UOpenPocketBaseClient* Existing = Subsystem->GetClient(ClientName))
    {
        if (Existing->IsReady() && Existing->GetBaseUrl() == NormalizedBaseUrl)
        {
            OutClient = Existing;
            return true;
        }
        OutError = MakeClientEntryError(
            TEXT("A PocketBase client with this name is already initialized for another server."));
        return false;
    }

    OutClient = Subsystem->CreateClient(ClientName, Config, OutError);
    return OutClient != nullptr;
}
}

bool UOpenPocketBaseClientLibrary::InitializePocketBase(
    const UObject* WorldContextObject,
    const FString& BaseUrl,
    UOpenPocketBaseClient*& Client,
    FOpenPocketBaseError& Error)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = BaseUrl;
    return GetOrCreateClient(WorldContextObject, NAME_None, Config, Client, Error);
}

bool UOpenPocketBaseClientLibrary::InitializePocketBaseWithConfig(
    const UObject* WorldContextObject,
    const FOpenPocketBaseClientConfig& Config,
    UOpenPocketBaseClient*& Client,
    FOpenPocketBaseError& Error)
{
    return GetOrCreateClient(WorldContextObject, NAME_None, Config, Client, Error);
}

UOpenPocketBaseClient* UOpenPocketBaseClientLibrary::GetPocketBaseClient(
    const UObject* WorldContextObject)
{
    UOpenPocketBaseSubsystem* Subsystem = GetPocketBaseSubsystem(WorldContextObject);
    return Subsystem != nullptr ? Subsystem->GetDefaultClient() : nullptr;
}

bool UOpenPocketBaseClientLibrary::CreateNamedPocketBaseClient(
    const UObject* WorldContextObject,
    const FName ClientName,
    const FOpenPocketBaseClientConfig& Config,
    UOpenPocketBaseClient*& Client,
    FOpenPocketBaseError& Error)
{
    if (ClientName.IsNone())
    {
        Client = nullptr;
        Error = MakeClientEntryError(TEXT("Named PocketBase clients require a name."));
        return false;
    }
    return GetOrCreateClient(WorldContextObject, ClientName, Config, Client, Error);
}

UOpenPocketBaseClient* UOpenPocketBaseClientLibrary::GetNamedPocketBaseClient(
    const UObject* WorldContextObject,
    const FName ClientName)
{
    UOpenPocketBaseSubsystem* Subsystem = GetPocketBaseSubsystem(WorldContextObject);
    return Subsystem != nullptr && !ClientName.IsNone()
        ? Subsystem->GetClient(ClientName)
        : nullptr;
}

bool UOpenPocketBaseClientLibrary::ShutdownPocketBase(const UObject* WorldContextObject)
{
    UOpenPocketBaseSubsystem* Subsystem = GetPocketBaseSubsystem(WorldContextObject);
    return Subsystem != nullptr && Subsystem->RemoveClient(NAME_None);
}

bool UOpenPocketBaseClientLibrary::RemoveNamedPocketBaseClient(
    const UObject* WorldContextObject,
    const FName ClientName)
{
    UOpenPocketBaseSubsystem* Subsystem = GetPocketBaseSubsystem(WorldContextObject);
    return Subsystem != nullptr && !ClientName.IsNone() && Subsystem->RemoveClient(ClientName);
}
