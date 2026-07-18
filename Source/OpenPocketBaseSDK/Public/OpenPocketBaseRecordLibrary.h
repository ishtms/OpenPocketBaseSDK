#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseRecord.h"

#include "OpenPocketBaseRecordLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseRecordLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "New Record Body", NativeMakeFunc))
    static FOpenPocketBaseRecordBody NewRecordBody();

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With String Field", Keywords = "record body set add"))
    static FOpenPocketBaseRecordBody WithStringField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseStringFieldRef Field,
        const FString& Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Number Field", Keywords = "record body set add"))
    static FOpenPocketBaseRecordBody WithNumberField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseNumberFieldRef Field,
        double Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Boolean Field", Keywords = "record body set add bool true false"))
    static FOpenPocketBaseRecordBody WithBooleanField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseBooleanFieldRef Field,
        bool bValue,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Null Field", Keywords = "record body set add empty"))
    static FOpenPocketBaseRecordBody WithNullField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseAnyFieldRef Field,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With String Array Field", Keywords = "record body set add list"))
    static FOpenPocketBaseRecordBody WithStringArrayField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseStringArrayFieldRef Field,
        const TArray<FString>& Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "Record Options", NativeMakeFunc))
    static FOpenPocketBaseRecordOptions NewRecordOptions();

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "Select Field"))
    static FOpenPocketBaseRecordOptions RecordOptionsSelectField(
        FOpenPocketBaseRecordOptions Options,
        FOpenPocketBaseFieldSelection Field);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "Include Expansion"))
    static FOpenPocketBaseRecordOptions RecordOptionsIncludeExpansion(
        FOpenPocketBaseRecordOptions Options,
        FOpenPocketBaseExpand Expand);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "List Options", NativeMakeFunc))
    static FOpenPocketBaseListOptions NewListOptions(int32 Page = 1, int32 PerPage = 30);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "Where"))
    static FOpenPocketBaseListOptions ListOptionsWhere(
        FOpenPocketBaseListOptions Options,
        FOpenPocketBaseFilter Filter);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "Then Sort By"))
    static FOpenPocketBaseListOptions ListOptionsThenSortBy(
        FOpenPocketBaseListOptions Options,
        FOpenPocketBaseSort Sort);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "Select Field"))
    static FOpenPocketBaseListOptions ListOptionsSelectField(
        FOpenPocketBaseListOptions Options,
        FOpenPocketBaseFieldSelection Field);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "Include Expansion"))
    static FOpenPocketBaseListOptions ListOptionsIncludeExpansion(
        FOpenPocketBaseListOptions Options,
        FOpenPocketBaseExpand Expand);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (ReturnDisplayName = "Has Field"))
    static bool HasField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseAnyFieldRef Field);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (ReturnDisplayName = "Is Null"))
    static bool IsFieldNull(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseAnyFieldRef Field);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (ReturnDisplayName = "Found"))
    static bool TryGetStringField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseStringFieldRef Field,
        FString& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static EOpenPocketBaseFieldState GetStringFieldState(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseStringFieldRef Field,
        FString& OutValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (ReturnDisplayName = "Found"))
    static bool TryGetIntegerField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseNumberFieldRef Field,
        int64& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static EOpenPocketBaseFieldState GetIntegerFieldState(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseNumberFieldRef Field,
        int64& OutValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (ReturnDisplayName = "Found"))
    static bool TryGetNumberField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseNumberFieldRef Field,
        double& OutValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (ReturnDisplayName = "Found"))
    static bool TryGetBooleanField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseBooleanFieldRef Field,
        bool& OutValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (ReturnDisplayName = "Found"))
    static bool TryGetDateField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseDateFieldRef Field,
        FDateTime& OutValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (ReturnDisplayName = "Found"))
    static bool TryGetStringArrayField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseStringArrayFieldRef Field,
        TArray<FString>& OutValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (ReturnDisplayName = "Found"))
    static bool TryGetObjectField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseJsonFieldRef Field,
        FJsonObjectWrapper& OutValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Utilities",
        meta = (ReturnDisplayName = "Parsed"))
    static bool TryParsePocketBaseDate(const FString& Value, FDateTime& OutDateTime);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities")
    static FString FormatPocketBaseDate(const FDateTime& Value);
};
