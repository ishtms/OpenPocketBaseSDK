#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseSchema.h"

class UScriptStruct;

enum class EOpenPocketBaseSchemaReferenceStatus : uint8
{
    Valid,
    Empty,
    MissingSchema,
    StaleSchema,
    MissingCollection,
    MissingField,
    WrongFieldType
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
    static bool AcceptsField(
        const UScriptStruct* ReferenceStruct,
        const FOpenPocketBaseFieldRef& Field);

    static void BuildCollectionChoices(
        const TArray<UOpenPocketBaseSchema*>& Schemas,
        bool bIncludeSystemCollections,
        TArray<FOpenPocketBaseSchemaPickerChoice>& OutChoices);
    static void BuildFieldChoices(
        const TArray<UOpenPocketBaseSchema*>& Schemas,
        const FOpenPocketBaseFieldPickerFilter& Filter,
        TArray<FOpenPocketBaseSchemaPickerChoice>& OutChoices);

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
        const FOpenPocketBaseCollectionRef& Ref,
        FText& OutMessage);
    static EOpenPocketBaseSchemaReferenceStatus ValidateField(
        const UScriptStruct* ReferenceStruct,
        const FOpenPocketBaseFieldRef& Ref,
        FText& OutMessage);
};
