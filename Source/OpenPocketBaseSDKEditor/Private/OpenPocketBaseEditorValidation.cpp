#include "OpenPocketBaseEditorValidation.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "K2Node_BaseAsyncTask.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ModuleDescriptor.h"
#include "Modules/ModuleManager.h"
#include "OpenPocketBaseCapability.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseProjectSettings.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr int64 MaximumScannedFileBytes = 256 * 1024;
constexpr int32 MaximumScannedFiles = 1024;
constexpr int64 MaximumAggregateScanBytes = 8 * 1024 * 1024;

FString SanitizeIdentifier(FStringView Identifier)
{
    FString SafeIdentifier(Identifier.Left(256));
    SafeIdentifier.ReplaceInline(TEXT("\r"), TEXT("_"));
    SafeIdentifier.ReplaceInline(TEXT("\n"), TEXT("_"));
    SafeIdentifier.ReplaceInline(TEXT("\t"), TEXT("_"));
    return SafeIdentifier;
}

const TCHAR* CodeName(const EOpenPocketBaseEditorValidationCode Code)
{
    switch (Code)
    {
    case EOpenPocketBaseEditorValidationCode::InvalidProjectSettings:
        return TEXT("InvalidProjectSettings");
    case EOpenPocketBaseEditorValidationCode::MissingOfflineModule:
        return TEXT("MissingOfflineModule");
    case EOpenPocketBaseEditorValidationCode::MissingPrivilegedModule:
        return TEXT("MissingPrivilegedModule");
    case EOpenPocketBaseEditorValidationCode::StreamingUnavailable:
        return TEXT("StreamingUnavailable");
    case EOpenPocketBaseEditorValidationCode::SecurePersistenceUnavailable:
        return TEXT("SecurePersistenceUnavailable");
    case EOpenPocketBaseEditorValidationCode::OAuthCallbackUnavailable:
        return TEXT("OAuthCallbackUnavailable");
    case EOpenPocketBaseEditorValidationCode::MissingPrivilegedShippingGate:
        return TEXT("MissingPrivilegedShippingGate");
    case EOpenPocketBaseEditorValidationCode::EmbeddedSuperuserCredential:
        return TEXT("EmbeddedSuperuserCredential");
    case EOpenPocketBaseEditorValidationCode::ScanLimitReached:
        return TEXT("ScanLimitReached");
    default:
        return TEXT("Unknown");
    }
}

const TCHAR* SeverityName(const EOpenPocketBaseEditorValidationSeverity Severity)
{
    switch (Severity)
    {
    case EOpenPocketBaseEditorValidationSeverity::Information:
        return TEXT("Information");
    case EOpenPocketBaseEditorValidationSeverity::Warning:
        return TEXT("Warning");
    case EOpenPocketBaseEditorValidationSeverity::Error:
        return TEXT("Error");
    default:
        return TEXT("Unknown");
    }
}

void AddIssue(
    FOpenPocketBaseEditorValidationReport& Report,
    const EOpenPocketBaseEditorValidationCode Code,
    const FString& Message,
    FStringView Context = FStringView())
{
    FOpenPocketBaseEditorValidationIssue Issue;
    Issue.Severity = EOpenPocketBaseEditorValidationSeverity::Error;
    Issue.Code = Code;
    Issue.Message = Message;
    Issue.Context = SanitizeIdentifier(Context);
    Report.Add(MoveTemp(Issue));
}

bool IsPrivilegedPasswordName(FStringView Name)
{
    FString LowerName(Name);
    LowerName.ToLowerInline();
    return LowerName.Contains(TEXT("password")) &&
        (LowerName.Contains(TEXT("superuser")) || LowerName.Contains(TEXT("admin")));
}

