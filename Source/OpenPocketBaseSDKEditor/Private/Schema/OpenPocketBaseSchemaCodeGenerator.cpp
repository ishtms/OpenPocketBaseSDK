#include "OpenPocketBaseSchemaCodeGenerator.h"

#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenPocketBaseSchema.h"

#define LOCTEXT_NAMESPACE "OpenPocketBaseSchemaCodeGenerator"

namespace
{
struct FFieldAccessor
{
    const FOpenPocketBaseSchemaField* Field = nullptr;
    FString Name;
    FString Type;
    FString StableKey;
};

FString ProjectSourceDirectory()
{
    return FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));
}

FString EscapeStringLiteral(const FString& Value)
{
    FString Escaped = Value;
    Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
    Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
    Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
    Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
    Escaped.ReplaceInline(TEXT("\t"), TEXT("\\t"));
    return Escaped;
}

FString MakeIdentifier(const FString& Value, const TCHAR* Fallback)
{
    FString Identifier;
    bool bUpperNext = true;
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
        {
            bUpperNext = true;
            continue;
        }
        if (Character == TEXT('_'))
        {
            bUpperNext = true;
            continue;
        }

        Identifier.AppendChar(bUpperNext ? FChar::ToUpper(Character) : Character);
        bUpperNext = false;
    }

    if (Identifier.IsEmpty())
    {
        Identifier = Fallback;
    }
    if (FChar::IsDigit(Identifier[0]))
    {
        Identifier.InsertAt(0, TEXT('N'));
    }
    return Identifier;
}

bool IsValidNamespace(const FString& RootNamespace)
{
    TArray<FString> Parts;
    RootNamespace.ParseIntoArray(Parts, TEXT("::"), false);
    if (Parts.IsEmpty())
    {
        return false;
    }

    for (const FString& Part : Parts)
    {
        if (Part.IsEmpty() || FChar::IsDigit(Part[0]))
        {
            return false;
        }
        for (const TCHAR Character : Part)
        {
            if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
            {
                return false;
            }
        }
    }
    return true;
}

FString StableIdentifier(const FString& Base, const FString& StableKey, const int32 CollisionCount)
{
    if (CollisionCount == 1 && Base != TEXT("Detail"))
    {
        return Base;
    }
    return FString::Printf(TEXT("%s_%08X"), *Base, FCrc::StrCrc32(*StableKey));
}

const TCHAR* CollectionTypeLiteral(const EOpenPocketBaseCollectionType Type)
{
    switch (Type)
    {
    case EOpenPocketBaseCollectionType::Base:
        return TEXT("EOpenPocketBaseCollectionType::Base");
    case EOpenPocketBaseCollectionType::Auth:
        return TEXT("EOpenPocketBaseCollectionType::Auth");
    case EOpenPocketBaseCollectionType::View:
        return TEXT("EOpenPocketBaseCollectionType::View");
    default:
        return TEXT("EOpenPocketBaseCollectionType::Unknown");
    }
}

const TCHAR* FieldTypeLiteral(const EOpenPocketBaseFieldType Type)
{
    switch (Type)
    {
    case EOpenPocketBaseFieldType::Text:
        return TEXT("EOpenPocketBaseFieldType::Text");
    case EOpenPocketBaseFieldType::Number:
        return TEXT("EOpenPocketBaseFieldType::Number");
    case EOpenPocketBaseFieldType::Boolean:
        return TEXT("EOpenPocketBaseFieldType::Boolean");
    case EOpenPocketBaseFieldType::Email:
        return TEXT("EOpenPocketBaseFieldType::Email");
    case EOpenPocketBaseFieldType::Url:
        return TEXT("EOpenPocketBaseFieldType::Url");
    case EOpenPocketBaseFieldType::Editor:
        return TEXT("EOpenPocketBaseFieldType::Editor");
    case EOpenPocketBaseFieldType::Date:
        return TEXT("EOpenPocketBaseFieldType::Date");
    case EOpenPocketBaseFieldType::Autodate:
        return TEXT("EOpenPocketBaseFieldType::Autodate");
    case EOpenPocketBaseFieldType::Select:
        return TEXT("EOpenPocketBaseFieldType::Select");
    case EOpenPocketBaseFieldType::File:
        return TEXT("EOpenPocketBaseFieldType::File");
    case EOpenPocketBaseFieldType::Relation:
        return TEXT("EOpenPocketBaseFieldType::Relation");
    case EOpenPocketBaseFieldType::Json:
        return TEXT("EOpenPocketBaseFieldType::Json");
    case EOpenPocketBaseFieldType::Password:
        return TEXT("EOpenPocketBaseFieldType::Password");
    case EOpenPocketBaseFieldType::GeoPoint:
        return TEXT("EOpenPocketBaseFieldType::GeoPoint");
    default:
        return TEXT("EOpenPocketBaseFieldType::Unknown");
    }
}

