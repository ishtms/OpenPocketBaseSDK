#include "OpenPocketBaseStringLibrary.h"

#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "UObject/UnrealType.h"

namespace
{
bool IsSensitiveProperty(const FProperty* Property)
{
    const FString Name = Property->GetName();
    return Name.Contains(TEXT("Password"), ESearchCase::IgnoreCase) ||
        Name.Contains(TEXT("Secret"), ESearchCase::IgnoreCase) ||
        Name.Contains(TEXT("Token"), ESearchCase::IgnoreCase) ||
        Name.Contains(TEXT("Header"), ESearchCase::IgnoreCase) ||
        (Name.Contains(TEXT("Authorization"), ESearchCase::IgnoreCase) &&
            !Name.EndsWith(TEXT("Url"), ESearchCase::IgnoreCase));
}

TSharedPtr<FJsonValue> ExportDebugProperty(FProperty* Property, const void* Value)
{
    if (IsSensitiveProperty(Property))
    {
        return MakeShared<FJsonValueString>(TEXT("<redacted>"));
    }

    const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
    if (ArrayProperty != nullptr && CastField<FByteProperty>(ArrayProperty->Inner) != nullptr)
    {
        const FScriptArrayHelper Array(ArrayProperty, Value);
        return MakeShared<FJsonValueString>(
            FString::Printf(TEXT("<%d bytes>"), Array.Num()));
    }
    return nullptr;
}

void NormalizeBooleanPropertyNames(
    const UStruct* StructType,
    FString& Json,
    TSet<const UStruct*>& VisitedTypes);

void NormalizeNestedBooleanPropertyNames(
    const FProperty* Property,
    FString& Json,
    TSet<const UStruct*>& VisitedTypes)
{
    if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        NormalizeBooleanPropertyNames(StructProperty->Struct, Json, VisitedTypes);
    }
    else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
    {
        NormalizeNestedBooleanPropertyNames(ArrayProperty->Inner, Json, VisitedTypes);
    }
    else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
    {
        NormalizeNestedBooleanPropertyNames(SetProperty->ElementProp, Json, VisitedTypes);
    }
    else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
    {
        NormalizeNestedBooleanPropertyNames(MapProperty->KeyProp, Json, VisitedTypes);
        NormalizeNestedBooleanPropertyNames(MapProperty->ValueProp, Json, VisitedTypes);
    }
}

void NormalizeBooleanPropertyNames(
    const UStruct* StructType,
    FString& Json,
    TSet<const UStruct*>& VisitedTypes)
{
    if (StructType == nullptr || VisitedTypes.Contains(StructType))
    {
        return;
    }
    VisitedTypes.Add(StructType);

    for (TFieldIterator<FProperty> It(StructType); It; ++It)
    {
        const FProperty* Property = *It;
        const FString AuthoredName = Property->GetAuthoredName();
        if (CastField<FBoolProperty>(Property) != nullptr &&
            AuthoredName.Len() > 1 &&
            AuthoredName[0] == TEXT('b') &&
            FChar::IsUpper(AuthoredName[1]))
        {
            const FString JsonName = FJsonObjectConverter::StandardizeCase(AuthoredName);
            const FString FriendlyName = FJsonObjectConverter::StandardizeCase(
                AuthoredName.RightChop(1));
            Json.ReplaceInline(
                *FString::Printf(TEXT("\"%s\""), *JsonName),
                *FString::Printf(TEXT("\"%s\""), *FriendlyName),
                ESearchCase::CaseSensitive);
        }
        NormalizeNestedBooleanPropertyNames(Property, Json, VisitedTypes);
    }
}
}

