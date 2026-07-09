#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UOpenPocketBaseProjectSettings;

enum class EOpenPocketBaseEditorValidationSeverity : uint8
{
    Information,
    Warning,
    Error
};

enum class EOpenPocketBaseEditorValidationCode : uint8
{
    InvalidProjectSettings,
    MissingOfflineModule,
    MissingPrivilegedModule,
    StreamingUnavailable,
    SecurePersistenceUnavailable,
    OAuthCallbackUnavailable,
    MissingPrivilegedShippingGate,
    EmbeddedSuperuserCredential,
    ScanLimitReached
};

enum class EOpenPocketBaseEditorArtifactKind : uint8
{
    Config,
    Source
};

struct OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseEditorValidationIssue
{
    EOpenPocketBaseEditorValidationSeverity Severity =
        EOpenPocketBaseEditorValidationSeverity::Error;
    EOpenPocketBaseEditorValidationCode Code =
        EOpenPocketBaseEditorValidationCode::InvalidProjectSettings;
    FString Message;
    FString Context;
};

struct OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseEditorValidationReport
{
    static constexpr int32 MaximumIssues = 128;

    TArray<FOpenPocketBaseEditorValidationIssue> Issues;
    bool bIssueLimitReached = false;

    void Add(FOpenPocketBaseEditorValidationIssue&& Issue);
    bool HasErrors() const;
    FString ToSanitizedText() const;
};

struct OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseEditorValidationEnvironment
{
    bool bHttpStreamingAvailable = true;
    bool bSecurePersistenceAvailable = true;
    bool bOAuthCallbackAvailable = true;
    bool bOfflineModuleAvailable = true;
    bool bPrivilegedModuleAvailable = true;
    bool bPrivilegedShippingGatePresent = true;
};

class OPENPOCKETBASESDKEDITOR_API FOpenPocketBaseEditorValidator
{
public:
    static FOpenPocketBaseEditorValidationReport ValidateSettings(
        const UOpenPocketBaseProjectSettings& Settings,
        const FOpenPocketBaseEditorValidationEnvironment& Environment);

    static FOpenPocketBaseEditorValidationReport ValidateSettings(
        const UOpenPocketBaseProjectSettings& Settings);

    static FOpenPocketBaseEditorValidationReport ValidateCurrentProject();

    static void ScanCommandLine(
        FStringView CommandLine,
        FOpenPocketBaseEditorValidationReport& OutReport);

    static void ScanTextArtifact(
        EOpenPocketBaseEditorArtifactKind Kind,
        FStringView Identifier,
        FStringView Contents,
        FOpenPocketBaseEditorValidationReport& OutReport);

    static void ScanBlueprint(
        const UBlueprint& Blueprint,
        FOpenPocketBaseEditorValidationReport& OutReport);
};
