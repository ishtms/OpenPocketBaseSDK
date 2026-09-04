// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

namespace UE::OpenPocketBase::Compatibility
{
inline FString GetEffectiveUrl(
    const FHttpRequestPtr& Request,
    const FHttpResponsePtr& Response)
{
    if (Response.IsValid() && !Response->GetEffectiveURL().IsEmpty())
    {
        return Response->GetEffectiveURL();
    }
    return Request.IsValid() ? Request->GetEffectiveURL() : FString();
}
}