FString ValueReferenceType(const FOpenPocketBaseSchemaField& Field)
{
    switch (Field.GetValueType())
    {
    case EOpenPocketBaseFieldValueType::String:
        return TEXT("FOpenPocketBaseStringFieldRef");
    case EOpenPocketBaseFieldValueType::Number:
        return TEXT("FOpenPocketBaseNumberFieldRef");
    case EOpenPocketBaseFieldValueType::Boolean:
        return TEXT("FOpenPocketBaseBooleanFieldRef");
    case EOpenPocketBaseFieldValueType::DateTime:
        return TEXT("FOpenPocketBaseDateFieldRef");
    case EOpenPocketBaseFieldValueType::StringArray:
        return TEXT("FOpenPocketBaseStringArrayFieldRef");
    case EOpenPocketBaseFieldValueType::Json:
    case EOpenPocketBaseFieldValueType::GeoPoint:
        return TEXT("FOpenPocketBaseJsonFieldRef");
    default:
        return TEXT("FOpenPocketBaseAnyFieldRef");
    }
}

FString PrimaryReferenceType(const FOpenPocketBaseSchemaField& Field)
{
    if (Field.Type == EOpenPocketBaseFieldType::File)
    {
        return TEXT("FOpenPocketBaseFileFieldRef");
    }
    if (Field.Type == EOpenPocketBaseFieldType::Relation)
    {
        return TEXT("FOpenPocketBaseRelationFieldRef");
    }
    return ValueReferenceType(Field);
}

FString CollectionReferenceType(const EOpenPocketBaseCollectionType Type)
{
    if (Type == EOpenPocketBaseCollectionType::Auth)
    {
        return TEXT("FOpenPocketBaseAuthCollectionRef");
    }
    if (Type == EOpenPocketBaseCollectionType::Base)
    {
        return TEXT("FOpenPocketBaseWritableCollectionRef");
    }
    return TEXT("FOpenPocketBaseCollectionRef");
}

FString CollectionServiceType(const EOpenPocketBaseCollectionType Type)
{
    if (Type == EOpenPocketBaseCollectionType::Auth)
    {
        return TEXT("FOpenPocketBaseAuthCollectionService");
    }
    if (Type == EOpenPocketBaseCollectionType::Base)
    {
        return TEXT("FOpenPocketBaseWritableCollectionService");
    }
    return TEXT("FOpenPocketBaseCollectionService");
}

FString CollectionServiceCall(const EOpenPocketBaseCollectionType Type)
{
    if (Type == EOpenPocketBaseCollectionType::Auth)
    {
        return TEXT("AuthCollection");
    }
    if (Type == EOpenPocketBaseCollectionType::Base)
    {
        return TEXT("WritableCollection");
    }
    return TEXT("Collection");
}

void AppendFieldAccessor(
    FString& Header,
    const FFieldAccessor& Accessor,
    const FString& CollectionId)
{
    const FOpenPocketBaseSchemaField& Field = *Accessor.Field;
    Header.Appendf(
        TEXT("inline %s %s()\n")
        TEXT("{\n")
        TEXT("    return Detail::MakeFieldRef<%s>(TEXT(\"%s\"), TEXT(\"%s\"), TEXT(\"%s\"), %s, %s, %s, TEXT(\"%s\"));\n")
        TEXT("}\n\n"),
        *Accessor.Type,
        *Accessor.Name,
        *Accessor.Type,
        *EscapeStringLiteral(CollectionId),
        *EscapeStringLiteral(Field.Id),
        *EscapeStringLiteral(Field.Name),
        FieldTypeLiteral(Field.Type),
        Field.bMultiple ? TEXT("true") : TEXT("false"),
        Field.bReadOnly ? TEXT("true") : TEXT("false"),
        *EscapeStringLiteral(Field.RelatedCollectionId));
}
}

