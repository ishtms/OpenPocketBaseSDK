#include "OpenPocketBaseSchemaDiagnostics.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaImporter.h"

namespace
{
bool SameFieldDefinition(
    const FOpenPocketBaseSchemaField& Left,
    const FOpenPocketBaseSchemaField& Right)
{
    return Left.Type == Right.Type &&
        Left.bMultiple == Right.bMultiple &&
        Left.bRequired == Right.bRequired &&
        Left.bSystem == Right.bSystem &&
        Left.bHidden == Right.bHidden &&
        Left.bReadOnly == Right.bReadOnly &&
        Left.Storage == Right.Storage &&
        Left.bHasMin == Right.bHasMin &&
        Left.Min == Right.Min &&
        Left.bHasMax == Right.bHasMax &&
        Left.Max == Right.Max &&
        Left.Pattern == Right.Pattern &&
        Left.Choices == Right.Choices &&
        Left.MimeTypes == Right.MimeTypes &&
        Left.MaxSizeBytes == Right.MaxSizeBytes &&
        Left.MinSelect == Right.MinSelect &&
        Left.MaxSelect == Right.MaxSelect &&
        Left.RelatedCollectionId == Right.RelatedCollectionId;
}

void AddDiffSection(
    const TCHAR* Label,
    const TArray<FString>& Values,
    TArray<FString>& OutLines)
{
    if (Values.IsEmpty())
    {
        return;
    }

    OutLines.Add(FString::Printf(TEXT("%s:"), Label));
    for (const FString& Value : Values)
    {
        OutLines.Add(TEXT("  ") + Value);
    }
}

void AddDiagnostic(
    FOpenPocketBaseSchemaValidationReport& Report,
    const EOpenPocketBaseSchemaDiagnosticSeverity Severity,
    FString Message)
{
    FOpenPocketBaseSchemaDiagnostic& Diagnostic = Report.Issues.AddDefaulted_GetRef();
    Diagnostic.Severity = Severity;
    Diagnostic.Message = MoveTemp(Message);
}

const TCHAR* DiagnosticPrefix(const EOpenPocketBaseSchemaDiagnosticSeverity Severity)
{
    switch (Severity)
    {
    case EOpenPocketBaseSchemaDiagnosticSeverity::Warning:
        return TEXT("Warning");
    case EOpenPocketBaseSchemaDiagnosticSeverity::Error:
        return TEXT("Error");
    default:
        return TEXT("Info");
    }
}
}

FString FOpenPocketBaseSchemaSummary::ToText() const
{
    const TCHAR* SourceStatus = !bHasSource
        ? TEXT("Not set")
        : bSourceExists ? TEXT("Ready") : TEXT("Missing");
    return FString::Printf(
        TEXT("%d collections (%d base, %d auth, %d view)\n")
        TEXT("%d fields (%d writable, %d required)\n")
        TEXT("Source: %s\n")
        TEXT("Fingerprint: %s"),
        CollectionCount,
        BaseCollectionCount,
        AuthCollectionCount,
        ViewCollectionCount,
        FieldCount,
        WritableFieldCount,
        RequiredFieldCount,
        SourceStatus,
        bHasFingerprint ? TEXT("Ready") : TEXT("Missing"));
}

bool FOpenPocketBaseSchemaDiff::IsEmpty() const
{
    return NumChanges() == 0;
}

int32 FOpenPocketBaseSchemaDiff::NumChanges() const
{
    return AddedCollections.Num() +
        RemovedCollections.Num() +
        RenamedCollections.Num() +
        ChangedCollections.Num() +
        AddedFields.Num() +
        RemovedFields.Num() +
        RenamedFields.Num() +
        ChangedFields.Num();
}

FString FOpenPocketBaseSchemaDiff::ToText() const
{
    if (IsEmpty())
    {
        return TEXT("No schema changes.");
    }

    TArray<FString> Lines;
    Lines.Add(FString::Printf(
        TEXT("%d schema change%s"),
        NumChanges(),
        NumChanges() == 1 ? TEXT("") : TEXT("s")));
    AddDiffSection(TEXT("Added collections"), AddedCollections, Lines);
    AddDiffSection(TEXT("Removed collections"), RemovedCollections, Lines);
    AddDiffSection(TEXT("Renamed collections"), RenamedCollections, Lines);
    AddDiffSection(TEXT("Changed collections"), ChangedCollections, Lines);
    AddDiffSection(TEXT("Added fields"), AddedFields, Lines);
    AddDiffSection(TEXT("Removed fields"), RemovedFields, Lines);
    AddDiffSection(TEXT("Renamed fields"), RenamedFields, Lines);
    AddDiffSection(TEXT("Changed fields"), ChangedFields, Lines);
    return FString::Join(Lines, TEXT("\n"));
}

