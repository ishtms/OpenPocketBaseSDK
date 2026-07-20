#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenPocketBaseJsonValue.h"

#include "OpenPocketBaseJsonValueLibrary.generated.h"

UCLASS()
class OPENPOCKETBASESDK_API UOpenPocketBaseJsonValueLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (DisplayName = "JSON Object", NativeMakeFunc, ToolTip = "Creates an empty JSON object that can be extended with Set JSON Property.", Keywords = "pocketbase json make object dictionary"))
    static FOpenPocketBaseJsonValue MakeJsonObject();

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (DisplayName = "JSON Array", NativeMakeFunc, ToolTip = "Creates an empty JSON array that can be extended with Add JSON Item.", Keywords = "pocketbase json make array list"))
    static FOpenPocketBaseJsonValue MakeJsonArray();

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (DisplayName = "JSON String", ToolTip = "Creates a JSON string value.", Keywords = "pocketbase json text value"))
    static FOpenPocketBaseJsonValue JsonString(const FString& Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (DisplayName = "JSON Number", ToolTip = "Creates a JSON number value.", Keywords = "pocketbase json number float value"))
    static FOpenPocketBaseJsonValue JsonNumber(double Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (DisplayName = "JSON Boolean", ToolTip = "Creates a JSON true or false value.", Keywords = "pocketbase json boolean bool true false"))
    static FOpenPocketBaseJsonValue JsonBoolean(bool bValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (DisplayName = "JSON Null", ToolTip = "Creates a JSON null value.", Keywords = "pocketbase json null empty"))
    static FOpenPocketBaseJsonValue JsonNull();

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (DisplayName = "Set JSON Property", ToolTip = "Returns a copy of a JSON object with the named property set.", Keywords = "pocketbase json object field key set add"))
    static FOpenPocketBaseJsonValue SetJsonProperty(
        FOpenPocketBaseJsonValue Object,
        const FString& Name,
        FOpenPocketBaseJsonValue Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (DisplayName = "Add JSON Item", ToolTip = "Returns a copy of a JSON array with the value appended.", Keywords = "pocketbase json array item append add"))
    static FOpenPocketBaseJsonValue AddJsonItem(
        FOpenPocketBaseJsonValue Array,
        FOpenPocketBaseJsonValue Value);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (DisplayName = "Parse JSON", ReturnDisplayName = "Parsed", ToolTip = "Parses an object, array, scalar, or null JSON value.", Keywords = "pocketbase json parse deserialize"))
    static bool ParseJson(
        const FString& Json,
        FOpenPocketBaseJsonValue& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (ReturnDisplayName = "Found", ToolTip = "Reads a property from a JSON object.", Keywords = "pocketbase json object get property key"))
    static bool TryGetJsonProperty(
        FOpenPocketBaseJsonValue Object,
        const FString& Name,
        FOpenPocketBaseJsonValue& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (ReturnDisplayName = "Found", ToolTip = "Reads an item from a JSON array by index.", Keywords = "pocketbase json array get item index"))
    static bool TryGetJsonArrayItem(
        FOpenPocketBaseJsonValue Array,
        int32 Index,
        FOpenPocketBaseJsonValue& OutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (ReturnDisplayName = "Found", ToolTip = "Reads a JSON string without converting other JSON types.", Keywords = "pocketbase json get string text"))
    static bool TryGetJsonString(
        FOpenPocketBaseJsonValue Value,
        FString& OutString);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (ReturnDisplayName = "Found", ToolTip = "Reads a JSON number without converting other JSON types.", Keywords = "pocketbase json get number float"))
    static bool TryGetJsonNumber(
        FOpenPocketBaseJsonValue Value,
        double& OutNumber);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (ReturnDisplayName = "Found", ToolTip = "Reads a JSON Boolean without converting other JSON types.", Keywords = "pocketbase json get boolean bool"))
    static bool TryGetJsonBoolean(
        FOpenPocketBaseJsonValue Value,
        bool& bOutValue);

    UFUNCTION(BlueprintPure, Category = "Open PocketBase|JSON", meta = (ReturnDisplayName = "Found", ToolTip = "Returns the number of items in a JSON array.", Keywords = "pocketbase json array length count size"))
    static bool TryGetJsonArrayLength(
        FOpenPocketBaseJsonValue Array,
        int32& OutLength);
};
