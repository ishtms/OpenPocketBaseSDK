#include "OpenPocketBaseFile.h"

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
    Input.FileName = MoveTemp(FileName);
    Input.ContentType = MoveTemp(ContentType);
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
    Input.FileName = MoveTemp(FileName);
    Input.ContentType = MoveTemp(ContentType);
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
    Input.FileName = MoveTemp(FileName);
    Input.ContentType = MoveTemp(ContentType);
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
    Input.FileName = MoveTemp(FileName);
    Input.ContentType = MoveTemp(ContentType);
    Input.Modifier = Modifier;
    Input.bUseFilePath = false;
    return Input;
}

FString FOpenPocketBaseFileInput::GetFieldName() const
{
    return Field.IsSet() ? Field.Name : DynamicFieldName;
}

bool FOpenPocketBaseFileInput::IsValid() const
{
    return Field.IsSet()
        ? FOpenPocketBaseFileFieldRef::Accepts(Field)
        : !DynamicFieldName.IsEmpty();
}

bool FOpenPocketBaseFileInput::BelongsTo(
    const FOpenPocketBaseCollectionRef& Collection) const
{
    return FOpenPocketBaseFileFieldRef::Accepts(Field) && Field.BelongsTo(Collection);
}