bool FOpenPocketBaseSchemaValidationReport::HasErrors() const
{
    return Issues.ContainsByPredicate(
        [](const FOpenPocketBaseSchemaDiagnostic& Diagnostic)
        {
            return Diagnostic.Severity == EOpenPocketBaseSchemaDiagnosticSeverity::Error;
        });
}

FString FOpenPocketBaseSchemaValidationReport::ToText() const
{
    if (Issues.IsEmpty())
    {
        return TEXT("Schema validation passed.");
    }

    TArray<FString> Lines;
    Lines.Reserve(Issues.Num());
    for (const FOpenPocketBaseSchemaDiagnostic& Diagnostic : Issues)
    {
        Lines.Add(FString::Printf(
            TEXT("%s: %s"),
            DiagnosticPrefix(Diagnostic.Severity),
            *Diagnostic.Message));
    }
    return FString::Join(Lines, TEXT("\n"));
}

FOpenPocketBaseSchemaSummary FOpenPocketBaseSchemaDiagnostics::Summarize(
    const UOpenPocketBaseSchema& Schema)
{
    FOpenPocketBaseSchemaSummary Summary;
    Summary.CollectionCount = Schema.Collections.Num();
    Summary.bHasSource = !Schema.Source.IsEmpty();
    Summary.bSourceExists = Summary.bHasSource &&
        IFileManager::Get().FileExists(*Schema.Source);
    Summary.bHasFingerprint = !Schema.Fingerprint.IsEmpty();

    for (const FOpenPocketBaseSchemaCollection& Collection : Schema.Collections)
    {
        switch (Collection.Type)
        {
        case EOpenPocketBaseCollectionType::Base:
            ++Summary.BaseCollectionCount;
            break;
        case EOpenPocketBaseCollectionType::Auth:
            ++Summary.AuthCollectionCount;
            break;
        case EOpenPocketBaseCollectionType::View:
            ++Summary.ViewCollectionCount;
            break;
        default:
            break;
        }

        Summary.FieldCount += Collection.Fields.Num();
        for (const FOpenPocketBaseSchemaField& Field : Collection.Fields)
        {
            if (!Field.bReadOnly && Field.Storage == EOpenPocketBaseFieldStorage::Data)
            {
                ++Summary.WritableFieldCount;
            }
            if (Field.bRequired)
            {
                ++Summary.RequiredFieldCount;
            }
        }
    }
    return Summary;
}

