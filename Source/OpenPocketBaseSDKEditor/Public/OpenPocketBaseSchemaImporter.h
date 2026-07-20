#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"

#include "OpenPocketBaseSchemaImporter.generated.h"

class UOpenPocketBaseSchema;

class OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseSchemaImporter
{
public:
    static bool ImportJson(
        const FString& Json,
        const FString& Source,
        UOpenPocketBaseSchema& OutSchema,
        FText& OutError);
};

UCLASS(HideCategories = Object)
class OPENPOCKETBASESDKEDITOR_API UOpenPocketBaseSchemaFactory final
    : public UFactory
    , public FReimportHandler
{
    GENERATED_BODY()

public:
    UOpenPocketBaseSchemaFactory();

    virtual bool FactoryCanImport(const FString& Filename) override;
    virtual FText GetDisplayName() const override;
    virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
    virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
    virtual EReimportResult::Type Reimport(UObject* Obj) override;
    virtual int32 GetPriority() const override;
    virtual UObject* FactoryCreateText(
        UClass* InClass,
        UObject* InParent,
        FName InName,
        EObjectFlags Flags,
        UObject* Context,
        const TCHAR* Type,
        const TCHAR*& Buffer,
        const TCHAR* BufferEnd,
        FFeedbackContext* Warn) override;
};
