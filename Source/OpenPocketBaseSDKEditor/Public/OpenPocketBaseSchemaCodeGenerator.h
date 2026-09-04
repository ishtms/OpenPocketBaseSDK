// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"

class UOpenPocketBaseSchema;

class OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseSchemaCodeGenerator
{
public:
    static bool GenerateHeader(
        const UOpenPocketBaseSchema& Schema,
        const FString& RootNamespace,
        FString& OutHeader,
        FText& OutError);

    static bool FindDefaultOutputPath(
        const UOpenPocketBaseSchema& Schema,
        FString& OutProjectRelativePath,
        FText& OutError);

    static bool WriteHeader(
        const UOpenPocketBaseSchema& Schema,
        const FString& RootNamespace,
        const FString& OutputPath,
        FString& OutAbsolutePath,
        FText& OutError);
};
