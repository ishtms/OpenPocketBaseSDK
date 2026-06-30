#pragma once

#include "OpenPocketBaseRecord.h"
#include "OpenPocketBaseResult.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace OpenPocketBase::Json
{
TOpenPocketBaseResult<FOpenPocketBaseRecord> ParseRecordResponse(
    const FOpenPocketBaseHttpResponse& Response);

TOpenPocketBaseResult<FOpenPocketBaseRecordPage> ParseRecordPageResponse(
    const FOpenPocketBaseHttpResponse& Response);

TOpenPocketBaseResult<FOpenPocketBaseAuthResult> ParseAuthResponse(
    const FOpenPocketBaseHttpResponse& Response,
    FString& OutToken);

TArray<uint8> SerializeObject(const TSharedRef<FJsonObject>& Object);
}
