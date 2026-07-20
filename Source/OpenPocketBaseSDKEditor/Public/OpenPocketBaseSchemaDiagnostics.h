#pragma once

#include "CoreMinimal.h"

class UOpenPocketBaseSchema;

enum class EOpenPocketBaseSchemaDiagnosticSeverity : uint8
{
    Info,
    Warning,
    Error
};

struct OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseSchemaDiagnostic
{
    EOpenPocketBaseSchemaDiagnosticSeverity Severity =
        EOpenPocketBaseSchemaDiagnosticSeverity::Info;
    FString Message;
};

struct OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseSchemaSummary
{
    int32 CollectionCount = 0;
    int32 BaseCollectionCount = 0;
    int32 AuthCollectionCount = 0;
    int32 ViewCollectionCount = 0;
    int32 FieldCount = 0;
    int32 WritableFieldCount = 0;
    int32 RequiredFieldCount = 0;
    bool bHasSource = false;
    bool bSourceExists = false;
    bool bHasFingerprint = false;

    FString ToText() const;
};

struct OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseSchemaDiff
{
    TArray<FString> AddedCollections;
    TArray<FString> RemovedCollections;
    TArray<FString> RenamedCollections;
    TArray<FString> ChangedCollections;
    TArray<FString> AddedFields;
    TArray<FString> RemovedFields;
    TArray<FString> RenamedFields;
    TArray<FString> ChangedFields;

    bool IsEmpty() const;
    int32 NumChanges() const;
    FString ToText() const;
};

struct OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseSchemaValidationReport
{
    TArray<FOpenPocketBaseSchemaDiagnostic> Issues;

    bool HasErrors() const;
    FString ToText() const;
};

class OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseSchemaDiagnostics
{
public:
    static FOpenPocketBaseSchemaSummary Summarize(const UOpenPocketBaseSchema& Schema);
    static FOpenPocketBaseSchemaDiff Compare(
        const UOpenPocketBaseSchema& Current,
        const UOpenPocketBaseSchema& Candidate);
    static FOpenPocketBaseSchemaValidationReport Validate(
        const UOpenPocketBaseSchema& Schema);
    static bool LoadSourcePreview(
        const UOpenPocketBaseSchema& Schema,
        UOpenPocketBaseSchema*& OutPreview,
        FText& OutError);
};
