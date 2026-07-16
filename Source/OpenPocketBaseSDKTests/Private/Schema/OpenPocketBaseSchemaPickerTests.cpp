#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaPicker.h"

namespace
{
UOpenPocketBaseSchema* MakePickerSchema()
{
    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    Schema->SchemaId = FGuid(1, 1, 2, 3);

    FOpenPocketBaseSchemaCollection Tasks;
    Tasks.Id = TEXT("tasks_id");
    Tasks.Name = TEXT("sdk_tasks");
    Tasks.Type = EOpenPocketBaseCollectionType::Base;

    FOpenPocketBaseSchemaField Title;
    Title.Id = TEXT("title_id");
    Title.Name = TEXT("title");
    Title.Type = EOpenPocketBaseFieldType::Text;
    Title.bRequired = true;
    Tasks.Fields.Add(Title);

    FOpenPocketBaseSchemaField Done;
    Done.Id = TEXT("done_id");
    Done.Name = TEXT("done");
    Done.Type = EOpenPocketBaseFieldType::Boolean;
    Tasks.Fields.Add(Done);

    FOpenPocketBaseSchemaField Created;
    Created.Id = TEXT("created_id");
    Created.Name = TEXT("created");
    Created.Type = EOpenPocketBaseFieldType::Autodate;
    Created.bReadOnly = true;
    Tasks.Fields.Add(Created);

    FOpenPocketBaseSchemaCollection System;
    System.Id = TEXT("system_id");
    System.Name = TEXT("_system");
    System.Type = EOpenPocketBaseCollectionType::Base;
    System.bSystem = true;

    Schema->Collections = {Tasks, System};
    return Schema;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaPickerChoicesTest,
    "OpenPocketBase.Schema.PickerBuildsContextAwareChoices",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaPickerChoicesTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = MakePickerSchema();
    const TArray<UOpenPocketBaseSchema*> Schemas = {Schema};

    TArray<FOpenPocketBaseSchemaPickerChoice> Collections;
    FOpenPocketBaseSchemaPickerModel::BuildCollectionChoices(Schemas, false, Collections);
    TestEqual(TEXT("System collections stay out of the normal picker"), Collections.Num(), 1);
    if (Collections.IsEmpty())
    {
        return false;
    }
    TestEqual(TEXT("Collection labels use the PocketBase name"), Collections[0].Label.ToString(), FString(TEXT("sdk_tasks")));

    FOpenPocketBaseFieldPickerFilter BooleanFilter;
    BooleanFilter.Collection = &Collections[0].Collection;
    BooleanFilter.ReferenceStruct = FOpenPocketBaseBooleanFieldRef::StaticStruct();
    TArray<FOpenPocketBaseSchemaPickerChoice> BooleanFields;
    FOpenPocketBaseSchemaPickerModel::BuildFieldChoices(Schemas, BooleanFilter, BooleanFields);
    TestEqual(TEXT("Boolean pins only offer Boolean fields"), BooleanFields.Num(), 1);
    if (!BooleanFields.IsEmpty())
    {
        TestEqual(TEXT("The matching Boolean field is offered"), BooleanFields[0].Field.Name, FString(TEXT("done")));
        TestTrue(TEXT("Search includes the collection and field"), BooleanFields[0].SearchText.Contains(TEXT("sdk_tasks.done")));
    }

    FOpenPocketBaseFieldPickerFilter WritableFilter;
    WritableFilter.Collection = &Collections[0].Collection;
    WritableFilter.ReferenceStruct = FOpenPocketBaseAnyFieldRef::StaticStruct();
    WritableFilter.bWritableOnly = true;
    TArray<FOpenPocketBaseSchemaPickerChoice> WritableFields;
    FOpenPocketBaseSchemaPickerModel::BuildFieldChoices(Schemas, WritableFilter, WritableFields);
    TestEqual(TEXT("Read-only fields stay out of write pickers"), WritableFields.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaPickerSerializationTest,
    "OpenPocketBase.Schema.PickerSerializesStableReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaPickerSerializationTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = MakePickerSchema();
    FOpenPocketBaseCollectionRef Collection;
    TestTrue(TEXT("The fixture collection resolves"), Schema->MakeCollectionRef(TEXT("sdk_tasks"), Collection));
    FOpenPocketBaseBooleanFieldRef Done;
    TestTrue(TEXT("The fixture field resolves"), Schema->MakeTypedFieldRef(Collection, TEXT("done"), Done));

    const FString CollectionDefault = FOpenPocketBaseSchemaPickerModel::ExportCollectionDefault(Collection);
    FOpenPocketBaseCollectionRef ParsedCollection;
    TestTrue(TEXT("Collection defaults round trip"), FOpenPocketBaseSchemaPickerModel::ParseCollectionDefault(CollectionDefault, ParsedCollection));
    TestEqual(TEXT("Collection stable IDs survive serialization"), ParsedCollection.CollectionId, Collection.CollectionId);

    const FString FieldDefault = FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
        FOpenPocketBaseBooleanFieldRef::StaticStruct(),
        Done);
    FOpenPocketBaseFieldRef ParsedField;
    TestTrue(
        TEXT("Typed field defaults round trip"),
        FOpenPocketBaseSchemaPickerModel::ParseFieldDefault(
            FOpenPocketBaseBooleanFieldRef::StaticStruct(),
            FieldDefault,
            ParsedField));
    TestEqual(TEXT("Field stable IDs survive serialization"), ParsedField.FieldId, FString(TEXT("done_id")));

    FText ValidationMessage;
    TestEqual(
        TEXT("Current field references validate"),
        FOpenPocketBaseSchemaPickerModel::ValidateField(
            FOpenPocketBaseBooleanFieldRef::StaticStruct(),
            ParsedField,
            ValidationMessage),
        EOpenPocketBaseSchemaReferenceStatus::Valid);

    Schema->Collections[0].Fields.RemoveAt(1);
    TestEqual(
        TEXT("Deleted fields become stale"),
        FOpenPocketBaseSchemaPickerModel::ValidateField(
            FOpenPocketBaseBooleanFieldRef::StaticStruct(),
            ParsedField,
            ValidationMessage),
        EOpenPocketBaseSchemaReferenceStatus::MissingField);
    TestFalse(TEXT("Stale references explain the problem"), ValidationMessage.IsEmpty());
    return true;
}

#endif