bool ContainsCredentialAssignment(FStringView Contents, const bool bRequireQuotedLiteral)
{
    TArray<FString> Lines;
    FString(Contents).ParseIntoArrayLines(Lines, false);
    for (FString& Line : Lines)
    {
        Line.ToLowerInline();
        const bool bPrivileged = Line.Contains(TEXT("superuser")) ||
            Line.Contains(TEXT("authenticate_superuser")) ||
            Line.Contains(TEXT("adminpassword")) ||
            Line.Contains(TEXT("admin_password"));
        const bool bPassword = Line.Contains(TEXT("password"));
        const bool bAssignment = Line.Contains(TEXT("="));
        const bool bLiteral = Line.Contains(TEXT("\"")) || Line.Contains(TEXT("'"));
        if (bPrivileged && bPassword && bAssignment && (!bRequireQuotedLiteral || bLiteral))
        {
            return true;
        }
    }

    FString LowerContents(Contents.Left(1024 * 1024));
    LowerContents.ToLowerInline();
    int32 SearchFrom = 0;
    while (SearchFrom < LowerContents.Len())
    {
        const int32 CallIndex = LowerContents.Find(
            TEXT("authenticatesuperuser"),
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            SearchFrom);
        if (CallIndex == INDEX_NONE)
        {
            break;
        }
        const FStringView CallWindow = FStringView(LowerContents).Mid(
            CallIndex,
            FMath::Min(1024, LowerContents.Len() - CallIndex));
        if (CallWindow.Contains(TEXT("text(\"")) || CallWindow.Contains(TEXT("(\"")))
        {
            return true;
        }
        SearchFrom = CallIndex + 1;
    }
    return false;
}

bool IsPrivilegedFactory(const UFunction* Function)
{
    if (Function == nullptr || Function->GetFName() != TEXT("AuthenticateSuperuser"))
    {
        return false;
    }
    const UPackage* Package = Function->GetOutermost();
    return Package != nullptr && Package->GetName().Contains(TEXT("OpenPocketBaseSDKAdmin"));
}

bool HasEmbeddedCredentialProperty(const UObject* Object)
{
    if (Object == nullptr)
    {
        return false;
    }
    for (TFieldIterator<FStrProperty> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        if (IsPrivilegedPasswordName(It->GetName()) && !It->GetPropertyValue_InContainer(Object).IsEmpty())
        {
            return true;
        }
    }
    return false;
}

bool HasPrivilegedShippingGate()
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OpenPocketBaseSDK"));
    if (!Plugin.IsValid())
    {
        return false;
    }
    for (const FModuleDescriptor& Module : Plugin->GetDescriptor().Modules)
    {
        if (Module.Name == TEXT("OpenPocketBaseSDKAdmin"))
        {
            return Module.TargetConfigurationDenyList.Contains(EBuildConfiguration::Shipping);
        }
    }
    return false;
}

FOpenPocketBaseEditorValidationEnvironment BuildEnvironment(
    const UOpenPocketBaseProjectSettings& Settings)
{
    FOpenPocketBaseEditorValidationEnvironment Environment;
    Environment.bOfflineModuleAvailable =
        FModuleManager::Get().ModuleExists(TEXT("OpenPocketBaseSDKOffline"));
    Environment.bPrivilegedModuleAvailable =
        FModuleManager::Get().ModuleExists(TEXT("OpenPocketBaseSDKAdmin"));
    Environment.bPrivilegedShippingGatePresent = HasPrivilegedShippingGate();

    FName CapabilityProfile = Settings.DefaultProfile;
    for (const FOpenPocketBaseProjectProfile& Profile : Settings.Profiles)
    {
        if (Profile.bEnableAssistedOAuth)
        {
            CapabilityProfile = Profile.Name;
            break;
        }
    }

    FOpenPocketBaseClientConfig Config;
    FOpenPocketBaseError Error;
    if (!Settings.TryResolveProfile(CapabilityProfile, Config, Error))
    {
        return Environment;
    }

    FOpenPocketBaseClientResult ClientResult = FOpenPocketBaseClient::Create(Config);
    if (!ClientResult.IsSuccess())
    {
        Environment.bHttpStreamingAvailable = false;
        Environment.bSecurePersistenceAvailable = false;
        Environment.bOAuthCallbackAvailable = false;
        return Environment;
    }
    FOpenPocketBaseClientRef Client = ClientResult.TakeValue();

    Environment.bHttpStreamingAvailable = Client->GetCapability(
        EOpenPocketBaseCapability::HttpStreaming).IsSupported();
    Environment.bSecurePersistenceAvailable = Client->GetCapability(
        EOpenPocketBaseCapability::SecurePersistence).IsSupported();
    Environment.bOAuthCallbackAvailable = Client->GetCapability(
        EOpenPocketBaseCapability::OAuthCallback).IsSupported();
    Client->Shutdown();
    return Environment;
}

