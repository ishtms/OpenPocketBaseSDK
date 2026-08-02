#include "OpenPocketBaseStringLibrary.h"

#include "Dom/JsonValue.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "JsonObjectConverter.h"
#include "UObject/UnrealType.h"

namespace
{
bool IsSensitiveDebugName(const FString& Name)
{
    FString Normalized = Name.ToLower();
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));

    return Normalized.Contains(TEXT("password")) ||
        Normalized.Contains(TEXT("passcode")) ||
        Normalized.Contains(TEXT("secret")) ||
        Normalized.Contains(TEXT("token")) ||
        Normalized.Contains(TEXT("authorization")) ||
        Normalized.Contains(TEXT("credential")) ||
        Normalized.Contains(TEXT("cookie")) ||
        Normalized.Contains(TEXT("apikey")) ||
        Normalized.Contains(TEXT("codeverifier")) ||
        Normalized.Contains(TEXT("codechallenge")) ||
        Normalized == TEXT("code") ||
        Normalized == TEXT("state") ||
        Normalized == TEXT("transactionid") ||
        Normalized == TEXT("mfaid") ||
        Normalized == TEXT("otpid");
}

FString RedactUrlParameters(const FString& Parameters)
{
    TArray<FString> Parts;
    Parameters.ParseIntoArray(Parts, TEXT("&"), false);
    for (FString& Part : Parts)
    {
        FString Name;
        FString Value;
        if (!Part.Split(TEXT("="), &Name, &Value))
        {
            Name = Part;
        }
        if (IsSensitiveDebugName(FGenericPlatformHttp::UrlDecode(Name)))
        {
            Part = Name + TEXT("=<redacted>");
        }
    }
    return FString::Join(Parts, TEXT("&"));
}

FString RedactUrlSecrets(const FString& Url)
{
    int32 QueryIndex = INDEX_NONE;
    int32 FragmentIndex = INDEX_NONE;
    Url.FindChar(TEXT('?'), QueryIndex);
    Url.FindChar(TEXT('#'), FragmentIndex);

    if (QueryIndex == INDEX_NONE && FragmentIndex == INDEX_NONE)
    {
        return Url;
    }

    const int32 FirstParameterIndex = QueryIndex == INDEX_NONE
        ? FragmentIndex
        : FragmentIndex == INDEX_NONE
            ? QueryIndex
            : FMath::Min(QueryIndex, FragmentIndex);
    FString Result = Url.Left(FirstParameterIndex);

    if (QueryIndex != INDEX_NONE && (FragmentIndex == INDEX_NONE || QueryIndex < FragmentIndex))
    {
        const int32 QueryEnd = FragmentIndex == INDEX_NONE ? Url.Len() : FragmentIndex;
        Result += TEXT("?") + RedactUrlParameters(
            Url.Mid(QueryIndex + 1, QueryEnd - QueryIndex - 1));
    }
    if (FragmentIndex != INDEX_NONE)
    {
        Result += TEXT("#") + RedactUrlParameters(Url.Mid(FragmentIndex + 1));
    }
    return Result;
}

TSharedPtr<FJsonValue> SanitizeJsonValue(
    const TSharedPtr<FJsonValue>& Value,
    const FString& FieldName);

TSharedPtr<FJsonObject> SanitizeJsonObject(const TSharedPtr<FJsonObject>& Object)
{
    if (!Object.IsValid())
    {
        return nullptr;
    }

    const TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
    {
        Result->SetField(Pair.Key, SanitizeJsonValue(Pair.Value, Pair.Key));
    }
    return Result;
}

TSharedPtr<FJsonValue> SanitizeJsonValue(
    const TSharedPtr<FJsonValue>& Value,
    const FString& FieldName)
{
    if (IsSensitiveDebugName(FieldName))
    {
        return MakeShared<FJsonValueString>(TEXT("<redacted>"));
    }
    if (!Value.IsValid())
    {
        return MakeShared<FJsonValueNull>();
    }
    if (Value->Type == EJson::Object)
    {
        return MakeShared<FJsonValueObject>(SanitizeJsonObject(Value->AsObject()));
    }
    if (Value->Type == EJson::Array)
    {
        TArray<TSharedPtr<FJsonValue>> Items;
        Items.Reserve(Value->AsArray().Num());
        for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
        {
            Items.Add(SanitizeJsonValue(Item, FString()));
        }
        return MakeShared<FJsonValueArray>(MoveTemp(Items));
    }
    return Value;
}