bool FOpenPocketBaseSchemaCodeGenerator::GenerateHeader(
    const UOpenPocketBaseSchema& Schema,
    const FString& RootNamespace,
    FString& OutHeader,
    FText& OutError)
{
    OutHeader.Reset();
    OutError = FText::GetEmpty();
    if (!Schema.SchemaId.IsValid())
    {
        OutError = LOCTEXT("MissingSchemaId", "The schema needs a stable schema ID before C++ accessors can be generated.");
        return false;
    }
    if (!IsValidNamespace(RootNamespace))
    {
        OutError = LOCTEXT("InvalidNamespace", "Enter a valid C++ namespace, such as MyGame::PocketBase.");
        return false;
    }

    TArray<const FOpenPocketBaseSchemaCollection*> SortedCollections;
    SortedCollections.Reserve(Schema.Collections.Num());
    for (const FOpenPocketBaseSchemaCollection& Collection : Schema.Collections)
    {
        SortedCollections.Add(&Collection);
    }
    SortedCollections.Sort(
        [](const FOpenPocketBaseSchemaCollection& Left, const FOpenPocketBaseSchemaCollection& Right)
        {
            return Left.Id < Right.Id;
        });

    TMap<FString, int32> CollectionNameCounts;
    for (const FOpenPocketBaseSchemaCollection* Collection : SortedCollections)
    {
        ++CollectionNameCounts.FindOrAdd(MakeIdentifier(Collection->Name, TEXT("Collection")));
    }

    OutHeader = TEXT("#pragma once\n\n")
        TEXT("#include \"OpenPocketBaseClient.h\"\n")
        TEXT("#include \"OpenPocketBaseSchema.h\"\n\n");
    OutHeader.Appendf(TEXT("namespace %s\n{\n"), *RootNamespace);
    OutHeader.Append(TEXT("namespace Detail\n{\n"));
    OutHeader.Appendf(
        TEXT("inline const TCHAR* SchemaFingerprint() { return TEXT(\"%s\"); }\n"),
        *EscapeStringLiteral(Schema.Fingerprint));
    OutHeader.Appendf(
        TEXT("inline FGuid SchemaId() { return FGuid(0x%08Xu, 0x%08Xu, 0x%08Xu, 0x%08Xu); }\n\n"),
        Schema.SchemaId.A,
        Schema.SchemaId.B,
        Schema.SchemaId.C,
        Schema.SchemaId.D);
    OutHeader.Append(
        TEXT("template <typename TCollection>\n")
        TEXT("inline TCollection MakeCollectionRef(const TCHAR* CollectionId, const TCHAR* Name, EOpenPocketBaseCollectionType Type)\n")
        TEXT("{\n")
        TEXT("    TCollection Result;\n")
        TEXT("    Result.SchemaId = SchemaId();\n")
        TEXT("    Result.CollectionId = CollectionId;\n")
        TEXT("    Result.Name = Name;\n")
        TEXT("    Result.Type = Type;\n")
        TEXT("    return Result;\n")
        TEXT("}\n\n")
        TEXT("template <typename TField>\n")
        TEXT("inline TField MakeFieldRef(\n")
        TEXT("    const TCHAR* CollectionId,\n")
        TEXT("    const TCHAR* FieldId,\n")
        TEXT("    const TCHAR* Name,\n")
        TEXT("    EOpenPocketBaseFieldType Type,\n")
        TEXT("    bool bMultiple,\n")
        TEXT("    bool bReadOnly,\n")
        TEXT("    const TCHAR* RelatedCollectionId)\n")
        TEXT("{\n")
        TEXT("    TField Result;\n")
        TEXT("    Result.SchemaId = SchemaId();\n")
        TEXT("    Result.CollectionId = CollectionId;\n")
        TEXT("    Result.FieldId = FieldId;\n")
        TEXT("    Result.Name = Name;\n")
        TEXT("    Result.Type = Type;\n")
        TEXT("    Result.bMultiple = bMultiple;\n")
        TEXT("    Result.bReadOnly = bReadOnly;\n")
        TEXT("    Result.RelatedCollectionId = RelatedCollectionId;\n")
        TEXT("    return Result;\n")
        TEXT("}\n")
        TEXT("}\n\n"));

    for (const FOpenPocketBaseSchemaCollection* CollectionPtr : SortedCollections)
    {
        const FOpenPocketBaseSchemaCollection& Collection = *CollectionPtr;
        const FString BaseCollectionName = MakeIdentifier(Collection.Name, TEXT("Collection"));
        const FString CollectionName = StableIdentifier(
            BaseCollectionName,
            Collection.Id,
            CollectionNameCounts.FindRef(BaseCollectionName));
        const FString ReferenceType = CollectionReferenceType(Collection.Type);
        const FString ServiceType = CollectionServiceType(Collection.Type);
        const FString ServiceCall = CollectionServiceCall(Collection.Type);

        OutHeader.Appendf(TEXT("namespace %s\n{\n"), *CollectionName);
        OutHeader.Appendf(
            TEXT("inline %s Ref()\n")
            TEXT("{\n")
            TEXT("    return Detail::MakeCollectionRef<%s>(TEXT(\"%s\"), TEXT(\"%s\"), %s);\n")
            TEXT("}\n\n"),
            *ReferenceType,
            *ReferenceType,
            *EscapeStringLiteral(Collection.Id),
            *EscapeStringLiteral(Collection.Name),
            CollectionTypeLiteral(Collection.Type));
        OutHeader.Appendf(
            TEXT("inline %s Use(FOpenPocketBaseClient& Client)\n")
            TEXT("{\n")
            TEXT("    return Client.%s(Ref());\n")
            TEXT("}\n\n"),
            *ServiceType,
            *ServiceCall);

        TArray<FFieldAccessor> Accessors;
        TMap<FString, int32> AccessorNameCounts;
        TArray<const FOpenPocketBaseSchemaField*> SortedFields;
        SortedFields.Reserve(Collection.Fields.Num());
        for (const FOpenPocketBaseSchemaField& Field : Collection.Fields)
        {
            SortedFields.Add(&Field);
        }
        SortedFields.Sort(
            [](const FOpenPocketBaseSchemaField& Left, const FOpenPocketBaseSchemaField& Right)
            {
                return Left.Id < Right.Id;
            });
        for (const FOpenPocketBaseSchemaField* FieldPtr : SortedFields)
        {
            const FOpenPocketBaseSchemaField& Field = *FieldPtr;
            const FString BaseFieldName = MakeIdentifier(Field.Name, TEXT("Field"));
            FFieldAccessor& Primary = Accessors.AddDefaulted_GetRef();
            Primary.Field = &Field;
            Primary.Name = BaseFieldName;
            Primary.Type = PrimaryReferenceType(Field);
            Primary.StableKey = Field.Id;
            ++AccessorNameCounts.FindOrAdd(Primary.Name);

            if (Field.Type == EOpenPocketBaseFieldType::File ||
                Field.Type == EOpenPocketBaseFieldType::Relation)
            {
                FFieldAccessor& Value = Accessors.AddDefaulted_GetRef();
                Value.Field = &Field;
                Value.Name = BaseFieldName + TEXT("Value");
                Value.Type = ValueReferenceType(Field);
                Value.StableKey = Field.Id + TEXT(":value");
                ++AccessorNameCounts.FindOrAdd(Value.Name);
            }
        }

        OutHeader.Append(TEXT("namespace Fields\n{\n"));
        for (FFieldAccessor& Accessor : Accessors)
        {
            Accessor.Name = StableIdentifier(
                Accessor.Name,
                Accessor.StableKey,
                AccessorNameCounts.FindRef(Accessor.Name));
            AppendFieldAccessor(OutHeader, Accessor, Collection.Id);
        }
        OutHeader.Append(TEXT("}\n"));
        OutHeader.Append(TEXT("}\n\n"));
    }

    OutHeader.Append(TEXT("}\n"));
    return true;
}

