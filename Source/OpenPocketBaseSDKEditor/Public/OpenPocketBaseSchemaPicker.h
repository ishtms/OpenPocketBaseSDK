#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseSchema.h"

class UScriptStruct;
class UEdGraphPin;
class UOpenPocketBaseProjectSettings;

enum class EOpenPocketBaseSchemaReferenceStatus : uint8
{
    Valid,
    Empty,
    MissingSchema,
    StaleSchema,
    MissingCollection,
    MissingField,
    WrongCollectionType,
    WrongFieldType,
    ReadOnlyField
};

enum class EOpenPocketBaseCollectionRequirement : uint8
{
    Any,
    Writable,
    Auth
};

struct FOpenPocketBaseSchemaPickerChoice
{
    FText Label;
    FText Detail;
    FString SearchText;
    FOpenPocketBaseCollectionRef Collection;
    FOpenPocketBaseFieldRef Field;
};

struct FOpenPocketBaseFieldPickerFilter
{
    const FOpenPocketBaseCollectionRef* Collection = nullptr;
    const UScriptStruct* ReferenceStruct = nullptr;
    bool bWritableOnly = false;
    bool bIncludeHidden = false;
};

class OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseSchemaPickerModel
{
public:
    static bool SupportsCollectionStruct(const UScriptStruct* Struct);
    static bool SupportsFieldStruct(const UScriptStruct* Struct);
    static bool AcceptsCollection(
        const UScriptStruct* ReferenceStruct,
        const FOpenPocketBaseCollectionRef& Collection);
    static bool AcceptsField(
        const UScriptStruct* ReferenceStruct,
        const FOpenPocketBaseFieldRef& Field);

    static void BuildCollectionChoices(
        const TArray<UOpenPocketBaseSchema*>& Schemas,
        const UScriptStruct* ReferenceStruct,
        bool bIncludeSystemCollections,
        EOpenPocketBaseCollectionRequirement Requirement,
        TArray<FOpenPocketBaseSchemaPickerChoice>& OutChoices);
    static void BuildFieldChoices(
        const TArray<UOpenPocketBaseSchema*>& Schemas,
        const FOpenPocketBaseFieldPickerFilter& Filter,
        TArray<FOpenPocketBaseSchemaPickerChoice>& OutChoices);
    static void ChooseProfileSchemas(
        const UOpenPocketBaseProjectSettings& Settings,
        const TArray<UOpenPocketBaseSchema*>& AvailableSchemas,
        TArray<UOpenPocketBaseSchema*>& OutSchemas);

    static bool ParseCollectionDefault(
        const FString& DefaultValue,
        FOpenPocketBaseCollectionRef& OutRef);
    static bool ParseFieldDefault(
        const UScriptStruct* ReferenceStruct,
        const FString& DefaultValue,
        FOpenPocketBaseFieldRef& OutRef);
    static FString ExportCollectionDefault(const FOpenPocketBaseCollectionRef& Ref);
    static FString ExportFieldDefault(
        const UScriptStruct* ReferenceStruct,
        const FOpenPocketBaseFieldRef& Ref);

    static EOpenPocketBaseSchemaReferenceStatus ValidateCollection(
        const UScriptStruct* ReferenceStruct,
        const FOpenPocketBaseCollectionRef& Ref,
        EOpenPocketBaseCollectionRequirement Requirement,
        FText& OutMessage);
    static EOpenPocketBaseSchemaReferenceStatus ValidateField(
        const UScriptStruct* ReferenceStruct,
        const FOpenPocketBaseFieldRef& Ref,
        bool bWritableOnly,
        FText& OutMessage);
    static bool ResolveFieldFromPinContext(
        const UEdGraphPin& OriginPin,
        const FString& ContextPinName,
        FOpenPocketBaseFieldRef& OutField);
};