FString UOpenPocketBaseStringLibrary::FormatStruct(
    const UScriptStruct* StructType,
    const void* Value)
{
    if (StructType == nullptr || Value == nullptr)
    {
        return TEXT("<invalid OpenPocketBase value>");
    }

    const FJsonObjectConverter::CustomExportCallback ExportCallback =
        FJsonObjectConverter::CustomExportCallback::CreateStatic(&ExportDebugProperty);
    FString Json;
    if (!FJsonObjectConverter::UStructToJsonObjectString(
            StructType,
            Value,
            Json,
            0,
            0,
            0,
            &ExportCallback,
            true))
    {
        StructType->ExportText(Json, Value, nullptr, nullptr, PPF_None, nullptr);
    }
    else
    {
        TSet<const UStruct*> VisitedTypes;
        NormalizeBooleanPropertyNames(StructType, Json, VisitedTypes);
    }

    return FString::Printf(
        TEXT("%s\n%s"),
        *StructType->GetDisplayNameText().ToString(),
        *Json);
}

FString UOpenPocketBaseStringLibrary::FormatEnum(const UEnum* EnumType, int64 Value)
{
    if (EnumType == nullptr)
    {
        return LexToString(Value);
    }

    const FText DisplayName = EnumType->GetDisplayNameTextByValue(Value);
    return DisplayName.IsEmpty() ? LexToString(Value) : DisplayName.ToString();
}

#define OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(Suffix, Type) \
    FString UOpenPocketBaseStringLibrary::Conv_##Suffix##ToString(const Type& Value) \
    { \
        return FormatStruct(Type::StaticStruct(), &Value); \
    }

OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseAssistedOAuth2Options, FOpenPocketBaseAssistedOAuth2Options)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseAuthAttempt, FOpenPocketBaseAuthAttempt)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseAuthMethods, FOpenPocketBaseAuthMethods)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseAuthResult, FOpenPocketBaseAuthResult)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseBatchEntry, FOpenPocketBaseBatchEntry)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseBatchOperationResult, FOpenPocketBaseBatchOperationResult)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseBatchOptions, FOpenPocketBaseBatchOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseBatchRequest, FOpenPocketBaseBatchRequest)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseBatchResult, FOpenPocketBaseBatchResult)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseCapabilityInfo, FOpenPocketBaseCapabilityInfo)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseCapabilityReport, FOpenPocketBaseCapabilityReport)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseClientConfig, FOpenPocketBaseClientConfig)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseCollection, FOpenPocketBaseCollection)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseCollectionRef, FOpenPocketBaseCollectionRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseAuthCollectionRef, FOpenPocketBaseAuthCollectionRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseWritableCollectionRef, FOpenPocketBaseWritableCollectionRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseSchemaCollection, FOpenPocketBaseSchemaCollection)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseSchemaField, FOpenPocketBaseSchemaField)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFieldRef, FOpenPocketBaseFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseAnyFieldRef, FOpenPocketBaseAnyFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseStringFieldRef, FOpenPocketBaseStringFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseNumberFieldRef, FOpenPocketBaseNumberFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseBooleanFieldRef, FOpenPocketBaseBooleanFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseDateFieldRef, FOpenPocketBaseDateFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseStringArrayFieldRef, FOpenPocketBaseStringArrayFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseJsonFieldRef, FOpenPocketBaseJsonFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRelationFieldRef, FOpenPocketBaseRelationFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFileFieldRef, FOpenPocketBaseFileFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseCustomRouteRequest, FOpenPocketBaseCustomRouteRequest)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseCustomRouteResponse, FOpenPocketBaseCustomRouteResponse)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseError, FOpenPocketBaseError)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseExternalAuth, FOpenPocketBaseExternalAuth)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseExternalAuthList, FOpenPocketBaseExternalAuthList)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFieldError, FOpenPocketBaseFieldError)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFileDownloadOptions, FOpenPocketBaseFileDownloadOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFileDownloadResult, FOpenPocketBaseFileDownloadResult)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFileInput, FOpenPocketBaseFileInput)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFileUrlOptions, FOpenPocketBaseFileUrlOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFilter, FOpenPocketBaseFilter)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFullListOptions, FOpenPocketBaseFullListOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFullListResult, FOpenPocketBaseFullListResult)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseHealthResult, FOpenPocketBaseHealthResult)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseListOptions, FOpenPocketBaseListOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseMfaContinuation, FOpenPocketBaseMfaContinuation)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseOAuth2AuthMethod, FOpenPocketBaseOAuth2AuthMethod)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseOAuth2Authorization, FOpenPocketBaseOAuth2Authorization)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseOAuth2Callback, FOpenPocketBaseOAuth2Callback)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseOAuth2StartOptions, FOpenPocketBaseOAuth2StartOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseOAuthProvider, FOpenPocketBaseOAuthProvider)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseOtpRequest, FOpenPocketBaseOtpRequest)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBasePasswordAuthMethod, FOpenPocketBasePasswordAuthMethod)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseProjectProfile, FOpenPocketBaseProjectProfile)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseSort, FOpenPocketBaseSort)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseExpand, FOpenPocketBaseExpand)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRealtimeEvent, FOpenPocketBaseRealtimeEvent)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRealtimeOptions, FOpenPocketBaseRealtimeOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRecord, FOpenPocketBaseRecord)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRecordBody, FOpenPocketBaseRecordBody)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRecordOptions, FOpenPocketBaseRecordOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRecordPage, FOpenPocketBaseRecordPage)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRequestOptions, FOpenPocketBaseRequestOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseSessionRestoreResult, FOpenPocketBaseSessionRestoreResult)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseSessionSnapshot, FOpenPocketBaseSessionSnapshot)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseThumbnailOptions, FOpenPocketBaseThumbnailOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseTimedAuthMethod, FOpenPocketBaseTimedAuthMethod)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseTransferProgress, FOpenPocketBaseTransferProgress)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseUploadLimits, FOpenPocketBaseUploadLimits)

