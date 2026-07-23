#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseBlueprintClient.h"
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
        meta = (DisplayName = "New Record Body", NativeMakeFunc, ToolTip = "Starts a validated record body for the selected writable collection.", Keywords = "pocketbase record body create make collection"))
    static FOpenPocketBaseRecordBody NewRecordBody(
        UPARAM(meta = (OpenPocketBaseCollectionAccess = "Write")) FOpenPocketBaseCollection Collection);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With String Field", ToolTip = "Returns a copy of the record body with a text field set.", Keywords = "pocketbase record body string text set add"))
    static FOpenPocketBaseRecordBody WithStringField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseTextFieldRef Field,
        const FString& Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Number Field", Keywords = "record body set add"))
    static FOpenPocketBaseRecordBody WithNumberField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseNumberFieldRef Field,
        double Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Boolean Field", Keywords = "record body set add bool true false"))
    static FOpenPocketBaseRecordBody WithBooleanField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseBooleanFieldRef Field,
        bool bValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Null Field", Keywords = "record body set add empty"))
    static FOpenPocketBaseRecordBody WithNullField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseAnyFieldRef Field);

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
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Date Field", Keywords = "record body date time"))
    static FOpenPocketBaseRecordBody WithDateField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseDateFieldRef Field,
        FDateTime Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With JSON Field", ToolTip = "Returns a copy of the record body with any JSON object, array, scalar, or null value set.", Keywords = "pocketbase record body json object array scalar"))
    static FOpenPocketBaseRecordBody WithJsonField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseJsonFieldRef Field,
        FOpenPocketBaseJsonValue Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "Geo Point", NativeMakeFunc, Keywords = "latitude longitude coordinates"))
    static FOpenPocketBaseGeoPoint MakeGeoPoint(double Latitude, double Longitude);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Geo Point Field", Keywords = "record body location coordinates"))
    static FOpenPocketBaseRecordBody WithGeoPointField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseGeoPointFieldRef Field,
        FOpenPocketBaseGeoPoint Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Select Field", Keywords = "record body choice option"))
    static FOpenPocketBaseRecordBody WithSingleSelectField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseSingleSelectFieldRef Field,
        UPARAM(meta = (OpenPocketBaseSelectField = "Field")) const FString& Value);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Multiple Select Field", Keywords = "record body choices options"))
    static FOpenPocketBaseRecordBody WithMultipleSelectField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseMultipleSelectFieldRef Field,
        UPARAM(meta = (OpenPocketBaseSelectField = "Field")) FOpenPocketBaseSelectValues Values,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Relation Record", ToolTip = "Sets a single relation using the ID of a PocketBase record.", Keywords = "pocketbase record body relation related record"))
    static FOpenPocketBaseRecordBody WithRelationRecord(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseSingleRelationFieldRef Field,
        const FOpenPocketBaseRecord& Record);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body",
        meta = (DisplayName = "With Relation Records", Keywords = "record body related records"))
    static FOpenPocketBaseRecordBody WithRelationRecords(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseMultipleRelationFieldRef Field,
        const TArray<FOpenPocketBaseRecord>& Records,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body|Advanced",
        meta = (DisplayName = "With Relation ID (Advanced)", Keywords = "record body related record id"))
    static FOpenPocketBaseRecordBody WithSingleRelationField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseSingleRelationFieldRef Field,
        const FString& RecordId);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Body|Advanced",
        meta = (DisplayName = "With Relation IDs (Advanced)", Keywords = "record body related record ids"))
    static FOpenPocketBaseRecordBody WithMultipleRelationField(
        FOpenPocketBaseRecordBody Body,
        UPARAM(meta = (OpenPocketBaseFieldAccess = "Write")) FOpenPocketBaseMultipleRelationFieldRef Field,
        const TArray<FString>& RecordIds,
        EOpenPocketBaseFieldModifier Modifier = EOpenPocketBaseFieldModifier::Replace);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Body|Advanced", meta = (DisplayName = "With Dynamic String Field"))
    static FOpenPocketBaseRecordBody WithDynamicStringField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        const FString& Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Body|Advanced", meta = (DisplayName = "With Dynamic Number Field"))
    static FOpenPocketBaseRecordBody WithDynamicNumberField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        double Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Body|Advanced", meta = (DisplayName = "With Dynamic Boolean Field"))
    static FOpenPocketBaseRecordBody WithDynamicBooleanField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        bool bValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Body|Advanced", meta = (DisplayName = "With Dynamic Null Field"))
    static FOpenPocketBaseRecordBody WithDynamicNullField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Body|Advanced", meta = (DisplayName = "With Dynamic String Array Field"))
    static FOpenPocketBaseRecordBody WithDynamicStringArrayField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        const TArray<FString>& Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Body|Advanced", meta = (DisplayName = "With Dynamic Date Field"))
    static FOpenPocketBaseRecordBody WithDynamicDateField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        FDateTime Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Body|Advanced", meta = (DisplayName = "With Dynamic JSON Field"))
    static FOpenPocketBaseRecordBody WithDynamicJsonField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        FOpenPocketBaseJsonValue Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Body|Advanced", meta = (DisplayName = "With Dynamic Geo Point Field"))
    static FOpenPocketBaseRecordBody WithDynamicGeoPointField(
        FOpenPocketBaseRecordBody Body,
        const FString& FieldName,
        FOpenPocketBaseGeoPoint Value);

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
        meta = (DisplayName = "With Request Options"))
    static FOpenPocketBaseRecordOptions RecordOptionsWithRequestOptions(
        FOpenPocketBaseRecordOptions Options,
        FOpenPocketBaseRequestOptions RequestOptions);

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
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "Without Totals"))
    static FOpenPocketBaseListOptions ListOptionsWithoutTotals(
        FOpenPocketBaseListOptions Options);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "With Request Options"))
    static FOpenPocketBaseListOptions ListOptionsWithRequestOptions(
        FOpenPocketBaseListOptions Options,
        FOpenPocketBaseRequestOptions RequestOptions);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options",
        meta = (DisplayName = "Full List Options", NativeMakeFunc))
    static FOpenPocketBaseFullListOptions NewFullListOptions(
        int32 PerPage = 30,
        int32 MaxItems = 0,
        int32 MaxPages = 0);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options|Full List",
        meta = (DisplayName = "Where"))
    static FOpenPocketBaseFullListOptions FullListOptionsWhere(
        FOpenPocketBaseFullListOptions Options,
        FOpenPocketBaseFilter Filter);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options|Full List",
        meta = (DisplayName = "Then Sort By"))
    static FOpenPocketBaseFullListOptions FullListOptionsThenSortBy(
        FOpenPocketBaseFullListOptions Options,
        FOpenPocketBaseSort Sort);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options|Full List",
        meta = (DisplayName = "Select Field"))
    static FOpenPocketBaseFullListOptions FullListOptionsSelectField(
        FOpenPocketBaseFullListOptions Options,
        FOpenPocketBaseFieldSelection Field);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options|Full List",
        meta = (DisplayName = "Include Expansion"))
    static FOpenPocketBaseFullListOptions FullListOptionsIncludeExpansion(
        FOpenPocketBaseFullListOptions Options,
        FOpenPocketBaseExpand Expand);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options|Full List",
        meta = (DisplayName = "Without Totals"))
    static FOpenPocketBaseFullListOptions FullListOptionsWithoutTotals(
        FOpenPocketBaseFullListOptions Options);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Options|Full List",
        meta = (DisplayName = "With Request Options"))
    static FOpenPocketBaseFullListOptions FullListOptionsWithRequestOptions(
        FOpenPocketBaseFullListOptions Options,
        FOpenPocketBaseRequestOptions RequestOptions);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (NativeBreakFunc, DisplayName = "Break Open Pocket Base Record Page"))
    static void BreakRecordPage(
        const FOpenPocketBaseRecordPage& RecordPage,
        int32& Page,
        int32& PerPage,
        TArray<FOpenPocketBaseRecord>& Items,
        bool& bHasTotalItems,
        int64& TotalItems,
        bool& bHasTotalPages,
        int32& TotalPages);

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
        meta = (ReturnDisplayName = "Found", ToolTip = "Reads a text-compatible field from the record data. Found is false for missing, null, wrong-type, or wrong-collection values.", Keywords = "pocketbase record get read string text field"))
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
    static bool TryGetJsonField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseJsonFieldRef Field,
        FOpenPocketBaseJsonValue& OutValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records|Advanced",
        meta = (ReturnDisplayName = "Found", DisplayName = "Try Get JSON Object Field (Advanced)"))
    static bool TryGetObjectField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseJsonFieldRef Field,
        FJsonObjectWrapper& OutValue);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Records",
        meta = (ReturnDisplayName = "Found"))
    static bool TryGetGeoPointField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseGeoPointFieldRef Field,
        FOpenPocketBaseGeoPoint& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records", meta = (ReturnDisplayName = "Found"))
    static bool TryGetSingleSelectField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseSingleSelectFieldRef Field,
        FString& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records", meta = (ReturnDisplayName = "Found"))
    static bool TryGetMultipleSelectField(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseMultipleSelectFieldRef Field,
        TArray<FString>& OutValues);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records", meta = (ReturnDisplayName = "Found"))
    static bool TryGetSingleRelationId(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseSingleRelationFieldRef Field,
        FString& OutRecordId);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records", meta = (ReturnDisplayName = "Found"))
    static bool TryGetMultipleRelationIds(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseMultipleRelationFieldRef Field,
        TArray<FString>& OutRecordIds);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Expanded")
    static EOpenPocketBaseFieldState GetExpandedRecordState(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseSingleRelationFieldRef Relation,
        FOpenPocketBaseRecord& OutRecord);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Expanded")
    static EOpenPocketBaseFieldState GetExpandedRecordsState(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseRelationFieldRef Relation,
        TArray<FOpenPocketBaseRecord>& OutRecords);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Records|Expanded")
    static EOpenPocketBaseFieldState FollowExpansionPath(
        const FOpenPocketBaseRecord& Record,
        FOpenPocketBaseExpand Path,
        TArray<FOpenPocketBaseRecord>& OutRecords);

    UFUNCTION(
        BlueprintPure,
        Category = "Open PocketBase|Utilities",
        meta = (ReturnDisplayName = "Parsed"))
    static bool TryParsePocketBaseDate(const FString& Value, FDateTime& OutDateTime);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|Utilities")
    static FString FormatPocketBaseDate(const FDateTime& Value);
};
