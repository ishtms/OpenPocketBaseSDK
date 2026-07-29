#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AsyncActions/OpenPocketBaseRecordAsyncActions.h"
#include "AsyncActions/OpenPocketBaseBatchAsyncAction.h"
#include "AsyncActions/OpenPocketBaseFileAsyncActions.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseFilterLibrary.h"
#include "OpenPocketBaseClientLibrary.h"
#include "OpenPocketBaseFileLibrary.h"
#include "OpenPocketBaseAdminTypes.h"
#include "OpenPocketBaseBatchLibrary.h"
#include "OpenPocketBaseRecordLibrary.h"
#include "OpenPocketBaseRecord.h"
#include "OpenPocketBaseQueryLibrary.h"
#include "OpenPocketBaseRealtimeLibrary.h"
#include "OpenPocketBaseSchema.h"
#include "OpenPocketBaseSchemaPicker.h"
#include "OpenPocketBaseSubsystem.h"
#include "UObject/Field.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"

namespace
{
UK2Node_AsyncAction* AddAsyncConsumerNode(
    UEdGraph* Graph,
    UClass* FactoryClass,
    const FName FactoryFunctionName)
{
    const UFunction* FactoryFunction = FactoryClass->FindFunctionByName(FactoryFunctionName);
    if (FactoryFunction == nullptr)
    {
        return nullptr;
    }

    UK2Node_AsyncAction* Node = NewObject<UK2Node_AsyncAction>(Graph);
    Node->InitializeProxyFromFunction(FactoryFunction);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    Graph->AddNode(Node, true, false);
    return Node;
}

UK2Node_CallFunction* AddFunctionConsumerNode(
    UEdGraph* Graph,
    UClass* OwnerClass,
    const FName FunctionName)
{
    UFunction* Function = OwnerClass != nullptr
        ? OwnerClass->FindFunctionByName(FunctionName)
        : nullptr;
    if (Function == nullptr)
    {
        return nullptr;
    }

    UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
    Node->SetFromFunction(Function);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    Graph->AddNode(Node, true, false);
    return Node;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintValueApiTest,
    "OpenPocketBase.Blueprint.Values.UsesComposablePureNodes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintValueApiTest::RunTest(const FString& Parameters)
{
    const auto TestPureFunction = [this](UClass* Library, const FName Name)
    {
        const UFunction* Function = Library->FindFunctionByName(Name);
        TestNotNull(*FString::Printf(TEXT("%s exists"), *Name.ToString()), Function);
        if (Function != nullptr)
        {
            TestTrue(
                *FString::Printf(TEXT("%s is pure"), *Name.ToString()),
                Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
        }
    };

    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("StringFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("StringArrayFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("NumberFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("BooleanFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("DateFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("NullFilter"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("AndFilters"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("OrFilters"));
    TestPureFunction(UOpenPocketBaseFilterLibrary::StaticClass(), TEXT("DynamicFilter"));

    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("NewRecordBody"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithStringField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithNumberField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithBooleanField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithNullField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithStringArrayField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithDateField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithJsonField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithGeoPointField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithSingleSelectField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithMultipleSelectField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithSingleRelationField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithMultipleRelationField"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithRelationRecord"));
    TestPureFunction(UOpenPocketBaseRecordLibrary::StaticClass(), TEXT("WithRelationRecords"));
    TestPureFunction(UOpenPocketBaseQueryLibrary::StaticClass(), TEXT("SelectField"));
    TestPureFunction(UOpenPocketBaseQueryLibrary::StaticClass(), TEXT("SelectTextExcerpt"));
    TestPureFunction(UOpenPocketBaseQueryLibrary::StaticClass(), TEXT("SelectExpandedRecord"));

    TestPureFunction(UOpenPocketBaseBatchLibrary::StaticClass(), TEXT("NewBatch"));
    TestPureFunction(UOpenPocketBaseBatchLibrary::StaticClass(), TEXT("WithCreate"));
    TestPureFunction(UOpenPocketBaseBatchLibrary::StaticClass(), TEXT("WithUpdate"));
    TestPureFunction(UOpenPocketBaseBatchLibrary::StaticClass(), TEXT("WithUpsert"));
    TestPureFunction(UOpenPocketBaseBatchLibrary::StaticClass(), TEXT("WithDelete"));

    TestNotNull(
        TEXT("Batch operation results say whether a record was returned"),
        FOpenPocketBaseBatchOperationResult::StaticStruct()->FindPropertyByName(
            TEXT("bHasReturnedRecord")));
    TestNull(
        TEXT("The ambiguous batch result record flag is removed"),
        FOpenPocketBaseBatchOperationResult::StaticStruct()->FindPropertyByName(TEXT("bHasRecord")));

    TestNull(
        TEXT("The mutable filter accumulator is no longer exposed"),
        UOpenPocketBaseFilterLibrary::StaticClass()->FindFunctionByName(TEXT("AddBooleanParameter")));
    TestNull(
        TEXT("Manual filter binding is no longer part of the normal Blueprint flow"),
        UOpenPocketBaseFilterLibrary::StaticClass()->FindFunctionByName(TEXT("BindFilter")));
    TestNull(
        TEXT("Record body mutation is no longer exposed"),
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(TEXT("SetRecordBodyStringField")));
    TestNull(
        TEXT("Excerpt syntax is no longer assembled from raw field names"),
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(TEXT("MakeExcerptField")));
    TestNull(
        TEXT("Batch mutation is no longer exposed"),
        UOpenPocketBaseBatchLibrary::StaticClass()->FindFunctionByName(TEXT("AddCreate")));
    TestNull(
        TEXT("Batch reset is unnecessary for value flows"),
        UOpenPocketBaseBatchLibrary::StaticClass()->FindFunctionByName(TEXT("Clear")));

    const FProperty* FilterProperty =
        FOpenPocketBaseListOptions::StaticStruct()->FindPropertyByName(TEXT("Filter"));
    const FStructProperty* FilterStructProperty = CastField<FStructProperty>(FilterProperty);
    TestNotNull(TEXT("List options accept a filter value"), FilterStructProperty);
    if (FilterStructProperty != nullptr)
    {
        TestEqual(
            TEXT("List options use the Open PocketBase filter type"),
            FilterStructProperty->Struct->GetFName(),
            FName(TEXT("OpenPocketBaseFilter")));
    }

    const FProperty* RealtimeFilterProperty =
        FOpenPocketBaseRealtimeOptions::StaticStruct()->FindPropertyByName(TEXT("Filter"));
    const FStructProperty* RealtimeFilterStructProperty =
        CastField<FStructProperty>(RealtimeFilterProperty);
    TestNotNull(TEXT("Realtime options accept a filter value"), RealtimeFilterStructProperty);
    if (RealtimeFilterStructProperty != nullptr)
    {
        TestEqual(
            TEXT("Realtime options use the Open PocketBase filter type"),
            RealtimeFilterStructProperty->Struct->GetFName(),
            FName(TEXT("OpenPocketBaseFilter")));
    }

    const UFunction* CollectionFunction = UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, Collection));
    const FStructProperty* CollectionReferenceParameter = CollectionFunction != nullptr
        ? CastField<FStructProperty>(CollectionFunction->FindPropertyByName(TEXT("Reference")))
        : nullptr;
    TestNotNull(TEXT("Collection uses a schema picker parameter"), CollectionReferenceParameter);
    if (CollectionReferenceParameter != nullptr)
    {
        TestEqual(
            TEXT("Collection accepts a stable collection reference"),
            CollectionReferenceParameter->Struct->GetFName(),
            FOpenPocketBaseCollectionRef::StaticStruct()->GetFName());
    }
    TestNotNull(
        TEXT("Collection handles retain their schema reference"),
        FOpenPocketBaseCollection::StaticStruct()->FindPropertyByName(TEXT("Reference")));
    TestNull(
        TEXT("Collection handles do not expose an untyped name"),
        FOpenPocketBaseCollection::StaticStruct()->FindPropertyByName(TEXT("Name")));

    TestNull(
        TEXT("Blueprint exposes one collection entry point"),
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, WritableCollection)));

    const UFunction* CreateRecord = UOpenPocketBaseCreateRecordAsyncAction::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseCreateRecordAsyncAction, CreateRecord));
    const FStructProperty* CreateCollection = CreateRecord != nullptr
        ? CastField<FStructProperty>(CreateRecord->FindPropertyByName(TEXT("Collection")))
        : nullptr;
    TestNotNull(TEXT("Create Record accepts the unified collection value"), CreateCollection);
    if (CreateCollection != nullptr)
    {
        TestEqual(
            TEXT("Create Record uses the unified collection handle"),
            CreateCollection->Struct->GetFName(),
            FOpenPocketBaseCollection::StaticStruct()->GetFName());
    }

    TestNull(
        TEXT("Blueprint does not require a second auth collection node"),
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, AuthCollection)));

    const UFunction* WithStringField =
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithStringField));
    const FStructProperty* TextFieldParameter = WithStringField != nullptr
        ? CastField<FStructProperty>(WithStringField->FindPropertyByName(TEXT("Field")))
        : nullptr;
    TestNotNull(TEXT("With String Field exposes a schema field picker"), TextFieldParameter);
    if (TextFieldParameter != nullptr)
    {
        TestEqual(
            TEXT("Ordinary string writes accept text fields instead of every string-like field"),
            TextFieldParameter->Struct->GetName(),
            FString(TEXT("OpenPocketBaseTextFieldRef")));
    }

    const UFunction* BatchCreate = UOpenPocketBaseBatchLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseBatchLibrary, WithCreate));
    const FStructProperty* BatchCollection = BatchCreate != nullptr
        ? CastField<FStructProperty>(BatchCreate->FindPropertyByName(TEXT("Collection")))
        : nullptr;
    TestNotNull(TEXT("Batch Create exposes a collection input"), BatchCollection);
    if (BatchCollection != nullptr)
    {
        TestEqual(
            TEXT("Batch builders use the same collection handle as CRUD"),
            BatchCollection->Struct->GetFName(),
            FOpenPocketBaseCollection::StaticStruct()->GetFName());
    }
    const UFunction* SendBatch = UOpenPocketBaseSendBatchAsyncAction::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseSendBatchAsyncAction, SendBatch));
    TestNotNull(TEXT("Send Batch is available"), SendBatch);
    if (SendBatch != nullptr)
    {
        TestNull(
            TEXT("Send Batch gets its client from the collection-backed batch value"),
            SendBatch->FindPropertyByName(TEXT("PocketBaseClient")));
    }

    const UFunction* PasswordLogin = UOpenPocketBasePasswordAuthAsyncAction::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBasePasswordAuthAsyncAction, LogInWithPassword));
    const FStructProperty* PasswordAuthCollection = PasswordLogin != nullptr
        ? CastField<FStructProperty>(PasswordLogin->FindPropertyByName(TEXT("AuthCollection")))
        : nullptr;
    TestNotNull(TEXT("Password login accepts the unified collection value"), PasswordAuthCollection);
    if (PasswordAuthCollection != nullptr)
    {
        TestEqual(
            TEXT("Password login uses the unified collection handle"),
            PasswordAuthCollection->Struct->GetFName(),
            FOpenPocketBaseCollection::StaticStruct()->GetFName());
    }

    const auto TestTypedArray = [this](
        const UScriptStruct* OptionsType,
        const FName PropertyName,
        const UScriptStruct* ElementType)
    {
        const FArrayProperty* Array = CastField<FArrayProperty>(
            OptionsType->FindPropertyByName(PropertyName));
        const FStructProperty* Element = Array != nullptr
            ? CastField<FStructProperty>(Array->Inner)
            : nullptr;
        TestNotNull(*FString::Printf(TEXT("%s uses typed values"), *PropertyName.ToString()), Element);
        if (Element != nullptr)
        {
            TestEqual(
                *FString::Printf(TEXT("%s uses the expected query type"), *PropertyName.ToString()),
                Element->Struct->GetFName(),
                ElementType->GetFName());
        }
    };
    TestTypedArray(FOpenPocketBaseRecordOptions::StaticStruct(), TEXT("Expand"), FOpenPocketBaseExpand::StaticStruct());
    TestTypedArray(FOpenPocketBaseRecordOptions::StaticStruct(), TEXT("Fields"), FOpenPocketBaseFieldSelection::StaticStruct());
    TestTypedArray(FOpenPocketBaseListOptions::StaticStruct(), TEXT("Sort"), FOpenPocketBaseSort::StaticStruct());
    TestTypedArray(FOpenPocketBaseListOptions::StaticStruct(), TEXT("Expand"), FOpenPocketBaseExpand::StaticStruct());
    TestTypedArray(FOpenPocketBaseListOptions::StaticStruct(), TEXT("Fields"), FOpenPocketBaseFieldSelection::StaticStruct());
    TestTypedArray(FOpenPocketBaseRealtimeOptions::StaticStruct(), TEXT("Expand"), FOpenPocketBaseExpand::StaticStruct());
    TestTypedArray(FOpenPocketBaseRealtimeOptions::StaticStruct(), TEXT("Fields"), FOpenPocketBaseFieldSelection::StaticStruct());

    TestNotNull(
        TEXT("File inputs retain their schema field"),
        FOpenPocketBaseFileInput::StaticStruct()->FindPropertyByName(TEXT("Field")));
    TestNull(
        TEXT("File inputs do not expose an untyped field name"),
        FOpenPocketBaseFileInput::StaticStruct()->FindPropertyByName(TEXT("FieldName")));
    const UFunction* FileFromPath = UOpenPocketBaseFileLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFileLibrary, FileFromPath));
    const FStructProperty* FileFieldParameter = FileFromPath != nullptr
        ? CastField<FStructProperty>(FileFromPath->FindPropertyByName(TEXT("Field")))
        : nullptr;
    TestNotNull(TEXT("File From Path has a file-field picker"), FileFieldParameter);
    if (FileFieldParameter != nullptr)
    {
        TestEqual(
            TEXT("File From Path accepts only file fields"),
            FileFieldParameter->Struct->GetFName(),
            FOpenPocketBaseFileFieldRef::StaticStruct()->GetFName());
    }
    TestTrue(
        TEXT("File From Path derives the file name by default"),
        FileFromPath != nullptr && FileFromPath->HasMetaData(TEXT("CPP_Default_FileName")));
    TestTrue(
        TEXT("File From Path infers the content type by default"),
        FileFromPath != nullptr && FileFromPath->HasMetaData(TEXT("CPP_Default_ContentType")));
    TestEqual(
        TEXT("File From Path replaces the field by default"),
        FileFromPath != nullptr ? FileFromPath->GetMetaData(TEXT("CPP_Default_Modifier")) : FString(),
        FString(TEXT("Replace")));

    FOpenPocketBaseFileFieldRef AttachmentField;
    const FOpenPocketBaseFileInput InferredFile = UOpenPocketBaseFileLibrary::FileFromPath(
        AttachmentField,
        TEXT("C:/uploads/cover.png"),
        {},
        {},
        EOpenPocketBaseFieldModifier::Replace);
    TestEqual(TEXT("File names are derived from paths"), InferredFile.FileName, FString(TEXT("cover.png")));
    TestEqual(TEXT("File MIME types are inferred"), InferredFile.ContentType, FString(TEXT("image/png")));

    TestNotNull(
        TEXT("Single relations can be written from records"),
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(TEXT("WithRelationRecord")));
    TestNotNull(
        TEXT("Multiple relations can be written from records"),
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(TEXT("WithRelationRecords")));
    const UFunction* MultipleSelect = UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithMultipleSelectField));
    TestNotNull(
        TEXT("Multiple select writes expose append, prepend, remove, and replace"),
        MultipleSelect != nullptr ? MultipleSelect->FindPropertyByName(TEXT("Modifier")) : nullptr);
    const UFunction* MultipleRelation = UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithMultipleRelationField));
    TestNotNull(
        TEXT("Multiple relation writes expose append, prepend, remove, and replace"),
        MultipleRelation != nullptr ? MultipleRelation->FindPropertyByName(TEXT("Modifier")) : nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintNodeDefaultsTest,
    "OpenPocketBase.Blueprint.Nodes.ShowOptionsPins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintNodeDefaultsTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseAdminCollectionListOptions::StaticStruct();
    FOpenPocketBaseAdminLogListOptions::StaticStruct();
    FOpenPocketBaseDynamicAdminListOptions::StaticStruct();

    const auto IsSdkPackage = [](const UObject* Object)
    {
        const FString PackageName = Object->GetOutermost()->GetName();
        return PackageName == TEXT("/Script/OpenPocketBaseSDK") ||
            PackageName == TEXT("/Script/OpenPocketBaseSDKAdmin");
    };

    for (TObjectIterator<UClass> Class; Class; ++Class)
    {
        if (!IsSdkPackage(*Class))
        {
            continue;
        }

        for (TFieldIterator<UFunction> Function(*Class, EFieldIteratorFlags::ExcludeSuper);
             Function;
             ++Function)
        {
            for (TFieldIterator<FProperty> Property(*Function); Property; ++Property)
            {
                if (Property->HasAnyPropertyFlags(CPF_Parm) &&
                    Property->GetName().EndsWith(TEXT("Options")))
                {
                    TestFalse(
                        *FString::Printf(
                            TEXT("%s.%s keeps %s visible"),
                            *Class->GetName(),
                            *Function->GetName(),
                            *Property->GetName()),
                        Property->HasAnyPropertyFlags(CPF_AdvancedDisplay));
                }
            }

            if (Function->HasAnyFunctionFlags(FUNC_BlueprintCallable) &&
                CastField<FBoolProperty>(Function->GetReturnProperty()) != nullptr)
            {
                const FString ReturnDisplayName =
                    Function->GetMetaData(TEXT("ReturnDisplayName"));
                TestFalse(
                    *FString::Printf(
                        TEXT("%s.%s names its Boolean output"),
                        *Class->GetName(),
                        *Function->GetName()),
                    ReturnDisplayName.IsEmpty() ||
                        ReturnDisplayName == TEXT("Return Value"));
            }
        }
    }

    for (TObjectIterator<UScriptStruct> Struct; Struct; ++Struct)
    {
        if (!IsSdkPackage(*Struct))
        {
            continue;
        }

        for (TFieldIterator<FProperty> Property(*Struct); Property; ++Property)
        {
            if (Property->GetName().EndsWith(TEXT("Options")))
            {
                TestFalse(
                    *FString::Printf(
                        TEXT("%s keeps %s visible"),
                        *Struct->GetName(),
                        *Property->GetName()),
                    Property->HasAnyPropertyFlags(CPF_AdvancedDisplay));
            }
        }
    }

    const UFunction* RefreshSession =
        UOpenPocketBaseRefreshAuthAsyncAction::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRefreshAuthAsyncAction, RefreshAuth));
    if (RefreshSession != nullptr)
    {
        TestEqual(
            TEXT("Blueprint uses the user-facing Refresh Session name"),
            RefreshSession->GetMetaData(TEXT("DisplayName")),
            FString(TEXT("Refresh Session")));
    }

    const UFunction* OtpLogin =
        UOpenPocketBaseOtpAuthAsyncAction::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseOtpAuthAsyncAction,
                LogInWithOneTimePassword));
    if (OtpLogin != nullptr)
    {
        TestNotNull(
            TEXT("OTP login names the one-time password clearly"),
            FindFProperty<FProperty>(OtpLogin, TEXT("OneTimePassword")));
        TestNull(
            TEXT("OTP login does not expose a generic Password pin"),
            FindFProperty<FProperty>(OtpLogin, TEXT("Password")));
    }

    const UFunction* ConfirmPasswordReset =
        UOpenPocketBaseAccountAsyncAction::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                ConfirmPasswordReset));
    if (ConfirmPasswordReset != nullptr)
    {
        TestNotNull(
            TEXT("Password reset names the new password clearly"),
            FindFProperty<FProperty>(ConfirmPasswordReset, TEXT("NewPassword")));
        TestNotNull(
            TEXT("Password reset names its confirmation clearly"),
            FindFProperty<FProperty>(ConfirmPasswordReset, TEXT("ConfirmPassword")));
        TestNull(
            TEXT("Password reset does not expose the wire-level PasswordConfirm name"),
            FindFProperty<FProperty>(ConfirmPasswordReset, TEXT("PasswordConfirm")));
    }

    const UFunction* ConfirmEmailChange =
        UOpenPocketBaseAccountAsyncAction::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                ConfirmEmailChange));
    if (ConfirmEmailChange != nullptr)
    {
        TestNotNull(
            TEXT("Email change identifies the current password"),
            FindFProperty<FProperty>(ConfirmEmailChange, TEXT("CurrentPassword")));
        TestNull(
            TEXT("Email change does not expose an ambiguous Password pin"),
            FindFProperty<FProperty>(ConfirmEmailChange, TEXT("Password")));
    }

    const UFunction* BuildFileUrl =
        UOpenPocketBaseFileLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFileLibrary, TryBuildFileUrl));
    if (BuildFileUrl != nullptr)
    {
        TestEqual(
            TEXT("Try Build File URL exposes success and failure execution paths"),
            BuildFileUrl->GetMetaData(TEXT("ExpandBoolAsExecs")),
            FString(TEXT("ReturnValue")));
    }

    const UFunction* CurrentAuthRecord =
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, TryGetCurrentAuthRecord));
    const UFunction* CurrentSession =
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, TryGetCurrentSession));
    if (CurrentAuthRecord != nullptr)
    {
        TestEqual(
            TEXT("Get Current Auth Record exposes found and missing execution paths"),
            CurrentAuthRecord->GetMetaData(TEXT("ExpandBoolAsExecs")),
            FString(TEXT("ReturnValue")));
    }
    if (CurrentSession != nullptr)
    {
        TestEqual(
            TEXT("Get Current Session exposes authenticated and unauthenticated paths"),
            CurrentSession->GetMetaData(TEXT("ExpandBoolAsExecs")),
            FString(TEXT("ReturnValue")));
    }

    const UFunction* LegacyCurrentAuthRecord =
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, GetCurrentAuthRecord));
    const UFunction* LegacyCurrentSession =
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, GetCurrentSession));
    TestTrue(
        TEXT("Saved Get Current Auth Record nodes keep their original pin layout"),
        LegacyCurrentAuthRecord != nullptr &&
            !LegacyCurrentAuthRecord->HasMetaData(TEXT("ExpandBoolAsExecs")));
    TestTrue(
        TEXT("Saved Get Current Session nodes keep their original pin layout"),
        LegacyCurrentSession != nullptr &&
            !LegacyCurrentSession->HasMetaData(TEXT("ExpandBoolAsExecs")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintComplexValueApiTest,
    "OpenPocketBase.Blueprint.Values.SupportsComplexFields",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintComplexValueApiTest::RunTest(const FString& Parameters)
{
    TestNotNull(
        TEXT("Blueprint can opt into a runtime-defined collection"),
        UOpenPocketBaseClient::StaticClass()->FindFunctionByName(TEXT("DynamicCollection")));

    UClass* JsonLibrary = FindObject<UClass>(
        nullptr,
        TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseJsonValueLibrary"));
    TestNotNull(TEXT("The Blueprint JSON value library exists"), JsonLibrary);
    if (JsonLibrary != nullptr)
    {
        TestNotNull(TEXT("Blueprint can create JSON objects"), JsonLibrary->FindFunctionByName(TEXT("MakeJsonObject")));
        TestNotNull(TEXT("Blueprint can create JSON arrays"), JsonLibrary->FindFunctionByName(TEXT("MakeJsonArray")));
        TestNotNull(TEXT("Blueprint can create JSON strings"), JsonLibrary->FindFunctionByName(TEXT("JsonString")));
        TestNotNull(TEXT("Blueprint can create JSON numbers"), JsonLibrary->FindFunctionByName(TEXT("JsonNumber")));
        TestNotNull(TEXT("Blueprint can create JSON booleans"), JsonLibrary->FindFunctionByName(TEXT("JsonBoolean")));
        TestNotNull(TEXT("Blueprint can create JSON null"), JsonLibrary->FindFunctionByName(TEXT("JsonNull")));
        TestNotNull(TEXT("Blueprint can set JSON object properties"), JsonLibrary->FindFunctionByName(TEXT("SetJsonProperty")));
        TestNotNull(TEXT("Blueprint can append JSON array items"), JsonLibrary->FindFunctionByName(TEXT("AddJsonItem")));
    }

    const UFunction* WithJsonField =
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithJsonField));
    const FStructProperty* JsonValueParameter = WithJsonField != nullptr
        ? CastField<FStructProperty>(WithJsonField->FindPropertyByName(TEXT("Value")))
        : nullptr;
    TestNotNull(TEXT("JSON field writes expose a value input"), JsonValueParameter);
    if (JsonValueParameter != nullptr)
    {
        TestEqual(
            TEXT("JSON field writes accept every JSON root"),
            JsonValueParameter->Struct->GetName(),
            FString(TEXT("OpenPocketBaseJsonValue")));
    }

    const UFunction* WithSingleSelect =
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithSingleSelectField));
    const FProperty* SingleChoice = WithSingleSelect != nullptr
        ? WithSingleSelect->FindPropertyByName(TEXT("Value"))
        : nullptr;
    TestNotNull(TEXT("Single-select writes expose a choice input"), SingleChoice);
    if (SingleChoice != nullptr)
    {
        TestEqual(
            TEXT("Single-select choices stay linked to their field picker"),
            SingleChoice->GetMetaData(TEXT("OpenPocketBaseSelectField")),
            FString(TEXT("Field")));
    }

    const UFunction* WithMultipleSelect =
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithMultipleSelectField));
    const FStructProperty* MultipleChoices = WithMultipleSelect != nullptr
        ? CastField<FStructProperty>(WithMultipleSelect->FindPropertyByName(TEXT("Values")))
        : nullptr;
    TestNotNull(TEXT("Multiple-select writes expose a checklist value"), MultipleChoices);
    if (MultipleChoices != nullptr)
    {
        TestEqual(
            TEXT("Multiple-select writes use the select-values type"),
            MultipleChoices->Struct->GetName(),
            FString(TEXT("OpenPocketBaseSelectValues")));
        TestEqual(
            TEXT("Multiple-select choices stay linked to their field picker"),
            MultipleChoices->GetMetaData(TEXT("OpenPocketBaseSelectField")),
            FString(TEXT("Field")));
    }

    const UFunction* TryGetJsonField =
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(TEXT("TryGetJsonField"));
    TestNotNull(TEXT("Blueprint can read any JSON field value"), TryGetJsonField);
    const FStructProperty* JsonOutput = TryGetJsonField != nullptr
        ? CastField<FStructProperty>(TryGetJsonField->FindPropertyByName(TEXT("OutValue")))
        : nullptr;
    TestNotNull(TEXT("JSON field reads return a JSON value"), JsonOutput);
    if (JsonOutput != nullptr)
    {
        TestEqual(
            TEXT("JSON field reads preserve the root type"),
            JsonOutput->Struct->GetName(),
            FString(TEXT("OpenPocketBaseJsonValue")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintDiscoverabilityTest,
    "OpenPocketBase.Blueprint.Nodes.ExplainCoreWorkflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintDiscoverabilityTest::RunTest(const FString& Parameters)
{
    const TArray<TPair<UClass*, FName>> CoreFunctions = {
        {UOpenPocketBaseClientLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClientLibrary, InitializePocketBase)},
        {UOpenPocketBaseClient::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, Collection)},
        {UOpenPocketBaseRecordLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, NewRecordBody)},
        {UOpenPocketBaseRecordLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithStringField)},
        {UOpenPocketBaseRecordLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithJsonField)},
        {UOpenPocketBaseRecordLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithRelationRecord)},
        {UOpenPocketBaseRecordLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, TryGetStringField)},
        {UOpenPocketBaseFilterLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFilterLibrary, BooleanFilter)},
        {UOpenPocketBaseFilterLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFilterLibrary, AndFilters)},
        {UOpenPocketBaseQueryLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseQueryLibrary, SortDescending)},
        {UOpenPocketBaseQueryLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseQueryLibrary, SelectField)},
        {UOpenPocketBaseFileLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFileLibrary, FileFromPath)},
        {UOpenPocketBaseBatchLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseBatchLibrary, WithCreate)},
        {UOpenPocketBaseHealthAsyncAction::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseHealthAsyncAction, CheckHealth)},
        {UOpenPocketBaseListRecordsAsyncAction::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseListRecordsAsyncAction, ListRecords)},
        {UOpenPocketBaseCreateRecordAsyncAction::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseCreateRecordAsyncAction, CreateRecord)},
        {UOpenPocketBasePasswordAuthAsyncAction::StaticClass(), GET_FUNCTION_NAME_CHECKED(UOpenPocketBasePasswordAuthAsyncAction, LogInWithPassword)}};

    for (const TPair<UClass*, FName>& Entry : CoreFunctions)
    {
        const UFunction* Function = Entry.Key->FindFunctionByName(Entry.Value);
        TestNotNull(
            *FString::Printf(TEXT("Core node %s exists"), *Entry.Value.ToString()),
            Function);
        if (Function != nullptr)
        {
            TestFalse(
                *FString::Printf(TEXT("Core node %s explains when to use it"), *Entry.Value.ToString()),
                Function->GetMetaData(TEXT("ToolTip")).TrimStartAndEnd().IsEmpty());
            TestFalse(
                *FString::Printf(TEXT("Core node %s is easy to find"), *Entry.Value.ToString()),
                Function->GetMetaData(TEXT("Keywords")).TrimStartAndEnd().IsEmpty());
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintClientEntryApiTest,
    "OpenPocketBase.Blueprint.Client.UsesDirectEntryPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintClientEntryApiTest::RunTest(const FString& Parameters)
{
    UClass* Library = FindObject<UClass>(
        nullptr,
        TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseClientLibrary"));
    if (!TestNotNull(TEXT("The Blueprint client library exists"), Library))
    {
        return false;
    }

    const UFunction* Initialize = Library->FindFunctionByName(TEXT("InitializePocketBase"));
    TestNotNull(TEXT("Initialize PocketBase exists"), Initialize);
    if (Initialize != nullptr)
    {
        TestEqual(
            TEXT("Initialize PocketBase expands into success and failure execution paths"),
            Initialize->GetMetaData(TEXT("ExpandBoolAsExecs")),
            FString(TEXT("ReturnValue")));
        TestEqual(
            TEXT("Initialize PocketBase resolves its world automatically"),
            Initialize->GetMetaData(TEXT("WorldContext")),
            FString(TEXT("WorldContextObject")));
        TestEqual(
            TEXT("Initialize PocketBase hides its world pin"),
            Initialize->GetMetaData(TEXT("HidePin")),
            FString(TEXT("WorldContextObject")));
    }

    const UFunction* GetClient = Library->FindFunctionByName(TEXT("GetPocketBaseClient"));
    TestNotNull(TEXT("Get PocketBase Client exists"), GetClient);
    if (GetClient != nullptr)
    {
        TestTrue(
            TEXT("Get PocketBase Client is pure"),
            GetClient->HasAnyFunctionFlags(FUNC_BlueprintPure));
    }

    TestNotNull(
        TEXT("Advanced named client creation remains available"),
        Library->FindFunctionByName(TEXT("CreateNamedPocketBaseClient")));
    TestNotNull(
        TEXT("Advanced named client lookup remains available"),
        Library->FindFunctionByName(TEXT("GetNamedPocketBaseClient")));

    TestNull(
        TEXT("The subsystem no longer exposes client construction nodes"),
        UOpenPocketBaseSubsystem::StaticClass()->FindFunctionByName(TEXT("CreateClient")));
    TestNull(
        TEXT("The subsystem no longer exposes name-sensitive lookup nodes"),
        UOpenPocketBaseSubsystem::StaticClass()->FindFunctionByName(TEXT("GetClient")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintConsumerTest,
    "OpenPocketBase.Blueprint.Consumer.CompilesPublicNodes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintConsumerTest::RunTest(const FString& Parameters)
{
    const FName BlueprintName = MakeUniqueObjectName(
        GetTransientPackage(),
        UBlueprint::StaticClass(),
        TEXT("BP_OpenPocketBaseConsumer"));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UObject::StaticClass(),
        GetTransientPackage(),
        BlueprintName,
        BPTYPE_Normal,
        NAME_None);
    if (!TestNotNull(TEXT("A Blueprint-only consumer is created"), Blueprint))
    {
        return false;
    }

    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        TEXT("ExerciseOpenPocketBase"),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, Graph, true, nullptr);

    UK2Node_CallFunction* InitializeNode = NewObject<UK2Node_CallFunction>(Graph);
    InitializeNode->SetFromFunction(UOpenPocketBaseClientLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClientLibrary, InitializePocketBase)));
    InitializeNode->CreateNewGuid();
    InitializeNode->PostPlacedNewNode();
    InitializeNode->AllocateDefaultPins();
    Graph->AddNode(InitializeNode, true, false);

    UK2Node_CallFunction* GetClientNode = NewObject<UK2Node_CallFunction>(Graph);
    GetClientNode->SetFromFunction(UOpenPocketBaseClientLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClientLibrary, GetPocketBaseClient)));
    GetClientNode->CreateNewGuid();
    GetClientNode->PostPlacedNewNode();
    GetClientNode->AllocateDefaultPins();
    Graph->AddNode(GetClientNode, true, false);

    UK2Node_AsyncAction* HealthNode = AddAsyncConsumerNode(
        Graph,
        UOpenPocketBaseHealthAsyncAction::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseHealthAsyncAction, CheckHealth));
    TestNotNull(
        TEXT("Check Health is available as an async Blueprint node"),
        HealthNode);
    TestNotNull(
        TEXT("Check Health exposes the failure error"),
        HealthNode != nullptr ? HealthNode->FindPin(TEXT("Error"), EGPD_Output) : nullptr);
    TestNotNull(
        TEXT("Send Custom Route is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseCustomRouteAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseCustomRouteAsyncAction,
                SendCustomRoute)));

    UK2Node_AsyncAction* GetRecordNode = AddAsyncConsumerNode(
        Graph,
        UOpenPocketBaseGetRecordAsyncAction::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseGetRecordAsyncAction, GetRecord));
    TestNotNull(
        TEXT("Get Record is available as an async Blueprint node"),
        GetRecordNode);
    UEdGraphPin* CollectionPin =
        GetRecordNode != nullptr ? GetRecordNode->FindPin(TEXT("Collection"), EGPD_Input) : nullptr;
    TestNotNull(TEXT("Record actions accept a collection value"), CollectionPin);
    TestTrue(
        TEXT("Record actions use the Open PocketBase collection type"),
            CollectionPin != nullptr &&
            CollectionPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
            CollectionPin->PinType.PinSubCategoryObject != nullptr &&
            CollectionPin->PinType.PinSubCategoryObject->GetName() == TEXT("OpenPocketBaseCollection"));
    TestNull(
        TEXT("Record actions do not repeat the client pin"),
        GetRecordNode != nullptr
            ? GetRecordNode->FindPin(TEXT("PocketBaseClient"), EGPD_Input)
            : nullptr);

    UK2Node_CallFunction* CollectionNode = NewObject<UK2Node_CallFunction>(Graph);
    CollectionNode->SetFromFunction(UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, Collection)));
    CollectionNode->CreateNewGuid();
    CollectionNode->PostPlacedNewNode();
    CollectionNode->AllocateDefaultPins();
    Graph->AddNode(CollectionNode, true, false);
    UOpenPocketBaseSchema* CollectionSchema = NewObject<UOpenPocketBaseSchema>(
        GetTransientPackage(),
        TEXT("OpenPocketBaseCollectionSchema"));
    CollectionSchema->SchemaId = FGuid(89, 144, 233, 377);
    FOpenPocketBaseSchemaCollection CollectionDefinition;
    CollectionDefinition.Id = TEXT("tasks_id");
    CollectionDefinition.Name = TEXT("sdk_tasks");
    CollectionDefinition.Type = EOpenPocketBaseCollectionType::Base;
    FOpenPocketBaseSchemaCollection AuthCollectionDefinition;
    AuthCollectionDefinition.Id = TEXT("users_id");
    AuthCollectionDefinition.Name = TEXT("sdk_users");
    AuthCollectionDefinition.Type = EOpenPocketBaseCollectionType::Auth;
    CollectionSchema->Collections = {CollectionDefinition, AuthCollectionDefinition};
    FOpenPocketBaseCollectionRef CollectionReference;
    FOpenPocketBaseAuthCollectionRef AuthCollectionReference;
    TestTrue(
        TEXT("The consumer has a valid collection fixture"),
        CollectionSchema->MakeCollectionRef(CollectionDefinition.Id, CollectionReference));
    TestTrue(
        TEXT("The consumer has a valid auth collection fixture"),
        CollectionSchema->MakeTypedCollectionRef(
            AuthCollectionDefinition.Id,
            AuthCollectionReference));
    CollectionNode->FindPinChecked(TEXT("Reference"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportCollectionDefault(CollectionReference);
    UK2Node_CallFunction* AuthCollectionNode = NewObject<UK2Node_CallFunction>(Graph);
    AuthCollectionNode->SetFromFunction(UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, Collection)));
    AuthCollectionNode->CreateNewGuid();
    AuthCollectionNode->PostPlacedNewNode();
    AuthCollectionNode->AllocateDefaultPins();
    Graph->AddNode(AuthCollectionNode, true, false);
    AuthCollectionNode->FindPinChecked(TEXT("Reference"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportCollectionDefault(AuthCollectionReference);
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    TestTrue(
        TEXT("A retrieved client connects directly to Collection"),
        Schema->TryCreateConnection(
            GetClientNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
            CollectionNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input)));
    TestTrue(
        TEXT("A retrieved client connects directly to Auth Collection"),
        Schema->TryCreateConnection(
            GetClientNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
            AuthCollectionNode->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input)));
    TestTrue(
        TEXT("A Collection value connects directly to record actions"),
        Schema->TryCreateConnection(
            CollectionNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
            CollectionPin));

    UK2Node_CallFunction* SubscribeRecordsNode = NewObject<UK2Node_CallFunction>(Graph);
    SubscribeRecordsNode->SetFromFunction(
        UOpenPocketBaseRealtimeLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseRealtimeLibrary,
                SubscribeToRecords)));
    SubscribeRecordsNode->CreateNewGuid();
    SubscribeRecordsNode->PostPlacedNewNode();
    SubscribeRecordsNode->AllocateDefaultPins();
    Graph->AddNode(SubscribeRecordsNode, true, false);
    UEdGraphPin* RealtimeCollectionPin =
        SubscribeRecordsNode->FindPin(TEXT("Collection"), EGPD_Input);
    TestNotNull(
        TEXT("Subscribe to Records accepts a collection value"),
        RealtimeCollectionPin);
    TestNull(
        TEXT("Subscribe to Records does not repeat the client pin"),
        SubscribeRecordsNode->FindPin(TEXT("PocketBaseClient"), EGPD_Input));
    TestTrue(
        TEXT("A Collection value connects directly to realtime subscriptions"),
        Schema->TryCreateConnection(
            CollectionNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
            RealtimeCollectionPin));
    TestNotNull(
        TEXT("List Records is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseListRecordsAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseListRecordsAsyncAction, ListRecords)));
    TestNotNull(
        TEXT("Get Full Record List is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseGetFullListAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseGetFullListAsyncAction, GetFullList)));
    TestNotNull(
        TEXT("Get First Record is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseGetFirstRecordAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseGetFirstRecordAsyncAction, GetFirstRecord)));
    TestNotNull(
        TEXT("Create Record is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseCreateRecordAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseCreateRecordAsyncAction, CreateRecord)));
    TestNotNull(
        TEXT("Create Record with Files is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseCreateRecordWithFilesAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseCreateRecordWithFilesAsyncAction,
                CreateRecordWithFiles)));
    TestNotNull(
        TEXT("Update Record is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseUpdateRecordAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseUpdateRecordAsyncAction, UpdateRecord)));
    TestNotNull(
        TEXT("Update Record with Files is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseUpdateRecordWithFilesAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseUpdateRecordWithFilesAsyncAction,
                UpdateRecordWithFiles)));
    TestNotNull(
        TEXT("Delete Record is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseDeleteRecordAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseDeleteRecordAsyncAction, DeleteRecord)));
    TestNotNull(
        TEXT("Send Batch is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseSendBatchAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseSendBatchAsyncAction, SendBatch)));
    TestNotNull(
        TEXT("Get Protected File Token is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseGetFileTokenAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseGetFileTokenAsyncAction, GetProtectedFileToken)));
    TestNotNull(
        TEXT("Download File is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseDownloadFileAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseDownloadFileAsyncAction, DownloadFile)));
    TestNotNull(
        TEXT("Refresh Auth is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseRefreshAuthAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRefreshAuthAsyncAction, RefreshAuth)));
    TestNotNull(
        TEXT("Restore Session is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseRestoreSessionAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRestoreSessionAsyncAction, RestoreSession)));
    TestNotNull(
        TEXT("Log In with Password is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBasePasswordAuthAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UOpenPocketBasePasswordAuthAsyncAction, LogInWithPassword)));
    TestNotNull(
        TEXT("List Authentication Methods is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseListAuthMethodsAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseListAuthMethodsAsyncAction,
                ListAuthenticationMethods)));
    TestNotNull(
        TEXT("Request One-Time Password is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseRequestOtpAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseRequestOtpAsyncAction,
                RequestOneTimePassword)));
    TestNotNull(
        TEXT("Log In with One-Time Password is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseOtpAuthAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseOtpAuthAsyncAction,
                LogInWithOneTimePassword)));
    TestNotNull(
        TEXT("Begin Manual OAuth2 is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseBeginOAuth2AsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseBeginOAuth2AsyncAction,
                BeginManualOAuth2)));
    TestNotNull(
        TEXT("Complete Manual OAuth2 is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseCompleteOAuth2AsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseCompleteOAuth2AsyncAction,
                CompleteManualOAuth2)));
    TestNotNull(
        TEXT("OAuth exchange exposes an MFA Required terminal delegate"),
        UOpenPocketBaseCompleteOAuth2AsyncAction::StaticClass()->FindPropertyByName(
            TEXT("MfaRequired")));
    TestNotNull(
        TEXT("Log In with OAuth2 is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAssistedOAuth2AsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAssistedOAuth2AsyncAction,
                LogInWithOAuth2)));
    TestNotNull(
        TEXT("Assisted OAuth exposes an MFA Required terminal delegate"),
        UOpenPocketBaseAssistedOAuth2AsyncAction::StaticClass()->FindPropertyByName(
            TEXT("MfaRequired")));
    TestNotNull(
        TEXT("Password login exposes an MFA Required terminal delegate"),
        UOpenPocketBasePasswordAuthAsyncAction::StaticClass()->FindPropertyByName(
            TEXT("MfaRequired")));
    TestNotNull(
        TEXT("Request Password Reset is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                RequestPasswordReset)));
    TestNotNull(
        TEXT("Confirm Password Reset is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                ConfirmPasswordReset)));
    TestNotNull(
        TEXT("Request Verification is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                RequestVerification)));
    TestNotNull(
        TEXT("Confirm Verification is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                ConfirmVerification)));
    TestNotNull(
        TEXT("Request Email Change is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                RequestEmailChange)));
    TestNotNull(
        TEXT("Confirm Email Change is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                ConfirmEmailChange)));
    TestNotNull(
        TEXT("List Linked External Auths is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseListExternalAuthsAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseListExternalAuthsAsyncAction,
                ListLinkedExternalAuths)));
    TestNotNull(
        TEXT("Unlink External Auth is available as an async Blueprint node"),
        AddAsyncConsumerNode(
            Graph,
            UOpenPocketBaseAccountAsyncAction::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(
                UOpenPocketBaseAccountAsyncAction,
                UnlinkExternalAuth)));

    UK2Node_CallFunction* FieldNode = NewObject<UK2Node_CallFunction>(Graph);
    FieldNode->SetFromFunction(UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, GetStringFieldState)));
    FieldNode->CreateNewGuid();
    FieldNode->PostPlacedNewNode();
    FieldNode->AllocateDefaultPins();
    Graph->AddNode(FieldNode, true, false);

    UK2Node_CallFunction* BodyNode = NewObject<UK2Node_CallFunction>(Graph);
    BodyNode->SetFromFunction(UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithStringField)));
    BodyNode->CreateNewGuid();
    BodyNode->PostPlacedNewNode();
    BodyNode->AllocateDefaultPins();
    Graph->AddNode(BodyNode, true, false);

    UK2Node_CallFunction* FilterNode = NewObject<UK2Node_CallFunction>(Graph);
    FilterNode->SetFromFunction(UOpenPocketBaseFilterLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFilterLibrary, BooleanFilter)));
    FilterNode->CreateNewGuid();
    FilterNode->PostPlacedNewNode();
    FilterNode->AllocateDefaultPins();
    Graph->AddNode(FilterNode, true, false);

    UOpenPocketBaseSchema* TestSchema = NewObject<UOpenPocketBaseSchema>(
        GetTransientPackage(),
        TEXT("OpenPocketBaseConsumerSchema"));
    TestSchema->SchemaId = FGuid(144, 233, 377, 610);
    FOpenPocketBaseSchemaCollection TasksCollection;
    TasksCollection.Id = TEXT("tasks_id");
    TasksCollection.Name = TEXT("sdk_tasks");
    FOpenPocketBaseSchemaField TitleField;
    TitleField.Id = TEXT("title_id");
    TitleField.Name = TEXT("title");
    TitleField.Type = EOpenPocketBaseFieldType::Text;
    FOpenPocketBaseSchemaField DoneField;
    DoneField.Id = TEXT("done_id");
    DoneField.Name = TEXT("done");
    DoneField.Type = EOpenPocketBaseFieldType::Boolean;
    TasksCollection.Fields = {TitleField, DoneField};
    TestSchema->Collections = {TasksCollection};

    FOpenPocketBaseCollectionRef TasksRef;
    FOpenPocketBaseStringFieldRef TitleRef;
    FOpenPocketBaseBooleanFieldRef DoneRef;
    TestSchema->MakeCollectionRef(TasksCollection.Id, TasksRef);
    TestSchema->MakeTypedFieldRef(TasksRef, TitleField.Id, TitleRef);
    TestSchema->MakeTypedFieldRef(TasksRef, DoneField.Id, DoneRef);
    FieldNode->FindPinChecked(TEXT("Field"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
            FOpenPocketBaseStringFieldRef::StaticStruct(),
            TitleRef);
    BodyNode->FindPinChecked(TEXT("Field"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
            FOpenPocketBaseStringFieldRef::StaticStruct(),
            TitleRef);
    FilterNode->FindPinChecked(TEXT("Field"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
            FOpenPocketBaseBooleanFieldRef::StaticStruct(),
            DoneRef);

    UEdGraphPin* BodyFieldPin = BodyNode->FindPin(TEXT("Field"), EGPD_Input);
    TestNotNull(TEXT("Record body nodes expose a typed field picker"), BodyFieldPin);
    if (BodyFieldPin != nullptr)
    {
        TestEqual(
            TEXT("Record body field pickers exclude read-only fields"),
            BodyNode->GetPinMetaData(BodyFieldPin->PinName, TEXT("OpenPocketBaseFieldAccess")),
            FString(TEXT("Write")));
    }

    UEdGraphPin* FilterFieldPin = FilterNode->FindPin(TEXT("Field"), EGPD_Input);
    TestNotNull(TEXT("Filter nodes expose a typed field picker"), FilterFieldPin);
    if (FilterFieldPin != nullptr)
    {
        TestTrue(
            TEXT("Boolean filters only accept Boolean schema fields"),
            FilterFieldPin->PinType.PinSubCategoryObject.Get() ==
                FOpenPocketBaseBooleanFieldRef::StaticStruct());
    }

    UK2Node_CallFunction* FileUrlNode = NewObject<UK2Node_CallFunction>(Graph);
    FileUrlNode->SetFromFunction(UOpenPocketBaseFileLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFileLibrary, TryBuildFileUrl)));
    FileUrlNode->CreateNewGuid();
    FileUrlNode->PostPlacedNewNode();
    FileUrlNode->AllocateDefaultPins();
    Graph->AddNode(FileUrlNode, true, false);

    UK2Node_CallFunction* BatchNode = NewObject<UK2Node_CallFunction>(Graph);
    BatchNode->SetFromFunction(UOpenPocketBaseBatchLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseBatchLibrary, WithCreate)));
    BatchNode->CreateNewGuid();
    BatchNode->PostPlacedNewNode();
    BatchNode->AllocateDefaultPins();
    Graph->AddNode(BatchNode, true, false);

    UK2Node_CallFunction* LogoutNode = NewObject<UK2Node_CallFunction>(Graph);
    LogoutNode->SetFromFunction(UOpenPocketBaseClient::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, Logout)));
    LogoutNode->CreateNewGuid();
    LogoutNode->PostPlacedNewNode();
    LogoutNode->AllocateDefaultPins();
    Graph->AddNode(LogoutNode, true, false);

    TestNotNull(
        TEXT("Blueprint clients publish session changes"),
        UOpenPocketBaseClient::StaticClass()->FindPropertyByName(TEXT("SessionChanged")));

    int32 CollectionHandlePinCount = 0;
    int32 AuthCollectionHandlePinCount = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UEdGraphPin* RequiredCollectionPin =
                Node->FindPin(TEXT("Collection"), EGPD_Input))
        {
            ++CollectionHandlePinCount;
            if (RequiredCollectionPin->LinkedTo.IsEmpty())
            {
                TestTrue(
                    *FString::Printf(
                        TEXT("%s receives the typed collection handle"),
                        *Node->GetNodeTitle(ENodeTitleType::ListView).ToString()),
                    Schema->TryCreateConnection(
                        CollectionNode->FindPin(
                            UEdGraphSchema_K2::PN_ReturnValue,
                            EGPD_Output),
                        RequiredCollectionPin));
            }
        }
        if (UEdGraphPin* RequiredAuthCollectionPin =
                Node->FindPin(TEXT("AuthCollection"), EGPD_Input))
        {
            ++AuthCollectionHandlePinCount;
            TestTrue(
                *FString::Printf(
                    TEXT("%s receives the typed auth collection handle"),
                    *Node->GetNodeTitle(ENodeTitleType::ListView).ToString()),
                Schema->TryCreateConnection(
                    AuthCollectionNode->FindPin(
                        UEdGraphSchema_K2::PN_ReturnValue,
                        EGPD_Output),
                    RequiredAuthCollectionPin));
        }

        UK2Node_AsyncAction* AsyncNode = Cast<UK2Node_AsyncAction>(Node);
        if (AsyncNode == nullptr)
        {
            continue;
        }

        TestNotNull(
            *FString::Printf(
                TEXT("%s exposes the failure error"),
                *AsyncNode->GetNodeTitle(ENodeTitleType::ListView).ToString()),
            AsyncNode->FindPin(TEXT("Error"), EGPD_Output));
        TestNull(
            *FString::Printf(
                TEXT("%s resolves its Game Instance from the client"),
                *AsyncNode->GetNodeTitle(ENodeTitleType::ListView).ToString()),
            AsyncNode->FindPin(TEXT("WorldContextObject"), EGPD_Input));
    }
    TestTrue(
        TEXT("The consumer exercises required collection handles"),
        CollectionHandlePinCount > 0);
    TestTrue(
        TEXT("The consumer exercises required auth collection handles"),
        AuthCollectionHandlePinCount > 0);

    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    TestTrue(TEXT("The Blueprint consumer compiles without errors"), Blueprint->Status != BS_Error);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseConnectedBlueprintFlowTest,
    "OpenPocketBase.Blueprint.Consumer.CompilesConnectedSchemaFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseConnectedBlueprintFlowTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* PocketBaseSchema = NewObject<UOpenPocketBaseSchema>(
        GetTransientPackage(),
        TEXT("OpenPocketBaseConnectedFlowSchema"));
    PocketBaseSchema->SchemaId = FGuid(233, 377, 610, 987);
    FOpenPocketBaseSchemaCollection Tasks;
    Tasks.Id = TEXT("tasks_id");
    Tasks.Name = TEXT("sdk_tasks");
    Tasks.Type = EOpenPocketBaseCollectionType::Base;
    FOpenPocketBaseSchemaField Title;
    Title.Id = TEXT("title_id");
    Title.Name = TEXT("title");
    Title.Type = EOpenPocketBaseFieldType::Text;
    FOpenPocketBaseSchemaField Done;
    Done.Id = TEXT("done_id");
    Done.Name = TEXT("done");
    Done.Type = EOpenPocketBaseFieldType::Boolean;
    FOpenPocketBaseSchemaField Score;
    Score.Id = TEXT("score_id");
    Score.Name = TEXT("score");
    Score.Type = EOpenPocketBaseFieldType::Number;
    FOpenPocketBaseSchemaField Status;
    Status.Id = TEXT("status_id");
    Status.Name = TEXT("status");
    Status.Type = EOpenPocketBaseFieldType::Select;
    Status.Choices = {TEXT("todo"), TEXT("doing"), TEXT("done")};
    Tasks.Fields = {Title, Done, Score, Status};

    FOpenPocketBaseSchemaCollection Users;
    Users.Id = TEXT("users_id");
    Users.Name = TEXT("sdk_users");
    Users.Type = EOpenPocketBaseCollectionType::Auth;
    FOpenPocketBaseSchemaField Email;
    Email.Id = TEXT("email_id");
    Email.Name = TEXT("email");
    Email.Type = EOpenPocketBaseFieldType::Email;
    Users.Fields = {Email};
    PocketBaseSchema->Collections = {Tasks, Users};

    FOpenPocketBaseCollectionRef TasksRef;
    FOpenPocketBaseCollectionRef UsersRef;
    FOpenPocketBaseTextFieldRef TitleRef;
    FOpenPocketBaseTextFieldRef EmailRef;
    FOpenPocketBaseBooleanFieldRef DoneRef;
    FOpenPocketBaseAnyFieldRef TitleAnyRef;
    FOpenPocketBaseAnyFieldRef ScoreAnyRef;
    TestTrue(TEXT("The task collection resolves"), PocketBaseSchema->MakeCollectionRef(Tasks.Id, TasksRef));
    TestTrue(TEXT("The auth collection resolves"), PocketBaseSchema->MakeCollectionRef(Users.Id, UsersRef));
    TestTrue(TEXT("The title write field resolves"), PocketBaseSchema->MakeTypedFieldRef(TasksRef, Title.Id, TitleRef));
    TestTrue(TEXT("The email write field resolves"), PocketBaseSchema->MakeTypedFieldRef(UsersRef, Email.Id, EmailRef));
    TestTrue(TEXT("The done filter field resolves"), PocketBaseSchema->MakeTypedFieldRef(TasksRef, Done.Id, DoneRef));
    TestTrue(TEXT("The title projection field resolves"), PocketBaseSchema->MakeTypedFieldRef(TasksRef, Title.Id, TitleAnyRef));
    TestTrue(TEXT("The score sort field resolves"), PocketBaseSchema->MakeTypedFieldRef(TasksRef, Score.Id, ScoreAnyRef));

    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        UObject::StaticClass(),
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UBlueprint::StaticClass(),
            TEXT("BP_OpenPocketBaseConnectedFlow")),
        BPTYPE_Normal,
        NAME_None);
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        TEXT("PocketBaseFlow"),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, Graph, true, nullptr);
    const UEdGraphSchema_K2* GraphSchema = GetDefault<UEdGraphSchema_K2>();
    const auto Connect = [this, GraphSchema](
        const TCHAR* Description,
        UEdGraphPin* Output,
        UEdGraphPin* Input)
    {
        return TestTrue(Description, Output != nullptr && Input != nullptr &&
            GraphSchema->TryCreateConnection(Output, Input));
    };

    UK2Node_CallFunction* GetClient = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseClientLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClientLibrary, GetPocketBaseClient));
    UK2Node_CallFunction* UseCollection = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseClient::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, Collection));
    TestNotNull(TEXT("Get PocketBase Client is added"), GetClient);
    TestNotNull(TEXT("Use Collection is added"), UseCollection);
    if (GetClient == nullptr || UseCollection == nullptr)
    {
        return false;
    }
    UseCollection->FindPinChecked(TEXT("Reference"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportCollectionDefault(TasksRef);
    Connect(
        TEXT("The client flows into Use Collection"),
        GetClient->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        UseCollection->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input));

    UK2Node_CallFunction* NewBody = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseRecordLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, NewRecordBody));
    UK2Node_CallFunction* WithTitle = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseRecordLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithStringField));
    UK2Node_CallFunction* WithDone = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseRecordLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithBooleanField));
    TestNotNull(TEXT("New Record Body is added"), NewBody);
    TestNotNull(TEXT("With String Field is added"), WithTitle);
    TestNotNull(TEXT("With Boolean Field is added"), WithDone);
    if (NewBody == nullptr || WithTitle == nullptr || WithDone == nullptr)
    {
        return false;
    }
    WithTitle->FindPinChecked(TEXT("Field"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
            FOpenPocketBaseTextFieldRef::StaticStruct(),
            TitleRef);
    WithTitle->FindPinChecked(TEXT("Value"))->DefaultValue = TEXT("Ship the SDK");
    WithDone->FindPinChecked(TEXT("Field"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
            FOpenPocketBaseBooleanFieldRef::StaticStruct(),
            DoneRef);
    Connect(
        TEXT("The collection starts a typed record body"),
        UseCollection->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        NewBody->FindPin(TEXT("Collection"), EGPD_Input));
    Connect(
        TEXT("The new body flows into the title setter"),
        NewBody->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        WithTitle->FindPin(TEXT("Body"), EGPD_Input));
    Connect(
        TEXT("The title body flows into the done setter"),
        WithTitle->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        WithDone->FindPin(TEXT("Body"), EGPD_Input));

    UK2Node_AsyncAction* CreateRecord = AddAsyncConsumerNode(
        Graph,
        UOpenPocketBaseCreateRecordAsyncAction::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseCreateRecordAsyncAction, CreateRecord));
    TestNotNull(TEXT("Create Record is added"), CreateRecord);
    if (CreateRecord == nullptr)
    {
        return false;
    }
    Connect(
        TEXT("The same collection flows into Create Record"),
        UseCollection->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        CreateRecord->FindPin(TEXT("Collection"), EGPD_Input));
    Connect(
        TEXT("The completed body flows into Create Record"),
        WithDone->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        CreateRecord->FindPin(TEXT("Body"), EGPD_Input));

    UK2Node_CallFunction* Filter = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseFilterLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseFilterLibrary, BooleanFilter));
    UK2Node_CallFunction* NewOptions = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseRecordLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, NewListOptions));
    UK2Node_CallFunction* Where = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseRecordLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, ListOptionsWhere));
    UK2Node_CallFunction* Sort = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseQueryLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseQueryLibrary, SortDescending));
    UK2Node_CallFunction* ThenSort = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseRecordLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, ListOptionsThenSortBy));
    UK2Node_CallFunction* Select = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseQueryLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseQueryLibrary, SelectField));
    UK2Node_CallFunction* SelectOptions = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseRecordLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, ListOptionsSelectField));
    if (!TestNotNull(TEXT("Boolean Filter is added"), Filter) ||
        !TestNotNull(TEXT("List Options is added"), NewOptions) ||
        !TestNotNull(TEXT("Where is added"), Where) ||
        !TestNotNull(TEXT("Sort Descending is added"), Sort) ||
        !TestNotNull(TEXT("Then Sort By is added"), ThenSort) ||
        !TestNotNull(TEXT("Select Field is added"), Select) ||
        !TestNotNull(TEXT("Options Select Field is added"), SelectOptions))
    {
        return false;
    }
    Filter->FindPinChecked(TEXT("Field"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
            FOpenPocketBaseBooleanFieldRef::StaticStruct(),
            DoneRef);
    Sort->FindPinChecked(TEXT("Field"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
            FOpenPocketBaseAnyFieldRef::StaticStruct(),
            ScoreAnyRef);
    Select->FindPinChecked(TEXT("Field"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
            FOpenPocketBaseAnyFieldRef::StaticStruct(),
            TitleAnyRef);
    Connect(
        TEXT("List Options flows into Where"),
        NewOptions->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        Where->FindPin(TEXT("Options"), EGPD_Input));
    Connect(
        TEXT("The typed filter flows into Where"),
        Filter->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        Where->FindPin(TEXT("Filter"), EGPD_Input));
    Connect(
        TEXT("Filtered options flow into sorting"),
        Where->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        ThenSort->FindPin(TEXT("Options"), EGPD_Input));
    Connect(
        TEXT("The typed sort flows into options"),
        Sort->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        ThenSort->FindPin(TEXT("Sort"), EGPD_Input));
    Connect(
        TEXT("Sorted options flow into projection"),
        ThenSort->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        SelectOptions->FindPin(TEXT("Options"), EGPD_Input));
    Connect(
        TEXT("The typed projection flows into options"),
        Select->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        SelectOptions->FindPin(TEXT("Field"), EGPD_Input));

    UK2Node_AsyncAction* ListRecords = AddAsyncConsumerNode(
        Graph,
        UOpenPocketBaseListRecordsAsyncAction::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseListRecordsAsyncAction, ListRecords));
    TestNotNull(TEXT("List Records is added"), ListRecords);
    if (ListRecords == nullptr)
    {
        return false;
    }
    Connect(
        TEXT("The same collection flows into List Records"),
        UseCollection->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        ListRecords->FindPin(TEXT("Collection"), EGPD_Input));
    Connect(
        TEXT("The completed options flow into List Records"),
        SelectOptions->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        ListRecords->FindPin(TEXT("Options"), EGPD_Input));

    UK2Node_CallFunction* UseAuthCollection = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseClient::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseClient, Collection));
    UK2Node_CallFunction* NewUserBody = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseRecordLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, NewRecordBody));
    UK2Node_CallFunction* WithEmail = AddFunctionConsumerNode(
        Graph,
        UOpenPocketBaseRecordLibrary::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRecordLibrary, WithStringField));
    UK2Node_AsyncAction* RegisterUser = AddAsyncConsumerNode(
        Graph,
        UOpenPocketBaseRegisterUserAsyncAction::StaticClass(),
        GET_FUNCTION_NAME_CHECKED(UOpenPocketBaseRegisterUserAsyncAction, RegisterUser));
    if (!TestNotNull(TEXT("The auth collection node is added"), UseAuthCollection) ||
        !TestNotNull(TEXT("The user body node is added"), NewUserBody) ||
        !TestNotNull(TEXT("The email body node is added"), WithEmail) ||
        !TestNotNull(TEXT("Register User is added"), RegisterUser))
    {
        return false;
    }
    UseAuthCollection->FindPinChecked(TEXT("Reference"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportCollectionDefault(UsersRef);
    WithEmail->FindPinChecked(TEXT("Field"))->DefaultValue =
        FOpenPocketBaseSchemaPickerModel::ExportFieldDefault(
            FOpenPocketBaseTextFieldRef::StaticStruct(),
            EmailRef);
    WithEmail->FindPinChecked(TEXT("Value"))->DefaultValue = TEXT("player@example.com");
    RegisterUser->FindPinChecked(TEXT("Password"))->DefaultValue = TEXT("secret123");
    RegisterUser->FindPinChecked(TEXT("ConfirmPassword"))->DefaultValue = TEXT("secret123");
    Connect(
        TEXT("The client flows into the auth collection"),
        GetClient->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        UseAuthCollection->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input));
    Connect(
        TEXT("The auth collection starts the user body"),
        UseAuthCollection->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        NewUserBody->FindPin(TEXT("Collection"), EGPD_Input));
    Connect(
        TEXT("The user body flows into the email setter"),
        NewUserBody->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        WithEmail->FindPin(TEXT("Body"), EGPD_Input));
    Connect(
        TEXT("The same auth collection flows into Register User"),
        UseAuthCollection->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        RegisterUser->FindPin(TEXT("AuthCollection"), EGPD_Input));
    Connect(
        TEXT("The completed user body flows into Register User"),
        WithEmail->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output),
        RegisterUser->FindPin(TEXT("UserFields"), EGPD_Input));

    FKismetEditorUtilities::CompileBlueprint(
        Blueprint,
        EBlueprintCompileOptions::SkipGarbageCollection);
    TestTrue(TEXT("The connected schema-driven Blueprint flow compiles"), Blueprint->Status != BS_Error);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseBlueprintDiscoverabilityCoverageTest,
    "OpenPocketBase.Blueprint.Nodes.ExplainPublicWorkflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseBlueprintDiscoverabilityCoverageTest::RunTest(const FString& Parameters)
{
    const TSet<FString> ScriptPackages = {
        TEXT("/Script/OpenPocketBaseSDK"),
        TEXT("/Script/OpenPocketBaseSDKAdmin")};
    const TSet<FName> ImportantParameters = {
        TEXT("PocketBaseClient"),
        TEXT("Collection"),
        TEXT("Field"),
        TEXT("Body"),
        TEXT("Options"),
        TEXT("Record"),
        TEXT("RecordId"),
        TEXT("OutError")};
    int32 FunctionCount = 0;
    int32 ParameterCount = 0;
    for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
    {
        UClass* Class = *ClassIt;
        if (Class == nullptr || !Class->HasAnyClassFlags(CLASS_Native) ||
            !ScriptPackages.Contains(Class->GetOutermost()->GetName()))
        {
            continue;
        }

        for (TFieldIterator<UFunction> FunctionIt(
                 Class,
                 EFieldIteratorFlags::ExcludeSuper);
             FunctionIt;
             ++FunctionIt)
        {
            UFunction* Function = *FunctionIt;
            if (Function == nullptr ||
                !Function->HasAnyFunctionFlags(FUNC_BlueprintCallable) ||
                Function->HasMetaData(TEXT("DeprecatedFunction")))
            {
                continue;
            }

            ++FunctionCount;
            TestFalse(
                *FString::Printf(
                    TEXT("%s.%s has a useful tooltip"),
                    *Class->GetName(),
                    *Function->GetName()),
                Function->GetMetaData(TEXT("ToolTip")).TrimStartAndEnd().IsEmpty());
            TestFalse(
                *FString::Printf(
                    TEXT("%s.%s has search keywords"),
                    *Class->GetName(),
                    *Function->GetName()),
                Function->GetMetaData(TEXT("Keywords")).TrimStartAndEnd().IsEmpty());

            for (TFieldIterator<FProperty> PropertyIt(Function); PropertyIt; ++PropertyIt)
            {
                FProperty* Property = *PropertyIt;
                if (Property == nullptr || !Property->HasAnyPropertyFlags(CPF_Parm) ||
                    Property->HasAnyPropertyFlags(CPF_ReturnParm) ||
                    !ImportantParameters.Contains(Property->GetFName()))
                {
                    continue;
                }

                ++ParameterCount;
                TestFalse(
                    *FString::Printf(
                        TEXT("%s.%s parameter %s explains its role"),
                        *Class->GetName(),
                        *Function->GetName(),
                        *Property->GetName()),
                    Property->GetMetaData(TEXT("ToolTip")).TrimStartAndEnd().IsEmpty());
            }
        }
    }

    TestTrue(TEXT("The discoverability audit covers the public Blueprint API"), FunctionCount > 250);
    TestTrue(TEXT("The discoverability audit covers important workflow pins"), ParameterCount > 100);
    return true;
}

#endif