void ScanDirectory(
    const FString& Directory,
    const EOpenPocketBaseEditorArtifactKind Kind,
    const TArray<FString>& Extensions,
    int32& InOutFileCount,
    int64& InOutAggregateBytes,
    FOpenPocketBaseEditorValidationReport& OutReport)
{
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*"), true, false, false);
    Files.Sort();
    for (const FString& File : Files)
    {
        const FString Extension = FPaths::GetExtension(File, true).ToLower();
        if (!Extensions.Contains(Extension))
        {
            continue;
        }
        if (InOutFileCount >= MaximumScannedFiles ||
            InOutAggregateBytes >= MaximumAggregateScanBytes)
        {
            AddIssue(
                OutReport,
                EOpenPocketBaseEditorValidationCode::ScanLimitReached,
                TEXT("Credential scanning stopped after reaching its file or byte safety limit. "
                     "Remove unnecessary source files from the scan or review the remaining files manually."));
            return;
        }

        ++InOutFileCount;
        const int64 FileSize = IFileManager::Get().FileSize(*File);
        if (FileSize < 0 || FileSize > MaximumScannedFileBytes ||
            InOutAggregateBytes + FileSize > MaximumAggregateScanBytes)
        {
            continue;
        }

        FString Contents;
        if (!FFileHelper::LoadFileToString(Contents, *File))
        {
            continue;
        }
        InOutAggregateBytes += FileSize;
        FString RelativeIdentifier = File;
        FPaths::MakePathRelativeTo(RelativeIdentifier, *FPaths::ProjectDir());
        FOpenPocketBaseEditorValidator::ScanTextArtifact(
            Kind,
            RelativeIdentifier,
            Contents,
            OutReport);
    }
}
}

void FOpenPocketBaseEditorValidationReport::Add(FOpenPocketBaseEditorValidationIssue&& Issue)
{
    if (Issues.Num() >= MaximumIssues)
    {
        bIssueLimitReached = true;
        return;
    }
    Issues.Add(MoveTemp(Issue));
}

bool FOpenPocketBaseEditorValidationReport::HasErrors() const
{
    for (const FOpenPocketBaseEditorValidationIssue& Issue : Issues)
    {
        if (Issue.Severity == EOpenPocketBaseEditorValidationSeverity::Error)
        {
            return true;
        }
    }
    return false;
}

FString FOpenPocketBaseEditorValidationReport::ToSanitizedText() const
{
    FString Output;
    for (const FOpenPocketBaseEditorValidationIssue& Issue : Issues)
    {
        Output += FString::Printf(
            TEXT("[%s] %s: %s"),
            SeverityName(Issue.Severity),
            CodeName(Issue.Code),
            *Issue.Message);
        if (!Issue.Context.IsEmpty())
        {
            Output += TEXT(" [") + Issue.Context + TEXT("]");
        }
        Output += TEXT("\n");
    }
    if (bIssueLimitReached)
    {
        Output += TEXT("[Warning] ScanLimitReached: Additional validation issues were omitted.\n");
    }
    return Output;
}

