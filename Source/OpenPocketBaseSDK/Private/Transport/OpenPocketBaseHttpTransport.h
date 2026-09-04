// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "Templates/SharedPointer.h"
#include "Transport/OpenPocketBaseTransport.h"

TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> CreateOpenPocketBaseHttpTransport();
