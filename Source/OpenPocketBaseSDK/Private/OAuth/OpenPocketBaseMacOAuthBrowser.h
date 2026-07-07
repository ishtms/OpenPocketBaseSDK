#pragma once

#include "OAuth/OpenPocketBaseOAuthBrowser.h"

TSharedRef<IOpenPocketBaseOAuthBrowser, ESPMode::ThreadSafe>
CreateOpenPocketBaseMacOAuthBrowser();
