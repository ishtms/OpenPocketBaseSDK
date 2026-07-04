#pragma once

#include "OpenPocketBaseBatch.h"
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

TOpenPocketBaseResult<bool> ParseEmptyResponse(
    const FOpenPocketBaseHttpResponse& Response);

TOpenPocketBaseResult<FOpenPocketBaseBatchResult> ParseBatchResponse(
    const FOpenPocketBaseHttpResponse& Response,
    const FOpenPocketBaseBatchRequest& Request);

TArray<uint8> SerializeObject(const TSharedRef<FJsonObject>& Object);
}