bool FOpenPocketBaseSchemaCodeGenerator::FindDefaultOutputPath(
    const UOpenPocketBaseSchema& Schema,
    FString& OutProjectRelativePath,
    FText& OutError)
{
    OutProjectRelativePath.Reset();
    OutError = FText::GetEmpty();

    TArray<FString> BuildFiles;
    IFileManager::Get().FindFilesRecursive(
        BuildFiles,
        *ProjectSourceDirectory(),
        TEXT("*.Build.cs"),
        true,
        false);
    if (BuildFiles.IsEmpty())
    {
        OutError = LOCTEXT(
            "MissingGameModule",
            "No game C++ module was found. Add a C++ class to the project, then generate the accessors again.");
        return false;
    }

    BuildFiles.Sort();
    const FString PreferredBuildFile = FPaths::Combine(
        ProjectSourceDirectory(),
        FApp::GetProjectName(),
        FString::Printf(TEXT("%s.Build.cs"), FApp::GetProjectName()));
    const FString* SelectedBuildFile = BuildFiles.FindByPredicate(
        [&PreferredBuildFile](const FString& BuildFile)
        {
            return FPaths::IsSamePath(BuildFile, PreferredBuildFile);
        });
    if (SelectedBuildFile == nullptr)
    {
        SelectedBuildFile = &BuildFiles[0];
    }

    FString ModuleDirectory = FPaths::GetPath(*SelectedBuildFile);
    FPaths::MakePathRelativeTo(ModuleDirectory, *FPaths::ProjectDir());
    FString HeaderName = MakeIdentifier(Schema.GetName(), TEXT("PocketBase"));
    if (!HeaderName.EndsWith(TEXT("Schema")))
    {
        HeaderName += TEXT("Schema");
    }
    HeaderName += TEXT(".h");
    OutProjectRelativePath = FPaths::Combine(ModuleDirectory, TEXT("Generated"), HeaderName);
    FPaths::MakeStandardFilename(OutProjectRelativePath);
    return true;
}

