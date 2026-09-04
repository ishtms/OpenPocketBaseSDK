// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseSubsystem.h"

UOpenPocketBaseClient* UOpenPocketBaseSubsystem::CreateClient(
    const FName ClientName,
    const FOpenPocketBaseClientConfig& Config,
    FOpenPocketBaseError& OutError)
{
    if (GetClient(ClientName) != nullptr)
    {
        OutError = FOpenPocketBaseError();
        OutError.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
        OutError.Message = ClientName.IsNone()
            ? TEXT("The default PocketBase client already exists. Retrieve or remove it before creating another default client.")
            : FString::Printf(
                TEXT("PocketBase client '%s' already exists. Retrieve it or remove it before reusing that name."),
                *ClientName.ToString());
        return nullptr;
    }

    UOpenPocketBaseClient* Client = UOpenPocketBaseClient::Create(this, Config, OutError);
    if (Client == nullptr)
    {
        return nullptr;
    }

    if (ClientName.IsNone())
    {
        DefaultClient = Client;
    }
    else
    {
        NamedClients.Add(ClientName, Client);
    }
    return Client;
}

UOpenPocketBaseClient* UOpenPocketBaseSubsystem::GetClient(const FName ClientName) const
{
    if (ClientName.IsNone())
    {
        return DefaultClient;
    }

    if (const TObjectPtr<UOpenPocketBaseClient>* Client = NamedClients.Find(ClientName))
    {
        return Client->Get();
    }
    return nullptr;
}

UOpenPocketBaseClient* UOpenPocketBaseSubsystem::GetDefaultClient() const
{
    return DefaultClient;
}

bool UOpenPocketBaseSubsystem::RemoveClient(const FName ClientName)
{
    UOpenPocketBaseClient* Client = GetClient(ClientName);
    if (Client == nullptr)
    {
        return false;
    }

    Client->Shutdown();
    if (ClientName.IsNone())
    {
        DefaultClient = nullptr;
    }
    else
    {
        NamedClients.Remove(ClientName);
    }
    return true;
}

void UOpenPocketBaseSubsystem::Deinitialize()
{
    if (DefaultClient != nullptr)
    {
        DefaultClient->Shutdown();
        DefaultClient = nullptr;
    }

    for (const TPair<FName, TObjectPtr<UOpenPocketBaseClient>>& Pair : NamedClients)
    {
        if (Pair.Value != nullptr)
        {
            Pair.Value->Shutdown();
        }
    }
    NamedClients.Reset();
    Super::Deinitialize();
}
