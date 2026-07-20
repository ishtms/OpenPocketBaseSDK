#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaPicker.h"
#include "Schema/OpenPocketBaseSchemaTestLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaCompilerValidationTest,
    "OpenPocketBase.Schema.CompilerRejectsStaleLiteralReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaCompilerValidationTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    Schema->SchemaId = FGuid(8, 5, 3, 2);

    FOpenPocketBaseSchemaCollection Tasks;
    Tasks.Id = TEXT("tasks_id");
    Tasks.Name = TEXT("sdk_tasks");
    Tasks.Type = EOpenPocketBaseCollectionType::Base;
    FOpenPocketBaseSchemaField Done;
    Done.Id = TEXT("done_id");
    Done.Name = TEXT("done");
    Done.Type = EOpenPocketBaseFieldType::Boolean;
    Tasks.Fields.Add(Done);
    Schema->Collections.Add(Tasks);

    FOpenPocketBaseCollectionRef Collection;
    FOpenPocketBaseBooleanFieldRef Field;
    TestTrue(TEXT("The fixture collection resolves"), Schema->MakeCollectionRef(TEXT("sdk_tasks"), Collection));
    TestTrue(TEXT("The fixture field resolves"), Schema->MakeTypedFieldRef(Collection, TEXT("done"), Field));

    const FName BlueprintName = MakeUniqueObjectName(
        GetTransientPackage(),
        UBlueprint::StaticClass(),
        TEXT("BP_OpenPocketBaseSchemaValidation"));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UObject::StaticClass(),
        GetTransientPackage(),
        BlueprintName,
        BPTYPE_Normal,
        NAME_None);
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        TEXT("TestSchemaReferences"),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, Graph, true, nullptr);

    UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
    Node->SetFromFunction(
        UOpenPocketBaseSchemaTestLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseSchemaTestLibrary,
                UseSchemaReferences)));
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    Graph->AddNode(Node, true, false);

    UEdGraphPin* CollectionPin = Node->FindPin(TEXT("Collection"), EGPD_Input);
    UEdGraphPin* FieldPin = Node->FindPin(TEXT("Field"), EGPD_Input);
    if (!TestNotNull(TEXT("The collection pin exists"), CollectionPin) ||
        !TestNotNull(TEXT("The typed field pin exists"), FieldPin))
    {
        return false;
    }

    const UEdGraphSchema* GraphSchema = Graph->GetSchema();
    const FString CollectionDefault =
        FOpenPocketBaseSchemaPickerModel::ExportCollectionDefault(Collection);
    GraphSchema->TrySetDefaultValue(*CollectionPin, CollectionDefault);
    TestEqual(
        TEXT("The collection reference is assigned"),
        CollectionPin->GetDefaultAsString(),
        CollectionDefault);
    const FString FieldDefault = FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
        FOpenPocketBaseBooleanFieldRef::StaticStruct(),
        Field);
    GraphSchema->TrySetDefaultValue(*FieldPin, FieldDefault);
    TestEqual(
        TEXT("The field reference is assigned"),
        FieldPin->GetDefaultAsString(),
        FieldDefault);

    FKismetEditorUtilities::CompileBlueprint(
        Blueprint,
        EBlueprintCompileOptions::SkipGarbageCollection);
    TestTrue(TEXT("Current schema references compile"), Blueprint->Status != BS_Error);

    Schema->Collections[0].Fields[0].bReadOnly = true;
    AddExpectedError(
        TEXT("is read-only"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    FKismetEditorUtilities::CompileBlueprint(
        Blueprint,
        EBlueprintCompileOptions::SkipGarbageCollection);
    TestEqual(TEXT("Fields that become read-only fail compilation"), Blueprint->Status, BS_Error);

    Schema->Collections[0].Fields[0].bReadOnly = false;
    Schema->Collections[0].Fields[0].Type = EOpenPocketBaseFieldType::Text;
    AddExpectedError(
        TEXT("no longer has the type required by this pin"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    FKismetEditorUtilities::CompileBlueprint(
        Blueprint,
        EBlueprintCompileOptions::SkipGarbageCollection);
    TestEqual(TEXT("Fields that change type fail compilation"), Blueprint->Status, BS_Error);

    Schema->Collections[0].Fields[0].Type = EOpenPocketBaseFieldType::Boolean;
    Schema->Collections[0].Fields.Reset();
    AddExpectedError(
        TEXT("no longer exists in the imported schema"),
        EAutomationExpectedErrorFlags::Contains,
        1);
    FKismetEditorUtilities::CompileBlueprint(
        Blueprint,
        EBlueprintCompileOptions::SkipGarbageCollection);
    TestEqual(TEXT("Stale field references fail compilation"), Blueprint->Status, BS_Error);
    return true;
}

#endif
