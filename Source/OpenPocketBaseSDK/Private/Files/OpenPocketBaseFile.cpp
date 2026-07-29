#include "OpenPocketBaseFile.h"

#include "GenericPlatform/GenericPlatformHttp.h"
#include "Misc/Paths.h"

namespace
{
FString ResolveFileName(const FString& FilePath, FString FileName)
{
    FileName.TrimStartAndEndInline();
    return FileName.IsEmpty() ? FPaths::GetCleanFilename(FilePath) : MoveTemp(FileName);
}

FString ResolveContentType(const FString& FileName, FString ContentType)
{
    ContentType.TrimStartAndEndInline();
    if (!ContentType.IsEmpty())
    {
        return ContentType;
    }
    ContentType = FGenericPlatformHttp::GetMimeType(FileName);
    return ContentType.IsEmpty() ? TEXT("application/octet-stream") : MoveTemp(ContentType);
}
}

FOpenPocketBaseFileInput FOpenPocketBaseFileInput::FromPath(
    FOpenPocketBaseFileFieldRef Field,
    FString FilePath,
    FString FileName,
    FString ContentType,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFileInput Input;
    Input.Field = MoveTemp(Field);
    Input.FilePath = MoveTemp(FilePath);
    Input.FileName = ResolveFileName(Input.FilePath, MoveTemp(FileName));
    Input.ContentType = ResolveContentType(Input.FileName, MoveTemp(ContentType));
    Input.Modifier = Modifier;
    Input.bUseFilePath = true;
    return Input;
}

FOpenPocketBaseFileInput FOpenPocketBaseFileInput::FromBytes(
    FOpenPocketBaseFileFieldRef Field,
    TArray<uint8> Bytes,
    FString FileName,
    FString ContentType,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFileInput Input;
    Input.Field = MoveTemp(Field);
    Input.Bytes = MoveTemp(Bytes);
    Input.FileName = ResolveFileName({}, MoveTemp(FileName));
    Input.ContentType = ResolveContentType(Input.FileName, MoveTemp(ContentType));
    Input.Modifier = Modifier;
    Input.bUseFilePath = false;
    return Input;
}

FOpenPocketBaseFileInput FOpenPocketBaseFileInput::DynamicFromPath(
    FString FieldName,
    FString FilePath,
    FString FileName,
    FString ContentType,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFileInput Input;
    Input.DynamicFieldName = MoveTemp(FieldName);
    Input.FilePath = MoveTemp(FilePath);
    Input.FileName = ResolveFileName(Input.FilePath, MoveTemp(FileName));
    Input.ContentType = ResolveContentType(Input.FileName, MoveTemp(ContentType));
    Input.Modifier = Modifier;
    Input.bUseFilePath = true;
    return Input;
}

FOpenPocketBaseFileInput FOpenPocketBaseFileInput::DynamicFromBytes(
    FString FieldName,
    TArray<uint8> Bytes,
    FString FileName,
    FString ContentType,
    const EOpenPocketBaseFieldModifier Modifier)
{
    FOpenPocketBaseFileInput Input;
    Input.DynamicFieldName = MoveTemp(FieldName);
    Input.Bytes = MoveTemp(Bytes);
    Input.FileName = ResolveFileName({}, MoveTemp(FileName));
    Input.ContentType = ResolveContentType(Input.FileName, MoveTemp(ContentType));
    Input.Modifier = Modifier;
    Input.bUseFilePath = false;
    return Input;
}

FString FOpenPocketBaseFileInput::GetFieldName() const
{
    FOpenPocketBaseFileFieldRef Current;
    return Field.ResolveCurrentAs(Current) ? Current.Name : DynamicFieldName;
}

bool FOpenPocketBaseFileInput::IsValid() const
{
    FOpenPocketBaseFileFieldRef Current;
    return Field.IsSet() ? Field.ResolveCurrentAs(Current) : !DynamicFieldName.IsEmpty();
}

bool FOpenPocketBaseFileInput::BelongsTo(
    const FOpenPocketBaseCollectionRef& Collection) const
{
    if (!DynamicFieldName.IsEmpty())
    {
        return true;
    }

    FOpenPocketBaseFileFieldRef Current;
    return Field.ResolveCurrentAs(Current) && Current.BelongsTo(Collection);
}
