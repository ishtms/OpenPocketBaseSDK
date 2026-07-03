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

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records")
    static void SetRecordBodyStringField(
        UPARAM(ref) FOpenPocketBaseRecordBody& Body,
        const FString& FieldName,
        const FString& Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records")
    static void SetRecordBodyNumberField(
        UPARAM(ref) FOpenPocketBaseRecordBody& Body,
        const FString& FieldName,
        double Value,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records")
    static void SetRecordBodyBooleanField(
        UPARAM(ref) FOpenPocketBaseRecordBody& Body,
        const FString& FieldName,
        bool bValue,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records")
    static void SetRecordBodyNullField(
        UPARAM(ref) FOpenPocketBaseRecordBody& Body,
        const FString& FieldName,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(BlueprintCallable, Category = "Open PocketBase|Records")
    static void SetRecordBodyStringArrayField(
        UPARAM(ref) FOpenPocketBaseRecordBody& Body,
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