bool FOpenPocketBaseSchemaCodeGenerator::WriteHeader(
    const UOpenPocketBaseSchema& Schema,
    const FString& RootNamespace,
    const FString& OutputPath,
    FString& OutAbsolutePath,
    FText& OutError)
{
    OutAbsolutePath.Reset();
    FString Header;
    if (!GenerateHeader(Schema, RootNamespace, Header, OutError))
    {
        return false;
    }
    if (OutputPath.IsEmpty())
    {
        OutError = LOCTEXT("MissingOutputPath", "Choose an output header path.");
        return false;
    }

    const FString CandidatePath = FPaths::IsRelative(OutputPath)
        ? FPaths::Combine(FPaths::ProjectDir(), OutputPath)
        : OutputPath;
    OutAbsolutePath = FPaths::ConvertRelativePathToFull(CandidatePath);
    FPaths::CollapseRelativeDirectories(OutAbsolutePath);
    FPaths::NormalizeFilename(OutAbsolutePath);

    FString SourceDirectory = FPaths::ConvertRelativePathToFull(ProjectSourceDirectory());
    FPaths::CollapseRelativeDirectories(SourceDirectory);
    FPaths::NormalizeDirectoryName(SourceDirectory);
    if (!FPaths::IsUnderDirectory(FPaths::GetPath(OutAbsolutePath), SourceDirectory))
    {
        OutError = LOCTEXT("OutputOutsideSource", "The generated header must be inside the project's Source directory.");
        OutAbsolutePath.Reset();
        return false;
    }
    if (!OutAbsolutePath.EndsWith(TEXT(".h"), ESearchCase::IgnoreCase))
    {
        OutError = LOCTEXT("OutputNotHeader", "The generated accessor path must end with .h.");
        OutAbsolutePath.Reset();
        return false;
    }

    FString ExistingHeader;
    if (FFileHelper::LoadFileToString(ExistingHeader, *OutAbsolutePath) && ExistingHeader == Header)
    {
        return true;
    }
    if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true) ||
        !FFileHelper::SaveStringToFile(
            Header,
            *OutAbsolutePath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FText::Format(
            LOCTEXT("WriteFailed", "Could not write the generated header to {0}."),
            FText::FromString(OutAbsolutePath));
        OutAbsolutePath.Reset();
        return false;
    }
    return true;
}

#undef LOCTEXT_NAMESPACE
