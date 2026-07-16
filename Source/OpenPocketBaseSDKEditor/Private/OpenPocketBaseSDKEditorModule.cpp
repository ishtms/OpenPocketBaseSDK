#include "BlueprintCompilationManager.h"
#include "EdGraphUtilities.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "OpenPocketBaseEditorValidation.h"
#include "Schema/OpenPocketBaseSchemaCompilerExtension.h"
#include "Schema/OpenPocketBaseSchemaGraphPinFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogOpenPocketBaseEditor, Log, All);

class FOpenPocketBaseSDKEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        SchemaPinFactory = MakeShared<FOpenPocketBaseSchemaGraphPinFactory>();
        FEdGraphUtilities::RegisterVisualPinFactory(SchemaPinFactory);
        SchemaCompilerExtension = NewObject<UOpenPocketBaseSchemaCompilerExtension>();
        FBlueprintCompilationManager::RegisterCompilerExtension(
            UBlueprint::StaticClass(),
            SchemaCompilerExtension);

        ValidationCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("OpenPocketBase.ValidateProject"),
            TEXT("Validates Open PocketBase project settings and credential boundaries."),
            FConsoleCommandDelegate::CreateRaw(this, &FOpenPocketBaseSDKEditorModule::ValidateProject),
            ECVF_Default);

        FOpenPocketBaseEditorValidationReport CommandLineReport;
        FOpenPocketBaseEditorValidator::ScanCommandLine(FCommandLine::Get(), CommandLineReport);
        if (CommandLineReport.HasErrors())
        {
            UE_LOG(
                LogOpenPocketBaseEditor,
                Error,
                TEXT("Open PocketBase found privileged credential material on the process command line. "
                     "Remove it and use a process-local prompt or secret provider."));
        }
    }

    virtual void ShutdownModule() override
    {
        if (SchemaPinFactory.IsValid())
        {
            FEdGraphUtilities::UnregisterVisualPinFactory(SchemaPinFactory);
            SchemaPinFactory.Reset();
        }

        if (ValidationCommand != nullptr)
        {
            IConsoleManager::Get().UnregisterConsoleObject(ValidationCommand, false);
            ValidationCommand = nullptr;
        }
    }

private:
    void ValidateProject() const
    {
        const FOpenPocketBaseEditorValidationReport Report =
            FOpenPocketBaseEditorValidator::ValidateCurrentProject();
        if (Report.Issues.IsEmpty() && !Report.bIssueLimitReached)
        {
            UE_LOG(LogOpenPocketBaseEditor, Display, TEXT("Open PocketBase project validation passed."));
            return;
        }

        UE_LOG(
            LogOpenPocketBaseEditor,
            Warning,
            TEXT("Open PocketBase project validation found %d issue(s)."),
            Report.Issues.Num());
        for (const FOpenPocketBaseEditorValidationIssue& Issue : Report.Issues)
        {
            UE_LOG(LogOpenPocketBaseEditor, Warning, TEXT("%s"), *Issue.Message);
        }
    }

    IConsoleObject* ValidationCommand = nullptr;
    TSharedPtr<FGraphPanelPinFactory> SchemaPinFactory;
    UOpenPocketBaseSchemaCompilerExtension* SchemaCompilerExtension = nullptr;
};

IMPLEMENT_MODULE(FOpenPocketBaseSDKEditorModule, OpenPocketBaseSDKEditor)
