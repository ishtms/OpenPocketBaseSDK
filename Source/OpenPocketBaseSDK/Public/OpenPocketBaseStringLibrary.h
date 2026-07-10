#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseAuthentication.h"
#include "OpenPocketBaseBatch.h"
#include "OpenPocketBaseCapability.h"
#include "OpenPocketBaseClientConfig.h"
#include "OpenPocketBaseCustomRoute.h"
#include "OpenPocketBaseError.h"
#include "OpenPocketBaseFile.h"
#include "OpenPocketBaseFilter.h"
#include "OpenPocketBaseProjectSettings.h"
#include "OpenPocketBaseRealtime.h"
#include "OpenPocketBaseRecord.h"
#include "OpenPocketBaseSession.h"

#include "OpenPocketBaseStringLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseStringLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static FString FormatStruct(const UScriptStruct* StructType, const void* Value);
    static FString FormatEnum(const UEnum* EnumType, int64 Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Assisted OAuth2 Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseAssistedOAuth2OptionsToString(const FOpenPocketBaseAssistedOAuth2Options& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Auth Attempt)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseAuthAttemptToString(const FOpenPocketBaseAuthAttempt& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Auth Methods)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseAuthMethodsToString(const FOpenPocketBaseAuthMethods& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Auth Result)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseAuthResultToString(const FOpenPocketBaseAuthResult& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Batch Entry)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseBatchEntryToString(const FOpenPocketBaseBatchEntry& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Batch Operation Result)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseBatchOperationResultToString(const FOpenPocketBaseBatchOperationResult& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Batch Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseBatchOptionsToString(const FOpenPocketBaseBatchOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Batch Request)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseBatchRequestToString(const FOpenPocketBaseBatchRequest& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Batch Result)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseBatchResultToString(const FOpenPocketBaseBatchResult& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Capability Info)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseCapabilityInfoToString(const FOpenPocketBaseCapabilityInfo& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Capability Report)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseCapabilityReportToString(const FOpenPocketBaseCapabilityReport& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Client Config)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseClientConfigToString(const FOpenPocketBaseClientConfig& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Custom Route Request)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseCustomRouteRequestToString(const FOpenPocketBaseCustomRouteRequest& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Custom Route Response)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseCustomRouteResponseToString(const FOpenPocketBaseCustomRouteResponse& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Error)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseErrorToString(const FOpenPocketBaseError& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (External Auth)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseExternalAuthToString(const FOpenPocketBaseExternalAuth& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (External Auth List)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseExternalAuthListToString(const FOpenPocketBaseExternalAuthList& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Field Error)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFieldErrorToString(const FOpenPocketBaseFieldError& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (File Download Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFileDownloadOptionsToString(const FOpenPocketBaseFileDownloadOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (File Download Result)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFileDownloadResultToString(const FOpenPocketBaseFileDownloadResult& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (File Input)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFileInputToString(const FOpenPocketBaseFileInput& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (File Token)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFileTokenToString(const FOpenPocketBaseFileToken& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (File URL Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFileUrlOptionsToString(const FOpenPocketBaseFileUrlOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Filter)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFilterToString(const FOpenPocketBaseFilter& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Full List Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFullListOptionsToString(const FOpenPocketBaseFullListOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Full List Result)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFullListResultToString(const FOpenPocketBaseFullListResult& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Health Result)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseHealthResultToString(const FOpenPocketBaseHealthResult& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (List Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseListOptionsToString(const FOpenPocketBaseListOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (MFA Continuation)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseMfaContinuationToString(const FOpenPocketBaseMfaContinuation& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (OAuth2 Auth Method)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseOAuth2AuthMethodToString(const FOpenPocketBaseOAuth2AuthMethod& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (OAuth2 Authorization)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseOAuth2AuthorizationToString(const FOpenPocketBaseOAuth2Authorization& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (OAuth2 Callback)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseOAuth2CallbackToString(const FOpenPocketBaseOAuth2Callback& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (OAuth2 Start Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseOAuth2StartOptionsToString(const FOpenPocketBaseOAuth2StartOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (OAuth Provider)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseOAuthProviderToString(const FOpenPocketBaseOAuthProvider& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (OTP Request)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseOtpRequestToString(const FOpenPocketBaseOtpRequest& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Password Auth Method)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBasePasswordAuthMethodToString(const FOpenPocketBasePasswordAuthMethod& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Project Profile)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseProjectProfileToString(const FOpenPocketBaseProjectProfile& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Realtime Event)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseRealtimeEventToString(const FOpenPocketBaseRealtimeEvent& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Realtime Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseRealtimeOptionsToString(const FOpenPocketBaseRealtimeOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Record)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseRecordToString(const FOpenPocketBaseRecord& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Record Body)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseRecordBodyToString(const FOpenPocketBaseRecordBody& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Record Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseRecordOptionsToString(const FOpenPocketBaseRecordOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Record Page)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseRecordPageToString(const FOpenPocketBaseRecordPage& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Request Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseRequestOptionsToString(const FOpenPocketBaseRequestOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Session Restore Result)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseSessionRestoreResultToString(const FOpenPocketBaseSessionRestoreResult& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Session Snapshot)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseSessionSnapshotToString(const FOpenPocketBaseSessionSnapshot& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Thumbnail Options)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseThumbnailOptionsToString(const FOpenPocketBaseThumbnailOptions& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Timed Auth Method)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseTimedAuthMethodToString(const FOpenPocketBaseTimedAuthMethod& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Transfer Progress)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseTransferProgressToString(const FOpenPocketBaseTransferProgress& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Upload Limits)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseUploadLimitsToString(const FOpenPocketBaseUploadLimits& Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Auth Attempt Status)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseAuthAttemptStatusToString(EOpenPocketBaseAuthAttemptStatus Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Batch Operation)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseBatchOperationToString(EOpenPocketBaseBatchOperation Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Capability)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseCapabilityToString(EOpenPocketBaseCapability Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Capability Status)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseCapabilityStatusToString(EOpenPocketBaseCapabilityStatus Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Custom Body Format)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseCustomBodyFormatToString(EOpenPocketBaseCustomBodyFormat Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Custom Route Method)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseCustomRouteMethodToString(EOpenPocketBaseCustomRouteMethod Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Error Kind)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseErrorKindToString(EOpenPocketBaseErrorKind Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Field Modifier)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFieldModifierToString(EOpenPocketBaseFieldModifier Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Field State)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFieldStateToString(EOpenPocketBaseFieldState Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (String Comparison)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseStringComparisonToString(EOpenPocketBaseStringComparison Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Number Comparison)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseNumberComparisonToString(EOpenPocketBaseNumberComparison Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Boolean Comparison)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseBooleanComparisonToString(EOpenPocketBaseBooleanComparison Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Date Comparison)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseDateComparisonToString(EOpenPocketBaseDateComparison Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Null Comparison)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseNullComparisonToString(EOpenPocketBaseNullComparison Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (File Download Target)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseFileDownloadTargetToString(EOpenPocketBaseFileDownloadTarget Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (JSON Root Type)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseJsonRootTypeToString(EOpenPocketBaseJsonRootType Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Realtime Action)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseRealtimeActionToString(EOpenPocketBaseRealtimeAction Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Realtime Connection State)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseRealtimeConnectionStateToString(EOpenPocketBaseRealtimeConnectionState Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Session Change Reason)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseSessionChangeReasonToString(EOpenPocketBaseSessionChangeReason Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Session Persistence)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseSessionPersistenceToString(EOpenPocketBaseSessionPersistence Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Session Persistence State)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseSessionPersistenceStateToString(EOpenPocketBaseSessionPersistenceState Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Session Restore Status)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseSessionRestoreStatusToString(EOpenPocketBaseSessionRestoreStatus Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Thumbnail Mode)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseThumbnailModeToString(EOpenPocketBaseThumbnailMode Value);

    UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (Transfer Phase)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Open PocketBase|Utilities|String")
    static FString Conv_OpenPocketBaseTransferPhaseToString(EOpenPocketBaseTransferPhase Value);
};