FOpenPocketBaseEditorValidationReport FOpenPocketBaseEditorValidator::ValidateSettings(
    const UOpenPocketBaseProjectSettings& Settings,
    const FOpenPocketBaseEditorValidationEnvironment& Environment)
{
    FOpenPocketBaseEditorValidationReport Report;
    FOpenPocketBaseClientConfig Config;
    FOpenPocketBaseError Error;
    if (!Settings.TryResolveProfile(NAME_None, Config, Error))
    {
        AddIssue(
            Report,
            EOpenPocketBaseEditorValidationCode::InvalidProjectSettings,
            FString::Printf(
                TEXT("Open PocketBase project settings are invalid. %s"),
                *Error.Message));
    }

    bool bRequiresSecurePersistence = false;
    bool bRequiresOAuthCallback = false;
    for (const FOpenPocketBaseProjectProfile& Profile : Settings.Profiles)
    {
        bRequiresSecurePersistence |=
            Profile.SessionPersistence == EOpenPocketBaseSessionPersistence::RequireSecureStorage;
        bRequiresOAuthCallback |= Profile.bEnableAssistedOAuth;
    }

    if ((Settings.bRequireRealtimeStreaming || bRequiresOAuthCallback) &&
        !Environment.bHttpStreamingAvailable)
    {
        AddIssue(
            Report,
            EOpenPocketBaseEditorValidationCode::StreamingUnavailable,
            TEXT("Realtime or assisted OAuth requires incremental HTTP streaming, but this target doesn't provide it. "
                 "Disable the dependent feature for this target or add a transport that supports streaming."));
    }
    if (bRequiresSecurePersistence && !Environment.bSecurePersistenceAvailable)
    {
        AddIssue(
            Report,
            EOpenPocketBaseEditorValidationCode::SecurePersistenceUnavailable,
            TEXT("A profile requires secure session persistence, but this target has no supported secure store. "
                 "Add a secure-store implementation or change that profile's Session Persistence setting."));
    }
    if (bRequiresOAuthCallback && !Environment.bOAuthCallbackAvailable)
    {
        AddIssue(
            Report,
            EOpenPocketBaseEditorValidationCode::OAuthCallbackUnavailable,
            TEXT("A profile enables assisted OAuth, but this target has no validated callback handler. "
                 "Add a platform callback bridge or disable Assisted OAuth for that profile."));
    }
    if (Settings.bRequireOfflineModule && !Environment.bOfflineModuleAvailable)
    {
        AddIssue(
            Report,
            EOpenPocketBaseEditorValidationCode::MissingOfflineModule,
            TEXT("Require Offline Module is enabled, but OpenPocketBaseSDKOffline isn't loaded. "
                 "Enable that plugin module or turn off the requirement in Project Settings."));
    }
    if (Settings.bRequirePrivilegedModule && !Environment.bPrivilegedModuleAvailable)
    {
        AddIssue(
            Report,
            EOpenPocketBaseEditorValidationCode::MissingPrivilegedModule,
            TEXT("Require Privileged Module is enabled, but OpenPocketBaseSDKAdmin isn't loaded. "
                 "Enable that plugin module for trusted targets or turn off the requirement."));
    }
    if (Settings.bRequirePrivilegedModule && !Environment.bPrivilegedShippingGatePresent)
    {
        AddIssue(
            Report,
            EOpenPocketBaseEditorValidationCode::MissingPrivilegedShippingGate,
            TEXT("OpenPocketBaseSDKAdmin can be loaded by a Shipping target. Exclude the module from Shipping before packaging."));
    }
    return Report;
}

FOpenPocketBaseEditorValidationReport FOpenPocketBaseEditorValidator::ValidateSettings(
    const UOpenPocketBaseProjectSettings& Settings)
{
    return ValidateSettings(Settings, BuildEnvironment(Settings));
}

FOpenPocketBaseEditorValidationReport FOpenPocketBaseEditorValidator::ValidateCurrentProject()
{
    FOpenPocketBaseEditorValidationReport Report =
        ValidateSettings(*GetDefault<UOpenPocketBaseProjectSettings>());
    ScanCommandLine(FCommandLine::Get(), Report);

    int32 FileCount = 0;
    int64 AggregateBytes = 0;
    ScanDirectory(
        FPaths::ProjectConfigDir(),
        EOpenPocketBaseEditorArtifactKind::Config,
        {TEXT(".ini")},
        FileCount,
        AggregateBytes,
        Report);
    ScanDirectory(
        FPaths::GameSourceDir(),
        EOpenPocketBaseEditorArtifactKind::Source,
        {TEXT(".h"), TEXT(".hpp"), TEXT(".cpp"), TEXT(".mm"), TEXT(".cs")},
        FileCount,
        AggregateBytes,
        Report);
    return Report;
}

