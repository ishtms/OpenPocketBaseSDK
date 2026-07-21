#include "Schema/OpenPocketBaseSchemaTestLibrary.h"

void UOpenPocketBaseSchemaTestLibrary::UseSchemaReferences(
    FOpenPocketBaseCollectionRef Collection,
    FOpenPocketBaseBooleanFieldRef Field)
{
}

FOpenPocketBaseSingleSelectFieldRef UOpenPocketBaseSchemaTestLibrary::PassThroughSingleSelectField(
    FOpenPocketBaseSingleSelectFieldRef Field)
{
    return Field;
}