bool IsOAuthUrlProperty(const FProperty* Property)
{
    const UStruct* Owner = Property->GetOwnerStruct();
    const FString Name = Property->GetName();
    return (Owner == FOpenPocketBaseOAuth2Callback::StaticStruct() &&
            Name == TEXT("CallbackUrl")) ||
        (Owner == FOpenPocketBaseOAuth2Authorization::StaticStruct() &&
            Name == TEXT("AuthorizationUrl")) ||
        (Owner == FOpenPocketBaseOAuthProvider::StaticStruct() &&
            Name == TEXT("AuthUrl"));
}

bool IsSensitiveProperty(const FProperty* Property)
{
    const FString Name = Property->GetName();
    const UStruct* Owner = Property->GetOwnerStruct();
    if (Owner == FOpenPocketBaseAuthMethods::StaticStruct() &&
        Name.Equals(TEXT("Password"), ESearchCase::CaseSensitive))
    {
        return false;
    }
    if ((Owner == FOpenPocketBaseOAuth2Callback::StaticStruct() &&
            Name == TEXT("TransactionId")) ||
        (Owner == FOpenPocketBaseOAuth2Authorization::StaticStruct() &&
            (Name == TEXT("TransactionId") || Name == TEXT("State"))) ||
        (Owner == FOpenPocketBaseOAuthProvider::StaticStruct() &&
            (Name == TEXT("State") || Name == TEXT("CodeVerifier"))) ||
        (Owner == FOpenPocketBaseMfaContinuation::StaticStruct() && Name == TEXT("Id")) ||
        (Owner == FOpenPocketBaseOtpRequest::StaticStruct() && Name == TEXT("OtpId")))
    {
        return true;
    }
    return Name.Contains(TEXT("Password"), ESearchCase::IgnoreCase) ||
        Name.Contains(TEXT("Secret"), ESearchCase::IgnoreCase) ||
        Name.Contains(TEXT("Token"), ESearchCase::IgnoreCase) ||
        Name.Contains(TEXT("Header"), ESearchCase::IgnoreCase) ||
        (Name.Contains(TEXT("Authorization"), ESearchCase::IgnoreCase) &&
            !Name.EndsWith(TEXT("Url"), ESearchCase::IgnoreCase));
}

TSharedPtr<FJsonValue> ExportDebugProperty(FProperty* Property, const void* Value)
{
    if (IsOAuthUrlProperty(Property))
    {
        return MakeShared<FJsonValueString>(
            RedactUrlSecrets(*static_cast<const FString*>(Value)));
    }

    if (Property->GetOwnerStruct() == FOpenPocketBaseCustomRouteRequest::StaticStruct())
    {
        const FString Name = Property->GetName();
        if (Name == TEXT("Query") || Name == TEXT("FormFields"))
        {
            const TMap<FString, FString>& Fields =
                *static_cast<const TMap<FString, FString>*>(Value);
            const TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
            for (const TPair<FString, FString>& Pair : Fields)
            {
                Object->SetStringField(
                    Pair.Key,
                    IsSensitiveDebugName(Pair.Key) ? TEXT("<redacted>") : Pair.Value);
            }
            return MakeShared<FJsonValueObject>(Object);
        }
        if (Name == TEXT("JsonBody"))
        {
            FJsonObjectWrapper Wrapper =
                *static_cast<const FJsonObjectWrapper*>(Value);
            if (!Wrapper.JsonObject.IsValid() && !Wrapper.JsonString.IsEmpty())
            {
                Wrapper.JsonObjectFromString(Wrapper.JsonString);
            }
            if (Wrapper.JsonObject.IsValid())
            {
                return MakeShared<FJsonValueObject>(SanitizeJsonObject(Wrapper.JsonObject));
            }
        }
    }

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
            CPF_Transient,
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
        if (StructType == FOpenPocketBaseAuthMethods::StaticStruct())
        {
            Json.ReplaceInline(
                TEXT("\"oAuth2\""),
                TEXT("\"oauth2\""),
                ESearchCase::CaseSensitive);
        }
    }

    FString TypeName;
#if WITH_EDITORONLY_DATA
    TypeName = StructType->GetDisplayNameText().ToString();
#endif
    if (TypeName.IsEmpty())
    {
        TypeName = FName::NameToDisplayString(StructType->GetAuthoredName(), false);
    }

    return FString::Printf(TEXT("%s\n%s"), *TypeName, *Json);
}

FString UOpenPocketBaseStringLibrary::FormatEnum(const UEnum* EnumType, int64 Value)
{
    if (EnumType == nullptr)
    {
        return LexToString(Value);
    }

    FString DisplayName;
#if WITH_EDITORONLY_DATA
    DisplayName = EnumType->GetDisplayNameTextByValue(Value).ToString();
#endif
    if (DisplayName.IsEmpty())
    {
        const FString AuthoredName = EnumType->GetAuthoredNameStringByValue(Value);
        DisplayName = AuthoredName.IsEmpty()
            ? LexToString(Value)
            : FName::NameToDisplayString(AuthoredName, false);
    }
    return DisplayName;
}

