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
    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static FString MakeExcerptField(
        const FString& FieldName,
        int32 MaxLength,
        bool bWithEllipsis = false);

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
        const FString& FieldName,
        const FString& Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Number Field", Keywords = "record body set add"))
    static FOpenPocketBaseRecordBody WithNumberField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        double Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Boolean Field", Keywords = "record body set add bool true false"))
    static FOpenPocketBaseRecordBody WithBooleanField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        bool bValue,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Null Field", Keywords = "record body set add empty"))
    static FOpenPocketBaseRecordBody WithNullField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With String Array Field", Keywords = "record body set add list"))
    static FOpenPocketBaseRecordBody WithStringArrayField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        const TArray<FString>& Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static bool HasField(const FOpenPocketBaseRecord& Record, const FString& FieldName);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static bool IsFieldNull(const FOpenPocketBaseRecord& Record, const FString& FieldName);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static bool TryGetStringField(
        const FOpenPocketBaseRecord& Record,
        const FString& FieldName,
        FString& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static EOpenPocketBaseFieldState GetStringFieldState(
        const FOpenPocketBaseRecord& Record,
        const FString& FieldName,
        FString& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static bool TryGetIntegerField(
        const FOpenPocketBaseRecord& Record,
        const FString& FieldName,
        int64& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static EOpenPocketBaseFieldState GetIntegerFieldState(
        const FOpenPocketBaseRecord& Record,
        const FString& FieldName,
        int64& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static bool TryGetNumberField(
        const FOpenPocketBaseRecord& Record,
        const FString& FieldName,
        double& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static bool TryGetBooleanField(
        const FOpenPocketBaseRecord& Record,
        const FString& FieldName,
        bool& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static bool TryGetDateField(
        const FOpenPocketBaseRecord& Record,
        const FString& FieldName,
        FDateTime& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static bool TryGetStringArrayField(
        const FOpenPocketBaseRecord& Record,
        const FString& FieldName,
        TArray<FString>& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records")
    static bool TryGetObjectField(
        const FOpenPocketBaseRecord& Record,
        const FString& FieldName,
        FJsonObjectWrapper& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities")
    static bool TryParsePocketBaseDate(const FString& Value, FDateTime& OutDateTime);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities")
    static FString FormatPocketBaseDate(const FDateTime& Value);
};
