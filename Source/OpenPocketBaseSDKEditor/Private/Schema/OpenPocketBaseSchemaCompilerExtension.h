#pragma once

#include "BlueprintCompilerExtension.h"

#include "OpenPocketBaseSchemaCompilerExtension.generated.h"

UCLASS()
class UOpenPocketBaseSchemaCompilerExtension final : public UBlueprintCompilerExtension
{
    GENERATED_BODY()

protected:
    virtual void ProcessBlueprintCompiled(
        const FKismetCompilerContext& CompilationContext,
        const FBlueprintCompiledData& Data) override;
};
