// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseClientLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenPocketBaseProjectSettings.h"
#include "OpenPocketBaseSubsystem.h"

namespace
{
FOpenPocketBaseError MakeClientEntryError(FString Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    Error.Message = MoveTemp(Message);
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
        OutError = MakeClientEntryError(TEXT("Initialize PocketBase needs a valid gameplay World Context with a Game Instance. Call it from a Level Blueprint, Actor, Component, Widget, or other object that belongs to the running game."));
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
            ClientName.IsNone()
                ? TEXT("The default PocketBase client is already initialized for another Base URL. Shut it down before connecting the default client to a different server.")
                : FString::Printf(
                      TEXT("PocketBase client '%s' is already initialized for another Base URL. Remove that named client before reusing its name for a different server."),
                      *ClientName.ToString()));
        return false;
    }

    OutClient = Subsystem->CreateClient(ClientName, Config, OutError);
    return OutClient != nullptr;
}
}

void UOpenPocketBaseClientLibrary::BreakError(
    const FOpenPocketBaseError& Error,
    EOpenPocketBaseErrorKind& Kind,
    int32& HttpStatus,
    FString& Code,
    FString& Message,
    TMap<FString, FOpenPocketBaseFieldError>& FieldErrors,
    bool& bMayRetry,
    FString& RequestId)
{
    Kind = Error.Kind;
    HttpStatus = Error.HttpStatus;
    Code = Error.Code;
    Message = Error.Message;
    FieldErrors = Error.FieldErrors;
    bMayRetry = Error.bMayRetry;
    RequestId = Error.RequestId;
}

bool UOpenPocketBaseClientLibrary::TryGetFieldError(
    const FOpenPocketBaseError& Error,
    const FString& FieldName,
    FOpenPocketBaseFieldError& FieldError)
{
    FieldError = {};
    const FOpenPocketBaseFieldError* Found = Error.FieldErrors.Find(FieldName);
    if (Found == nullptr)
    {
        return false;
    }
    FieldError = *Found;
    return true;
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

bool UOpenPocketBaseClientLibrary::InitializePocketBaseFromProjectSettings(
    const UObject* WorldContextObject,
    const FName Profile,
    UOpenPocketBaseClient*& Client,
    FOpenPocketBaseError& Error)
{
    Client = nullptr;
    FOpenPocketBaseClientConfig Config;
    if (!GetDefault<UOpenPocketBaseProjectSettings>()->TryResolveProfile(Profile, Config, Error))
    {
        return false;
    }
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
        Error = MakeClientEntryError(TEXT("Client Name is None. Enter a non-empty name when creating an additional PocketBase client, or use Initialize PocketBase for the default client."));
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