void FOpenPocketBaseEditorValidator::ScanCommandLine(
    FStringView CommandLine,
    FOpenPocketBaseEditorValidationReport& OutReport)
{
    FString LowerCommandLine(CommandLine.Left(32 * 1024));
    LowerCommandLine.ToLowerInline();
    static const TCHAR* CredentialSwitches[] = {
        TEXT("openpocketbasesuperuserpassword="),
        TEXT("openpocketbase-superuser-password="),
        TEXT("pocketbase-superuser-password="),
        TEXT("pocketbase-admin-password="),
        TEXT("superuser-password=")};
    for (const TCHAR* Switch : CredentialSwitches)
    {
        if (LowerCommandLine.Contains(Switch))
        {
            AddIssue(
                OutReport,
                EOpenPocketBaseEditorValidationCode::EmbeddedSuperuserCredential,
                TEXT("A PocketBase superuser password is present on the process command line, where other processes may read it. "
                     "Remove the argument and supply the password from a process-local prompt or secret provider."),
                TEXT("process command line"));
            return;
        }
    }
}

void FOpenPocketBaseEditorValidator::ScanTextArtifact(
    const EOpenPocketBaseEditorArtifactKind Kind,
    FStringView Identifier,
    FStringView Contents,
    FOpenPocketBaseEditorValidationReport& OutReport)
{
    if (!ContainsCredentialAssignment(
            Contents,
            Kind == EOpenPocketBaseEditorArtifactKind::Source))
    {
        return;
    }
    AddIssue(
        OutReport,
        EOpenPocketBaseEditorValidationCode::EmbeddedSuperuserCredential,
        Kind == EOpenPocketBaseEditorArtifactKind::Config
            ? TEXT("A PocketBase superuser password appears in a config file. Remove it and load the value from a secret provider at runtime.")
            : TEXT("A PocketBase superuser password appears in a source default or literal call. Remove it and supply the value at runtime."),
        Identifier);
}

void FOpenPocketBaseEditorValidator::ScanBlueprint(
    const UBlueprint& Blueprint,
    FOpenPocketBaseEditorValidationReport& OutReport)
{
    bool bFoundCredential = HasEmbeddedCredentialProperty(&Blueprint);
    if (!bFoundCredential && Blueprint.GeneratedClass != nullptr)
    {
        bFoundCredential = HasEmbeddedCredentialProperty(
            Blueprint.GeneratedClass->GetDefaultObject(false));
    }

    TArray<UEdGraph*> Graphs;
    Blueprint.GetAllGraphs(Graphs);
    for (const UEdGraph* Graph : Graphs)
    {
        if (Graph == nullptr)
        {
            continue;
        }
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            const UK2Node_BaseAsyncTask* AsyncNode = Cast<UK2Node_BaseAsyncTask>(Node);
            if (AsyncNode == nullptr || !IsPrivilegedFactory(AsyncNode->GetFactoryFunction()))
            {
                continue;
            }
            const UEdGraphPin* PasswordPin = AsyncNode->FindPin(TEXT("Password"), EGPD_Input);
            if (PasswordPin != nullptr && PasswordPin->LinkedTo.IsEmpty() &&
                (!PasswordPin->DefaultValue.IsEmpty() || PasswordPin->DefaultObject != nullptr))
            {
                bFoundCredential = true;
                break;
            }
        }
        if (bFoundCredential)
        {
            break;
        }
    }

    if (bFoundCredential)
    {
        AddIssue(
            OutReport,
            EOpenPocketBaseEditorValidationCode::EmbeddedSuperuserCredential,
            TEXT("A PocketBase superuser password is stored in this Blueprint asset. Clear the Password pin default and supply it at runtime."),
            Blueprint.GetPathName());
    }
}