#undef OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING

FString UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseFileTokenToString(
    const FOpenPocketBaseFileToken& Value)
{
    return FString::Printf(
        TEXT("Open Pocket Base File Token\n{\n\t\"isSet\": %s,\n\t\"value\": \"<redacted>\"\n}"),
        Value.IsSet() ? TEXT("true") : TEXT("false"));
}

#define OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(Suffix, Type) \
    FString UOpenPocketBaseStringLibrary::Conv_##Suffix##ToString(Type Value) \
    { \
        return FormatEnum(StaticEnum<Type>(), static_cast<int64>(Value)); \
    }

OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseAuthAttemptStatus, EOpenPocketBaseAuthAttemptStatus)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseBatchOperation, EOpenPocketBaseBatchOperation)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseCapability, EOpenPocketBaseCapability)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseCapabilityStatus, EOpenPocketBaseCapabilityStatus)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseCustomBodyFormat, EOpenPocketBaseCustomBodyFormat)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseCustomRouteMethod, EOpenPocketBaseCustomRouteMethod)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseErrorKind, EOpenPocketBaseErrorKind)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseFieldModifier, EOpenPocketBaseFieldModifier)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseFieldState, EOpenPocketBaseFieldState)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseStringComparison, EOpenPocketBaseStringComparison)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseNumberComparison, EOpenPocketBaseNumberComparison)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseBooleanComparison, EOpenPocketBaseBooleanComparison)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseDateComparison, EOpenPocketBaseDateComparison)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseNullComparison, EOpenPocketBaseNullComparison)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseFileDownloadTarget, EOpenPocketBaseFileDownloadTarget)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseJsonRootType, EOpenPocketBaseJsonRootType)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseRealtimeAction, EOpenPocketBaseRealtimeAction)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseRealtimeConnectionState, EOpenPocketBaseRealtimeConnectionState)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseSessionChangeReason, EOpenPocketBaseSessionChangeReason)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseSessionPersistence, EOpenPocketBaseSessionPersistence)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseSessionPersistenceState, EOpenPocketBaseSessionPersistenceState)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseSessionRestoreStatus, EOpenPocketBaseSessionRestoreStatus)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseThumbnailMode, EOpenPocketBaseThumbnailMode)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseTransferPhase, EOpenPocketBaseTransferPhase)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseCollectionType, EOpenPocketBaseCollectionType)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseFieldType, EOpenPocketBaseFieldType)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseFieldValueType, EOpenPocketBaseFieldValueType)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseSortDirection, EOpenPocketBaseSortDirection)

#undef OPENPOCKETBASE_IMPLEMENT_ENUM_STRING
