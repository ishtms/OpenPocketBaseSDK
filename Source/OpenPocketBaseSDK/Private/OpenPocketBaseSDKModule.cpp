// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseSDKModule.h"
#include "Transport/OpenPocketBaseHttpTransport.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FOpenPocketBaseSDKModule, OpenPocketBaseSDK)

void FOpenPocketBaseSDKModule::ShutdownModule()
{
    ShutdownOpenPocketBaseHttpTransports();
}
