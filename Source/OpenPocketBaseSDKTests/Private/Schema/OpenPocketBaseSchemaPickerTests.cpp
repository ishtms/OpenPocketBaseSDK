#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseProjectSettings.h"
#include "OpenPocketBaseRecordLibrary.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaPicker.h"
#include "Schema/OpenPocketBaseSchemaTestLibrary.h"

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
    FOpenPocketBaseSchemaPickerModel::BuildCollectionChoices(
        Schemas,
        FOpenPocketBaseCollectionRef::StaticStruct(),
        false,
        EOpenPocketBaseCollectionRequirement::Any,
        Collections);
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

    FOpenPocketBaseSchemaCollection Users;
    Users.Id = TEXT("users_id");
    Users.Name = TEXT("sdk_users");
    Users.Type = EOpenPocketBaseCollectionType::Auth;
    Schema->Collections.Add(Users);
    FOpenPocketBaseSchemaCollection Summary;
    Summary.Id = TEXT("summary_id");
    Summary.Name = TEXT("task_summary");
    Summary.Type = EOpenPocketBaseCollectionType::View;
    Schema->Collections.Add(Summary);

    TArray<FOpenPocketBaseSchemaPickerChoice> WritableCollections;
    FOpenPocketBaseSchemaPickerModel::BuildCollectionChoices(
        Schemas,
        FOpenPocketBaseCollectionRef::StaticStruct(),
        false,
        EOpenPocketBaseCollectionRequirement::Writable,
        WritableCollections);
    TestEqual(
        TEXT("Write operations offer base and auth collections"),
        WritableCollections.Num(),
        2);

    TArray<FOpenPocketBaseSchemaPickerChoice> AuthCollections;
    FOpenPocketBaseSchemaPickerModel::BuildCollectionChoices(
        Schemas,
        FOpenPocketBaseCollectionRef::StaticStruct(),
        false,
        EOpenPocketBaseCollectionRequirement::Auth,
        AuthCollections);
    TestEqual(TEXT("Auth operations offer only auth collections"), AuthCollections.Num(), 1);
    if (!AuthCollections.IsEmpty())
    {
        TestEqual(
            TEXT("The auth collection is offered"),
            AuthCollections[0].Collection.Name,
            FString(TEXT("sdk_users")));
    }

    UOpenPocketBaseProjectSettings* Settings = NewObject<UOpenPocketBaseProjectSettings>();
    Settings->DefaultProfile = TEXT("Local");
    FOpenPocketBaseProjectProfile Profile;
    Profile.Name = TEXT("Local");
    Profile.Schema = Schema;
    Settings->Profiles.Add(Profile);
    TArray<UOpenPocketBaseSchema*> PreferredSchemas;
    FOpenPocketBaseSchemaPickerModel::ChooseProfileSchemas(
        *Settings,
        {},
        PreferredSchemas);
    TestEqual(TEXT("The default profile avoids scanning unrelated schemas"), PreferredSchemas.Num(), 1);
    if (PreferredSchemas.Num() == 1)
    {
        TestEqual(TEXT("The picker uses the profile schema"), PreferredSchemas[0], Schema);
    }
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
            false,
            ValidationMessage),
        EOpenPocketBaseSchemaReferenceStatus::Valid);

    Schema->Collections[0].Fields.RemoveAt(1);
    TestEqual(
        TEXT("Deleted fields become stale"),
        FOpenPocketBaseSchemaPickerModel::ValidateField(
            FOpenPocketBaseBooleanFieldRef::StaticStruct(),
            ParsedField,
            false,
            ValidationMessage),
        EOpenPocketBaseSchemaReferenceStatus::MissingField);
    TestFalse(TEXT("Stale references explain the problem"), ValidationMessage.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaPickerCurrentDefinitionTest,
    "OpenPocketBase.Schema.PickerValidatesCurrentDefinitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaPickerCurrentDefinitionTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = MakePickerSchema();
    FOpenPocketBaseCollectionRef Collection;
    FOpenPocketBaseBooleanFieldRef Done;
    TestTrue(TEXT("The collection resolves"), Schema->MakeCollectionRef(TEXT("sdk_tasks"), Collection));
    TestTrue(TEXT("The Boolean field resolves"), Schema->MakeTypedFieldRef(Collection, TEXT("done"), Done));

    FText Message;
    Schema->Collections[0].Fields[1].Type = EOpenPocketBaseFieldType::Text;
    TestEqual(
        TEXT("A changed field type invalidates an old typed reference"),
        FOpenPocketBaseSchemaPickerModel::ValidateField(
            FOpenPocketBaseBooleanFieldRef::StaticStruct(), Done, false, Message),
        EOpenPocketBaseSchemaReferenceStatus::WrongFieldType);

    Schema->Collections[0].Fields[1].Type = EOpenPocketBaseFieldType::Boolean;
    Schema->Collections[0].Fields[1].bReadOnly = true;
    TestEqual(
        TEXT("A field that became read-only invalidates a write reference"),
        FOpenPocketBaseSchemaPickerModel::ValidateField(
            FOpenPocketBaseBooleanFieldRef::StaticStruct(), Done, true, Message),
        EOpenPocketBaseSchemaReferenceStatus::ReadOnlyField);

    Schema->Collections[0].Type = EOpenPocketBaseCollectionType::View;
    FOpenPocketBaseWritableCollectionRef Writable;
    static_cast<FOpenPocketBaseCollectionRef&>(Writable) = Collection;
    TestEqual(
        TEXT("A collection that became a view invalidates a writable reference"),
        FOpenPocketBaseSchemaPickerModel::ValidateCollection(
            FOpenPocketBaseWritableCollectionRef::StaticStruct(),
            Writable,
            EOpenPocketBaseCollectionRequirement::Any,
            Message),
        EOpenPocketBaseSchemaReferenceStatus::WrongCollectionType);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseConnectedSelectPickerTest,
    "OpenPocketBase.Schema.PickerResolvesConnectedSelectFields",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseConnectedSelectPickerTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = MakePickerSchema();
    FOpenPocketBaseSchemaField Status;
    Status.Id = TEXT("status_id");
    Status.Name = TEXT("status");
    Status.Type = EOpenPocketBaseFieldType::Select;
    Status.Choices = {TEXT("todo"), TEXT("doing"), TEXT("done")};
    Schema->Collections[0].Fields.Add(Status);

    FOpenPocketBaseCollectionRef Collection;
    FOpenPocketBaseSingleSelectFieldRef StatusRef;
    TestTrue(TEXT("The fixture collection resolves"), Schema->MakeCollectionRef(TEXT("sdk_tasks"), Collection));
    TestTrue(TEXT("The fixture select field resolves"), Schema->MakeTypedFieldRef(Collection, TEXT("status"), StatusRef));

    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UObject::StaticClass(),
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UBlueprint::StaticClass(),
            TEXT("BP_OpenPocketBaseConnectedSelect")),
        BPTYPE_Normal,
        NAME_None);
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        TEXT("ConnectedSelect"),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, Graph, true, nullptr);

    UK2Node_CallFunction* SourceNode = NewObject<UK2Node_CallFunction>(Graph);
    SourceNode->SetFromFunction(
        UOpenPocketBaseSchemaTestLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseSchemaTestLibrary,
                PassThroughSingleSelectField)));
    SourceNode->CreateNewGuid();
    SourceNode->PostPlacedNewNode();
    SourceNode->AllocateDefaultPins();
    Graph->AddNode(SourceNode, true, false);
    SourceNode->FindPinChecked(TEXT("Field"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
            FOpenPocketBaseSingleSelectFieldRef::StaticStruct(),
            StatusRef);

    UK2Node_CallFunction* SelectNode = NewObject<UK2Node_CallFunction>(Graph);
    SelectNode->SetFromFunction(
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseRecordLibrary,
                WithSingleSelectField)));
    SelectNode->CreateNewGuid();
    SelectNode->PostPlacedNewNode();
    SelectNode->AllocateDefaultPins();
    Graph->AddNode(SelectNode, true, false);

    const UEdGraphSchema_K2* GraphSchema = GetDefault<UEdGraphSchema_K2>();
    TestTrue(
        TEXT("A select field can flow through a connected Blueprint value"),
        GraphSchema->TryCreateConnection(
            SourceNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
            SelectNode->FindPin(TEXT("Field"), EGPD_Input)));

    UEdGraphPin* ValuePin = SelectNode->FindPin(TEXT("Value"), EGPD_Input);
    TestNotNull(TEXT("The select value pin exists"), ValuePin);
    FOpenPocketBaseFieldRef Resolved;
    TestTrue(
        TEXT("The select choice picker resolves its connected field"),
        ValuePin != nullptr &&
            FOpenPocketBaseSchemaPickerModel::ResolveFieldFromPinContext(
                *ValuePin,
                TEXT("Field"),
                Resolved));
    TestEqual(TEXT("Connected select choices come from the current schema"), Resolved.Choices.Num(), 3);
    TestTrue(TEXT("Connected select choices include doing"), Resolved.Choices.Contains(TEXT("doing")));
    return true;
}

#endif