FOpenPocketBaseSchemaDiff FOpenPocketBaseSchemaDiagnostics::Compare(
    const UOpenPocketBaseSchema& Current,
    const UOpenPocketBaseSchema& Candidate)
{
    FOpenPocketBaseSchemaDiff Diff;
    TMap<FString, const FOpenPocketBaseSchemaCollection*> CurrentCollections;
    TMap<FString, const FOpenPocketBaseSchemaCollection*> CandidateCollections;
    for (const FOpenPocketBaseSchemaCollection& Collection : Current.Collections)
    {
        CurrentCollections.Add(Collection.Id, &Collection);
    }
    for (const FOpenPocketBaseSchemaCollection& Collection : Candidate.Collections)
    {
        CandidateCollections.Add(Collection.Id, &Collection);
    }

    for (const FOpenPocketBaseSchemaCollection& CurrentCollection : Current.Collections)
    {
        const FOpenPocketBaseSchemaCollection* const* CandidatePtr =
            CandidateCollections.Find(CurrentCollection.Id);
        if (CandidatePtr == nullptr)
        {
            Diff.RemovedCollections.Add(CurrentCollection.Name);
            continue;
        }

        const FOpenPocketBaseSchemaCollection& CandidateCollection = **CandidatePtr;
        if (CurrentCollection.Name != CandidateCollection.Name)
        {
            Diff.RenamedCollections.Add(FString::Printf(
                TEXT("%s -> %s"),
                *CurrentCollection.Name,
                *CandidateCollection.Name));
        }
        if (CurrentCollection.Type != CandidateCollection.Type ||
            CurrentCollection.bSystem != CandidateCollection.bSystem)
        {
            Diff.ChangedCollections.Add(
                CandidateCollection.Name + TEXT(" (type or system status changed)"));
        }

        TMap<FString, const FOpenPocketBaseSchemaField*> CurrentFields;
        TMap<FString, const FOpenPocketBaseSchemaField*> CandidateFields;
        for (const FOpenPocketBaseSchemaField& Field : CurrentCollection.Fields)
        {
            CurrentFields.Add(Field.Id, &Field);
        }
        for (const FOpenPocketBaseSchemaField& Field : CandidateCollection.Fields)
        {
            CandidateFields.Add(Field.Id, &Field);
        }

        for (const FOpenPocketBaseSchemaField& CurrentField : CurrentCollection.Fields)
        {
            const FOpenPocketBaseSchemaField* const* CandidateFieldPtr =
                CandidateFields.Find(CurrentField.Id);
            if (CandidateFieldPtr == nullptr)
            {
                Diff.RemovedFields.Add(FString::Printf(
                    TEXT("%s.%s"),
                    *CurrentCollection.Name,
                    *CurrentField.Name));
                continue;
            }

            const FOpenPocketBaseSchemaField& CandidateField = **CandidateFieldPtr;
            if (CurrentField.Name != CandidateField.Name)
            {
                Diff.RenamedFields.Add(FString::Printf(
                    TEXT("%s.%s -> %s.%s"),
                    *CurrentCollection.Name,
                    *CurrentField.Name,
                    *CandidateCollection.Name,
                    *CandidateField.Name));
            }
            if (!SameFieldDefinition(CurrentField, CandidateField))
            {
                Diff.ChangedFields.Add(FString::Printf(
                    TEXT("%s.%s (definition changed)"),
                    *CandidateCollection.Name,
                    *CandidateField.Name));
            }
        }

        for (const FOpenPocketBaseSchemaField& CandidateField : CandidateCollection.Fields)
        {
            if (!CurrentFields.Contains(CandidateField.Id))
            {
                Diff.AddedFields.Add(FString::Printf(
                    TEXT("%s.%s"),
                    *CandidateCollection.Name,
                    *CandidateField.Name));
            }
        }
    }

    for (const FOpenPocketBaseSchemaCollection& CandidateCollection : Candidate.Collections)
    {
        if (!CurrentCollections.Contains(CandidateCollection.Id))
        {
            Diff.AddedCollections.Add(CandidateCollection.Name);
        }
    }

    Diff.AddedCollections.Sort();
    Diff.RemovedCollections.Sort();
    Diff.RenamedCollections.Sort();
    Diff.ChangedCollections.Sort();
    Diff.AddedFields.Sort();
    Diff.RemovedFields.Sort();
    Diff.RenamedFields.Sort();
    Diff.ChangedFields.Sort();
    return Diff;
}