#define OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(Suffix, Type) \
    FString UOpenPocketBaseStringLibrary::Conv_##Suffix##ToString(const Type& Value) \
    { \
        return FormatStruct(Type::StaticStruct(), &Value); \
    }

#define OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(Suffix, Type) \
    FString UOpenPocketBaseStringLibrary::Conv_##Suffix##ToString(Type Value) \
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
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseWritableCollection, FOpenPocketBaseWritableCollection)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseAuthCollection, FOpenPocketBaseAuthCollection)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseCollectionRef, FOpenPocketBaseCollectionRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseAuthCollectionRef, FOpenPocketBaseAuthCollectionRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseWritableCollectionRef, FOpenPocketBaseWritableCollectionRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseSchemaCollection, FOpenPocketBaseSchemaCollection)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseSchemaField, FOpenPocketBaseSchemaField)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseFieldRef, FOpenPocketBaseFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseAnyFieldRef, FOpenPocketBaseAnyFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseStringFieldRef, FOpenPocketBaseStringFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseTextFieldRef, FOpenPocketBaseTextFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseNumberFieldRef, FOpenPocketBaseNumberFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseBooleanFieldRef, FOpenPocketBaseBooleanFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseDateFieldRef, FOpenPocketBaseDateFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseStringArrayFieldRef, FOpenPocketBaseStringArrayFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseJsonFieldRef, FOpenPocketBaseJsonFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseGeoPointFieldRef, FOpenPocketBaseGeoPointFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseSingleSelectFieldRef, FOpenPocketBaseSingleSelectFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseMultipleSelectFieldRef, FOpenPocketBaseMultipleSelectFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseRelationFieldRef, FOpenPocketBaseRelationFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseSingleRelationFieldRef, FOpenPocketBaseSingleRelationFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseMultipleRelationFieldRef, FOpenPocketBaseMultipleRelationFieldRef)
OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING(OpenPocketBaseFileFieldRef, FOpenPocketBaseFileFieldRef)
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
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseFieldSelection, FOpenPocketBaseFieldSelection)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRealtimeEvent, FOpenPocketBaseRealtimeEvent)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRealtimeOptions, FOpenPocketBaseRealtimeOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRecord, FOpenPocketBaseRecord)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseGeoPoint, FOpenPocketBaseGeoPoint)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRecordBody, FOpenPocketBaseRecordBody)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRecordOptions, FOpenPocketBaseRecordOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRecordPage, FOpenPocketBaseRecordPage)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseSelectValues, FOpenPocketBaseSelectValues)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseRequestOptions, FOpenPocketBaseRequestOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseSessionRestoreResult, FOpenPocketBaseSessionRestoreResult)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseSessionSnapshot, FOpenPocketBaseSessionSnapshot)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseThumbnailOptions, FOpenPocketBaseThumbnailOptions)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseTimedAuthMethod, FOpenPocketBaseTimedAuthMethod)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseTransferProgress, FOpenPocketBaseTransferProgress)
OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING(OpenPocketBaseUploadLimits, FOpenPocketBaseUploadLimits)

#undef OPENPOCKETBASE_IMPLEMENT_STRUCT_STRING
#undef OPENPOCKETBASE_IMPLEMENT_STRUCT_VALUE_STRING

FString UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseJsonValueToString(
    const FOpenPocketBaseJsonValue& Value)
{
    if (!Value.IsValid())
    {
        return FString::Printf(
            TEXT("Open Pocket Base JSON Value\nInvalid: %s"),
            Value.ErrorMessage.IsEmpty() ? TEXT("The value is not set.") : *Value.ErrorMessage);
    }
    return FString::Printf(TEXT("Open Pocket Base JSON Value\n%s"), *Value.Json);
}

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
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseFieldStorage, EOpenPocketBaseFieldStorage)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseFieldState, EOpenPocketBaseFieldState)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseStringComparison, EOpenPocketBaseStringComparison)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseNumberComparison, EOpenPocketBaseNumberComparison)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseBooleanComparison, EOpenPocketBaseBooleanComparison)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseDateComparison, EOpenPocketBaseDateComparison)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseNullComparison, EOpenPocketBaseNullComparison)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseFileDownloadTarget, EOpenPocketBaseFileDownloadTarget)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseJsonRootType, EOpenPocketBaseJsonRootType)
OPENPOCKETBASE_IMPLEMENT_ENUM_STRING(OpenPocketBaseJsonValueType, EOpenPocketBaseJsonValueType)
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
