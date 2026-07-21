#include "OpenPocketBaseBlueprintMetadata.h"

#include "Engine/CancellableAsyncAction.h"
#include "UObject/UObjectIterator.h"

namespace
{
FString BuildFunctionTooltip(const UFunction& Function)
{
    const FString DisplayName = Function.GetDisplayNameText().ToString();
    const FString Category = Function.GetMetaData(TEXT("Category"));
    const UClass* OwnerClass = Function.GetOuterUClass();
    if (OwnerClass != nullptr && OwnerClass->IsChildOf(UCancellableAsyncAction::StaticClass()))
    {
        return FString::Printf(
            TEXT("Starts the PocketBase %s request. Handle its success, failure, cancellation, and any operation-specific outputs."),
            *DisplayName);
    }
    if (Category.Contains(TEXT("Records|Body")))
    {
        return FString::Printf(
            TEXT("Builds a PocketBase record body value using %s."),
            *DisplayName);
    }
    if (Category.Contains(TEXT("Filters")))
    {
        return FString::Printf(
            TEXT("Builds a typed PocketBase record filter using %s."),
            *DisplayName);
    }
    if (Category.Contains(TEXT("Query")))
    {
        return FString::Printf(
            TEXT("Builds a typed PocketBase query value using %s."),
            *DisplayName);
    }
    if (Category.Contains(TEXT("Options")))
    {
        return FString::Printf(
            TEXT("Builds PocketBase request options using %s."),
            *DisplayName);
    }
    if (Category.Contains(TEXT("Batch")))
    {
        return FString::Printf(
            TEXT("Builds or sends a PocketBase transaction batch using %s."),
            *DisplayName);
    }
    if (Category.Contains(TEXT("File")))
    {
        return FString::Printf(
            TEXT("Builds or runs the PocketBase file workflow for %s."),
            *DisplayName);
    }
    if (Category.Contains(TEXT("Realtime")))
    {
        return FString::Printf(
            TEXT("Builds or runs the PocketBase realtime workflow for %s."),
            *DisplayName);
    }
    if (Category.Contains(TEXT("Authentication")) || Category.Contains(TEXT("Session")))
    {
        return FString::Printf(
            TEXT("Reads or updates PocketBase authentication state using %s."),
            *DisplayName);
    }
    if (Category.Contains(TEXT("Admin")))
    {
        return FString::Printf(
            TEXT("Builds or runs the privileged PocketBase admin workflow for %s."),
            *DisplayName);
    }
    if (Function.HasAnyFunctionFlags(FUNC_BlueprintPure))
    {
        return FString::Printf(
            TEXT("Returns %s for the current PocketBase value or client."),
            *DisplayName);
    }
    return FString::Printf(TEXT("Runs the PocketBase %s operation."), *DisplayName);
}

FString BuildKeywords(const UFunction& Function)
{
    FString Category = Function.GetMetaData(TEXT("Category"));
    Category.ReplaceInline(TEXT("|"), TEXT(" "));
    return FString::Printf(
        TEXT("pocketbase %s %s"),
        *FName::NameToDisplayString(Function.GetName(), false),
        *Category).TrimStartAndEnd();
}

FString ParameterTooltip(const FName Name)
{
    if (Name == TEXT("PocketBaseClient"))
    {
        return TEXT("The initialized PocketBase client that owns this operation.");
    }
    if (Name == TEXT("Collection"))
    {
        return TEXT("The schema-backed PocketBase collection used by this operation.");
    }
    if (Name == TEXT("Field"))
    {
        return TEXT("The schema-backed PocketBase field used by this value.");
    }
    if (Name == TEXT("Body"))
    {
        return TEXT("The immutable PocketBase record body sent by this operation.");
    }
    if (Name == TEXT("Options"))
    {
        return TEXT("Request behavior, limits, and query options for this operation.");
    }
    if (Name == TEXT("Record"))
    {
        return TEXT("The PocketBase record used by this operation.");
    }
    if (Name == TEXT("RecordId"))
    {
        return TEXT("The PocketBase record ID.");
    }
    if (Name == TEXT("OutError"))
    {
        return TEXT("Receives a structured error when the operation cannot be prepared.");
    }
    return {};
}
}

void FOpenPocketBaseBlueprintMetadata::Apply(const FString& ScriptPackage)
{
    for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
    {
        UClass* Class = *ClassIt;
        if (Class == nullptr || !Class->HasAnyClassFlags(CLASS_Native) ||
            Class->GetOutermost()->GetName() != ScriptPackage)
        {
            continue;
        }

        for (TFieldIterator<UFunction> FunctionIt(
                 Class,
                 EFieldIteratorFlags::ExcludeSuper);
             FunctionIt;
             ++FunctionIt)
        {
            UFunction* Function = *FunctionIt;
            if (Function == nullptr ||
                !Function->HasAnyFunctionFlags(FUNC_BlueprintCallable) ||
                Function->HasMetaData(TEXT("DeprecatedFunction")))
            {
                continue;
            }

            if (Function->GetMetaData(TEXT("ToolTip")).TrimStartAndEnd().IsEmpty())
            {
                Function->SetMetaData(TEXT("ToolTip"), *BuildFunctionTooltip(*Function));
            }
            if (Function->GetMetaData(TEXT("Keywords")).TrimStartAndEnd().IsEmpty())
            {
                Function->SetMetaData(TEXT("Keywords"), *BuildKeywords(*Function));
            }

            for (TFieldIterator<FProperty> PropertyIt(Function); PropertyIt; ++PropertyIt)
            {
                FProperty* Property = *PropertyIt;
                if (Property == nullptr || !Property->HasAnyPropertyFlags(CPF_Parm) ||
                    Property->HasAnyPropertyFlags(CPF_ReturnParm) ||
                    !Property->GetMetaData(TEXT("ToolTip")).TrimStartAndEnd().IsEmpty())
                {
                    continue;
                }

                const FString Tooltip = ParameterTooltip(Property->GetFName());
                if (!Tooltip.IsEmpty())
                {
                    Property->SetMetaData(TEXT("ToolTip"), *Tooltip);
                }
            }
        }
    }
}