FOpenPocketBaseSchemaValidationReport FOpenPocketBaseSchemaDiagnostics::Validate(
    const UOpenPocketBaseSchema& Schema)
{
    FOpenPocketBaseSchemaValidationReport Report;
    if (!Schema.SchemaId.IsValid())
    {
        AddDiagnostic(
            Report,
            EOpenPocketBaseSchemaDiagnosticSeverity::Error,
            TEXT("Schema ID is missing."));
    }
    if (Schema.Source.IsEmpty())
    {
        AddDiagnostic(
            Report,
            EOpenPocketBaseSchemaDiagnosticSeverity::Warning,
            TEXT("Source file is not set."));
    }
    else if (!IFileManager::Get().FileExists(*Schema.Source))
    {
        AddDiagnostic(
            Report,
            EOpenPocketBaseSchemaDiagnosticSeverity::Warning,
            FString::Printf(TEXT("Source file is missing: %s"), *Schema.Source));
    }
    if (Schema.Fingerprint.IsEmpty())
    {
        AddDiagnostic(
            Report,
            EOpenPocketBaseSchemaDiagnosticSeverity::Warning,
            TEXT("Schema fingerprint is missing."));
    }

    TSet<FString> CollectionIds;
    TSet<FString> CollectionNames;
    for (const FOpenPocketBaseSchemaCollection& Collection : Schema.Collections)
    {
        if (Collection.Id.IsEmpty())
        {
            AddDiagnostic(
                Report,
                EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                FString::Printf(TEXT("Collection '%s' has no stable ID."), *Collection.Name));
        }
        else if (CollectionIds.Contains(Collection.Id))
        {
            AddDiagnostic(
                Report,
                EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                FString::Printf(TEXT("Duplicate collection ID: %s"), *Collection.Id));
        }
        else
        {
            CollectionIds.Add(Collection.Id);
        }

        if (Collection.Name.IsEmpty())
        {
            AddDiagnostic(
                Report,
                EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                FString::Printf(TEXT("Collection '%s' has no name."), *Collection.Id));
        }
        else if (CollectionNames.Contains(Collection.Name))
        {
            AddDiagnostic(
                Report,
                EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                FString::Printf(TEXT("Duplicate collection name: %s"), *Collection.Name));
        }
        else
        {
            CollectionNames.Add(Collection.Name);
        }

        if (Collection.Type == EOpenPocketBaseCollectionType::Unknown)
        {
            AddDiagnostic(
                Report,
                EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                FString::Printf(TEXT("Collection '%s' has an unknown type."), *Collection.Name));
        }

        TSet<FString> FieldIds;
        TSet<FString> FieldNames;
        for (const FOpenPocketBaseSchemaField& Field : Collection.Fields)
        {
            const FString FieldPath = FString::Printf(
                TEXT("%s.%s"),
                *Collection.Name,
                *Field.Name);
            if (Field.Id.IsEmpty())
            {
                AddDiagnostic(
                    Report,
                    EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                    FieldPath + TEXT(" has no stable field ID."));
            }
            else if (FieldIds.Contains(Field.Id))
            {
                AddDiagnostic(
                    Report,
                    EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                    FString::Printf(
                        TEXT("Collection '%s' has duplicate field ID: %s"),
                        *Collection.Name,
                        *Field.Id));
            }
            else
            {
                FieldIds.Add(Field.Id);
            }

            if (Field.Name.IsEmpty())
            {
                AddDiagnostic(
                    Report,
                    EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                    FString::Printf(TEXT("Collection '%s' has a field with no name."), *Collection.Name));
            }
            else if (FieldNames.Contains(Field.Name))
            {
                AddDiagnostic(
                    Report,
                    EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                    FString::Printf(
                        TEXT("Collection '%s' has duplicate field name: %s"),
                        *Collection.Name,
                        *Field.Name));
            }
            else
            {
                FieldNames.Add(Field.Name);
            }

            if (Field.Type == EOpenPocketBaseFieldType::Unknown)
            {
                AddDiagnostic(
                    Report,
                    EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                    FieldPath + TEXT(" has an unknown field type."));
            }
            if (Field.bHasMin && Field.bHasMax && Field.Min > Field.Max)
            {
                AddDiagnostic(
                    Report,
                    EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                    FieldPath + TEXT(" has a minimum greater than its maximum."));
            }
            if (Field.MinSelect < 0 || Field.MaxSelect < 0 ||
                (Field.MaxSelect > 0 && Field.MinSelect > Field.MaxSelect))
            {
                AddDiagnostic(
                    Report,
                    EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                    FieldPath + TEXT(" has invalid selection limits."));
            }
            if (Field.MaxSizeBytes < 0)
            {
                AddDiagnostic(
                    Report,
                    EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                    FieldPath + TEXT(" has a negative file size limit."));
            }
        }
    }

    for (const FOpenPocketBaseSchemaCollection& Collection : Schema.Collections)
    {
        for (const FOpenPocketBaseSchemaField& Field : Collection.Fields)
        {
            if (Field.Type == EOpenPocketBaseFieldType::Relation &&
                (Field.RelatedCollectionId.IsEmpty() ||
                 !CollectionIds.Contains(Field.RelatedCollectionId)))
            {
                AddDiagnostic(
                    Report,
                    EOpenPocketBaseSchemaDiagnosticSeverity::Error,
                    FString::Printf(
                        TEXT("Relation '%s.%s' targets a missing collection."),
                        *Collection.Name,
                        *Field.Name));
            }
        }
    }
    return Report;
}

bool FOpenPocketBaseSchemaDiagnostics::LoadSourcePreview(
    const UOpenPocketBaseSchema& Schema,
    UOpenPocketBaseSchema*& OutPreview,
    FText& OutError)
{
    OutPreview = nullptr;
    OutError = FText::GetEmpty();
    if (Schema.Source.IsEmpty())
    {
        OutError = FText::FromString(TEXT("This schema asset has no source file."));
        return false;
    }

    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Schema.Source))
    {
        OutError = FText::FromString(FString::Printf(
            TEXT("Could not read the schema source file: %s"),
            *Schema.Source));
        return false;
    }

    UOpenPocketBaseSchema* Preview = NewObject<UOpenPocketBaseSchema>(GetTransientPackage());
    Preview->SchemaId = Schema.SchemaId;
    if (!FOpenPocketBaseSchemaImporter::ImportJson(
            Json,
            Schema.Source,
            *Preview,
            OutError))
    {
        return false;
    }

    OutPreview = Preview;
    return true;
}
