#pragma once

#include "Templates/SharedPointer.h"
#include "Transport/OpenPocketBaseTransport.h"

TSharedRef<IOpenPocketBaseTransport, ESPMode::ThreadSafe> CreateOpenPocketBaseHttpTransport();
