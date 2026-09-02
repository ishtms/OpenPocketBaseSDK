#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseJsonValueLibrary.h"
#include "OpenPocketBaseRecordLibrary.h"
#include "OpenPocketBaseSchema.h"

namespace
{
struct FRecordFieldFixture
{
    UOpenPocketBaseSchema* Schema = nullptr;
    FOpenPocketBaseCollectionRef Tasks;
    FOpenPocketBaseCollectionRef Other;
    FOpenPocketBaseTextFieldRef Title;
    FOpenPocketBaseStringFieldRef Id;
    FOpenPocketBaseDateFieldRef Created;
    FOpenPocketBaseStringFieldRef Description;
    FOpenPocketBaseStringFieldRef Missing;
    FOpenPocketBaseStringFieldRef WrongType;
    FOpenPocketBaseNumberFieldRef Priority;
    FOpenPocketBaseBooleanFieldRef Done;
    FOpenPocketBaseDateFieldRef Due;
    FOpenPocketBaseJsonFieldRef Settings;
    FOpenPocketBaseGeoPointFieldRef Location;
    FOpenPocketBaseSingleSelectFieldRef Status;
    FOpenPocketBaseMultipleSelectFieldRef Labels;
    FOpenPocketBaseSingleRelationFieldRef Owner;
    FOpenPocketBaseMultipleRelationFieldRef Reviewers;
    FOpenPocketBaseTextFieldRef OtherTitle;

    FRecordFieldFixture()
    {
        Schema = NewObject<UOpenPocketBaseSchema>();
        Schema->SchemaId = FGuid(21, 34, 55, 89);

        FOpenPocketBaseSchemaCollection TasksCollection;
        TasksCollection.Id = TEXT("tasks_id");
        TasksCollection.Name = TEXT("sdk_tasks");
        TasksCollection.Type = EOpenPocketBaseCollectionType::Base;
        TasksCollection.Fields = {
            MakeField(TEXT("id"), TEXT("id"), EOpenPocketBaseFieldType::Text, false, EOpenPocketBaseFieldStorage::RecordId),
            MakeField(TEXT("created"), TEXT("created"), EOpenPocketBaseFieldType::Autodate, false, EOpenPocketBaseFieldStorage::Created),
            MakeField(TEXT("title_id"), TEXT("title"), EOpenPocketBaseFieldType::Text),
            MakeField(TEXT("description_id"), TEXT("description"), EOpenPocketBaseFieldType::Text),
            MakeField(TEXT("missing_id"), TEXT("missing"), EOpenPocketBaseFieldType::Text),
            MakeField(TEXT("wrong_type_id"), TEXT("wrongType"), EOpenPocketBaseFieldType::Text),
            MakeField(TEXT("priority_id"), TEXT("priority"), EOpenPocketBaseFieldType::Number),
            MakeField(TEXT("done_id"), TEXT("done"), EOpenPocketBaseFieldType::Boolean),
            MakeField(TEXT("due_id"), TEXT("due"), EOpenPocketBaseFieldType::Date),
            MakeField(TEXT("settings_id"), TEXT("settings"), EOpenPocketBaseFieldType::Json),
            MakeField(TEXT("location_id"), TEXT("location"), EOpenPocketBaseFieldType::GeoPoint),
            MakeField(TEXT("status_id"), TEXT("status"), EOpenPocketBaseFieldType::Select),
            MakeField(TEXT("labels_id"), TEXT("labels"), EOpenPocketBaseFieldType::Select, true),
            MakeField(TEXT("owner_id"), TEXT("owner"), EOpenPocketBaseFieldType::Relation),
            MakeField(TEXT("reviewers_id"), TEXT("reviewers"), EOpenPocketBaseFieldType::Relation, true)};
        TasksCollection.Fields[11].Choices = {TEXT("active"), TEXT("paused")};
        TasksCollection.Fields[12].Choices = {TEXT("sdk"), TEXT("unreal")};
        TasksCollection.Fields[12].MinSelect = 1;
        TasksCollection.Fields[12].MaxSelect = 2;
        TasksCollection.Fields[13].RelatedCollectionId = TEXT("users_id");
        TasksCollection.Fields[14].RelatedCollectionId = TEXT("users_id");
        TasksCollection.Fields[14].MaxSelect = 3;

        FOpenPocketBaseSchemaCollection OtherCollection;
        OtherCollection.Id = TEXT("other_id");
        OtherCollection.Name = TEXT("other");
        OtherCollection.Type = EOpenPocketBaseCollectionType::Base;
        OtherCollection.Fields = {
            MakeField(TEXT("other_title_id"), TEXT("title"), EOpenPocketBaseFieldType::Text)};

        Schema->Collections = {TasksCollection, OtherCollection};
        Schema->MakeCollectionRef(TasksCollection.Id, Tasks);
        Schema->MakeCollectionRef(OtherCollection.Id, Other);
        Schema->MakeTypedFieldRef(Tasks, TEXT("id"), Id);
        Schema->MakeTypedFieldRef(Tasks, TEXT("created"), Created);
        Schema->MakeTypedFieldRef(Tasks, TEXT("title_id"), Title);
        Schema->MakeTypedFieldRef(Tasks, TEXT("description_id"), Description);
        Schema->MakeTypedFieldRef(Tasks, TEXT("missing_id"), Missing);
        Schema->MakeTypedFieldRef(Tasks, TEXT("wrong_type_id"), WrongType);
        Schema->MakeTypedFieldRef(Tasks, TEXT("priority_id"), Priority);
        Schema->MakeTypedFieldRef(Tasks, TEXT("done_id"), Done);
        Schema->MakeTypedFieldRef(Tasks, TEXT("due_id"), Due);
        Schema->MakeTypedFieldRef(Tasks, TEXT("settings_id"), Settings);
        Schema->MakeTypedFieldRef(Tasks, TEXT("location_id"), Location);
        Schema->MakeTypedFieldRef(Tasks, TEXT("status_id"), Status);
        Schema->MakeTypedFieldRef(Tasks, TEXT("labels_id"), Labels);
        Schema->MakeTypedFieldRef(Tasks, TEXT("owner_id"), Owner);
        Schema->MakeTypedFieldRef(Tasks, TEXT("reviewers_id"), Reviewers);
        Schema->MakeTypedFieldRef(Other, TEXT("other_title_id"), OtherTitle);
    }

private:
    static FOpenPocketBaseSchemaField MakeField(
        const FString& Id,
        const FString& Name,
        EOpenPocketBaseFieldType Type,
        const bool bMultiple = false,
        const EOpenPocketBaseFieldStorage Storage = EOpenPocketBaseFieldStorage::Data)
    {
        FOpenPocketBaseSchemaField Field;
        Field.Id = Id;
        Field.Name = Name;
        Field.Type = Type;
        Field.bMultiple = bMultiple;
        Field.Storage = Storage;
        return Field;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordPageBreakNodeTest,
    "OpenPocketBase.Blueprint.Records.RecordPageBreakShowsEveryOutput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordPageBreakNodeTest::RunTest(const FString& Parameters)
{
    TestEqual(
        TEXT("Record pages use the SDK break function"),
        FOpenPocketBaseRecordPage::StaticStruct()->GetMetaData(TEXT("HasNativeBreak")),
        FString(TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseRecordLibrary.BreakRecordPage")));

    const UFunction* BreakRecordPage = UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(
        TEXT("BreakRecordPage"));
    if (!TestNotNull(TEXT("The record-page break function exists"), BreakRecordPage))
    {
        return false;
    }
    TestTrue(
        TEXT("The record-page function is a native Break Struct node"),
        BreakRecordPage->HasMetaData(TEXT("NativeBreakFunc")));
    TestFalse(
        TEXT("Every record-page output stays visible"),
        BreakRecordPage->HasMetaData(TEXT("AdvancedDisplay")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseOptionsBlueprintSurfaceTest,
    "OpenPocketBase.Blueprint.Records.OptionsUseGuidedBuilders",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseOptionsBlueprintSurfaceTest::RunTest(const FString& Parameters)
{
    TestEqual(
        TEXT("Record options use the guided builder"),
        FOpenPocketBaseRecordOptions::StaticStruct()->GetMetaData(TEXT("HasNativeMake")),
        FString(TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseRecordLibrary.NewRecordOptions")));
    TestEqual(
        TEXT("List options use the guided builder"),
        FOpenPocketBaseListOptions::StaticStruct()->GetMetaData(TEXT("HasNativeMake")),
        FString(TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseRecordLibrary.NewListOptions")));
    TestEqual(
        TEXT("Full-list options use the dedicated builder"),
        FOpenPocketBaseFullListOptions::StaticStruct()->GetMetaData(TEXT("HasNativeMake")),
        FString(TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseRecordLibrary.NewFullListOptions")));

    const FOpenPocketBaseFullListOptions DefaultStructOptions;
    const FOpenPocketBaseFullListOptions DefaultBlueprintOptions =
        UOpenPocketBaseRecordLibrary::NewFullListOptions();
    TestEqual(TEXT("Default C++ full-list options have a safe item bound"), DefaultStructOptions.MaxItems, 1000);
    TestEqual(TEXT("Default Blueprint full-list options have a safe item bound"), DefaultBlueprintOptions.MaxItems, 1000);
    TestEqual(TEXT("Default full-list options leave the optional page bound disabled"), DefaultBlueprintOptions.MaxPages, 0);

    const FRecordFieldFixture Fields;
    const FOpenPocketBaseFilter Filter = FOpenPocketBaseFilter::Boolean(
        Fields.Done,
        EOpenPocketBaseBooleanComparison::Equals,
        true);
    const FOpenPocketBaseFullListOptions Base =
        UOpenPocketBaseRecordLibrary::NewFullListOptions(5, 12, 10);
    const FOpenPocketBaseFullListOptions Filtered =
        UOpenPocketBaseRecordLibrary::FullListOptionsWhere(Base, Filter);

    TestEqual(TEXT("Full-list traversal always starts at page one"), Filtered.ListOptions.Page, 1);
    TestEqual(TEXT("Full-list page size is explicit"), Filtered.ListOptions.PerPage, 5);
    TestEqual(TEXT("Full-list item limit is explicit"), Filtered.MaxItems, 12);
    TestEqual(TEXT("Full-list page limit is explicit"), Filtered.MaxPages, 10);
    TestEqual(
        TEXT("Full-list filters use the same typed filter value"),
        Filtered.ListOptions.Filter.ToString(),
        FString(TEXT("done = true")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordLibraryTest,
    "OpenPocketBase.Blueprint.Records.FieldStatesRemainDistinct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordLibraryTest::RunTest(const FString& Parameters)
{
    const FRecordFieldFixture Fields;
    FOpenPocketBaseRecord Record;
    Record.CollectionId = Fields.Tasks.CollectionId;
    Record.Data.JsonObject = MakeShared<FJsonObject>();
    Record.Data.JsonObject->SetStringField(TEXT("title"), TEXT("Ship SDK"));
    Record.Data.JsonObject->SetField(TEXT("description"), MakeShared<FJsonValueNull>());
    Record.Data.JsonObject->SetNumberField(TEXT("wrongType"), 3);
    Record.Data.JsonObject->SetNumberField(TEXT("priority"), 3);

    FString TextValue;
    TestEqual(
        TEXT("A string field is found"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, Fields.Title, TextValue),
        EOpenPocketBaseFieldState::Found);
    TestEqual(TEXT("The string value is preserved"), TextValue, FString(TEXT("Ship SDK")));

    TestEqual(
        TEXT("An absent field remains distinguishable"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, Fields.Missing, TextValue),
        EOpenPocketBaseFieldState::Missing);
    TestEqual(
        TEXT("A field from another collection is rejected"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, Fields.OtherTitle, TextValue),
        EOpenPocketBaseFieldState::WrongCollection);
    TestEqual(
        TEXT("A null field remains distinguishable"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, Fields.Description, TextValue),
        EOpenPocketBaseFieldState::Null);
    TestEqual(
        TEXT("A wrong type remains distinguishable"),
        UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, Fields.WrongType, TextValue),
        EOpenPocketBaseFieldState::WrongType);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordValueBuilderTest,
    "OpenPocketBase.Blueprint.Records.BuildsImmutableRequestValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordValueBuilderTest::RunTest(const FString& Parameters)
{
    const FRecordFieldFixture Fields;
    FOpenPocketBaseCollection Collection;
    Collection.Reference = Fields.Tasks;
    const FOpenPocketBaseRecordBody Empty = UOpenPocketBaseRecordLibrary::NewRecordBody(Collection);
    const FOpenPocketBaseRecordBody WithTitle = UOpenPocketBaseRecordLibrary::WithStringField(
        Empty,
        Fields.Title,
        TEXT("Ship SDK"));
    const FOpenPocketBaseRecordBody WithDone = UOpenPocketBaseRecordLibrary::WithBooleanField(
        WithTitle,
        Fields.Done,
        false);

    TestTrue(TEXT("The text field reference is valid"), Fields.Title.IsSet());
    TestTrue(TEXT("Adding a text field keeps the body valid"), WithTitle.IsValid());
    TestEqual(
        TEXT("The text field is added before chaining"),
        WithTitle.Data.JsonObject->GetStringField(TEXT("title")),
        FString(TEXT("Ship SDK")));
    TestFalse(TEXT("The original body remains unchanged"), Empty.Data.JsonObject->HasField(TEXT("title")));
    TestEqual(
        TEXT("Pure body nodes preserve earlier fields"),
        WithDone.Data.JsonObject->GetStringField(TEXT("title")),
        FString(TEXT("Ship SDK")));
    TestFalse(
        TEXT("Pure body nodes add the new field"),
        WithDone.Data.JsonObject->GetBoolField(TEXT("done")));

    const UFunction* RemoveFilesFunction =
        UOpenPocketBaseRecordLibrary::StaticClass()->FindFunctionByName(TEXT("WithFilesRemoved"));
    TestNotNull(TEXT("Blueprint exposes typed file removal by existing file name"), RemoveFilesFunction);
    const FOpenPocketBaseRecordBody WithRemovedFiles =
        UOpenPocketBaseRecordLibrary::WithDynamicFilesRemoved(
            WithDone,
            TEXT("attachments"),
            {TEXT("old-a.txt"), TEXT("old-b.txt")});
    TestTrue(TEXT("Dynamic file removal keeps the body valid"), WithRemovedFiles.IsValid());
    TestEqual(
        TEXT("Blueprint file removal uses the PocketBase field-minus key"),
        WithRemovedFiles.Data.JsonObject->GetArrayField(TEXT("attachments-")).Num(),
        2);
    TestFalse(
        TEXT("Pure file-removal nodes do not mutate the source body"),
        WithDone.Data.JsonObject->HasField(TEXT("attachments-")));

    FOpenPocketBaseRecordBody NativeBody;
    NativeBody
        .SetStringField(Fields.Title, TEXT("Native"))
        .SetNumberField(Fields.Priority, 5.0)
        .SetBooleanField(Fields.Done, true);
    TestTrue(
        TEXT("Native body setters chain"),
        NativeBody.Data.JsonObject->GetBoolField(TEXT("done")));

    FOpenPocketBaseRecordBody NullBody;
    NullBody.SetNullField(Fields.Title);
    TestTrue(
        TEXT("Specific field references can be set to null"),
        NullBody.Data.JsonObject->HasTypedField<EJson::Null>(TEXT("title")));

    const FOpenPocketBaseRecordBody FirstNullCopy = NullBody;
    const FOpenPocketBaseRecordBody SecondNullCopy = FirstNullCopy;
    TestTrue(
        TEXT("Repeated body copies preserve JSON null fields"),
        SecondNullCopy.Data.JsonObject->HasTypedField<EJson::Null>(TEXT("title")));

    FOpenPocketBaseListOptions Options;
    Options
        .AtPage(2)
        .PageSize(50)
        .Where(FOpenPocketBaseFilter::Boolean(
            Fields.Done,
            EOpenPocketBaseBooleanComparison::Equals,
            false))
        .SkipTotals();
    TestEqual(TEXT("Native list options chain the page"), Options.Page, 2);
    TestEqual(TEXT("Native list options chain the page size"), Options.PerPage, 50);
    TestEqual(
        TEXT("Native list options chain a typed filter"),
        Options.Filter.ToString(),
        FString(TEXT("done = false")));
    TestTrue(TEXT("Native list options chain total skipping"), Options.bSkipTotal);

    const FOpenPocketBaseRecordBody InvalidBody = UOpenPocketBaseRecordLibrary::WithStringField(
        WithDone,
        Fields.OtherTitle,
        TEXT("Wrong collection"));
    TestFalse(TEXT("A record body cannot mix collections"), InvalidBody.IsValid());
    TestFalse(TEXT("Body collection mismatches explain the error"), InvalidBody.ErrorMessage.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseCompleteRecordBodyTest,
    "OpenPocketBase.Blueprint.Records.BuildsEverySchemaValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseCompleteRecordBodyTest::RunTest(const FString& Parameters)
{
    const FRecordFieldFixture Fields;
    const FOpenPocketBaseJsonValue Settings =
        UOpenPocketBaseJsonValueLibrary::SetJsonProperty(
            UOpenPocketBaseJsonValueLibrary::MakeJsonObject(),
            TEXT("notifications"),
            UOpenPocketBaseJsonValueLibrary::JsonBoolean(true));
    TestTrue(TEXT("JSON object builders create a valid value"), Settings.IsValid());

    const FOpenPocketBaseJsonValue ObjectWithNull =
        UOpenPocketBaseJsonValueLibrary::SetJsonProperty(
            UOpenPocketBaseJsonValueLibrary::MakeJsonObject(),
            TEXT("optional"),
            UOpenPocketBaseJsonValueLibrary::JsonNull());
    const FOpenPocketBaseJsonValue ObjectExtendedAfterNull =
        UOpenPocketBaseJsonValueLibrary::SetJsonProperty(
            ObjectWithNull,
            TEXT("name"),
            UOpenPocketBaseJsonValueLibrary::JsonString(TEXT("PocketBase")));
    TestTrue(
        TEXT("JSON objects remain valid when a property is added after an explicit null"),
        ObjectExtendedAfterNull.IsValid());
    const TSharedPtr<FJsonValue> ExtendedValue = ObjectExtendedAfterNull.ToJsonValue();
    const TSharedPtr<FJsonObject>* ExtendedObject = nullptr;
    TestTrue(
        TEXT("JSON objects retain explicit null properties after another property is added"),
        ExtendedValue.IsValid() && ExtendedValue->TryGetObject(ExtendedObject) &&
            ExtendedObject != nullptr && ExtendedObject->IsValid() &&
            (*ExtendedObject)->HasTypedField<EJson::Null>(TEXT("optional")));

    TSharedRef<FJsonObject> MalformedObject = MakeShared<FJsonObject>();
    MalformedObject->SetField(TEXT("invalid"), nullptr);
    TestFalse(
        TEXT("Malformed nested JSON values are rejected without entering the serializer"),
        FOpenPocketBaseJsonValue::FromJsonValue(
            MakeShared<FJsonValueObject>(MalformedObject)).IsValid());

    FOpenPocketBaseRecordBody Body;
    Body
        .SetDateField(Fields.Due, FDateTime(2026, 8, 24, 12, 30))
        .SetJsonField(Fields.Settings, Settings)
        .SetGeoPointField(Fields.Location, {30.7333, 76.7794})
        .SetSingleSelectField(Fields.Status, TEXT("active"))
        .SetMultipleSelectField(Fields.Labels, {TEXT("sdk"), TEXT("unreal")})
        .SetSingleRelationField(Fields.Owner, TEXT("owner000000001"))
        .SetMultipleRelationField(
            Fields.Reviewers,
            {TEXT("user0000000001"), TEXT("user0000000002")})
        .SetDynamicStringField(TEXT("password"), TEXT("secret123"))
        .SetDynamicStringField(TEXT("passwordConfirm"), TEXT("secret123"));

    TestTrue(TEXT("Every semantic body value is accepted"), Body.IsValid());
    TestEqual(TEXT("Single select uses a string"), Body.Data.JsonObject->GetStringField(TEXT("status")), FString(TEXT("active")));
    TestEqual(TEXT("Single relation uses a record ID"), Body.Data.JsonObject->GetStringField(TEXT("owner")), FString(TEXT("owner000000001")));
    TestEqual(TEXT("Date values use the PocketBase format"), Body.Data.JsonObject->GetStringField(TEXT("due")), FString(TEXT("2026-08-24 12:30:00.000Z")));
    TestEqual(TEXT("Geo latitude is preserved"), Body.Data.JsonObject->GetObjectField(TEXT("location"))->GetNumberField(TEXT("lat")), 30.7333);
    TestEqual(TEXT("Multiple selects use an array"), Body.Data.JsonObject->GetArrayField(TEXT("labels")).Num(), 2);
    TestEqual(TEXT("Multiple relations use an array"), Body.Data.JsonObject->GetArrayField(TEXT("reviewers")).Num(), 2);
    TestTrue(TEXT("JSON objects are preserved"), Body.Data.JsonObject->GetObjectField(TEXT("settings"))->GetBoolField(TEXT("notifications")));
    FOpenPocketBaseJsonValue Notifications;
    TestTrue(
        TEXT("JSON object properties can be read without serialization"),
        UOpenPocketBaseJsonValueLibrary::TryGetJsonProperty(
            Settings,
            TEXT("notifications"),
            Notifications));
    bool bNotifications = false;
    TestTrue(
        TEXT("JSON Boolean properties retain their type"),
        UOpenPocketBaseJsonValueLibrary::TryGetJsonBoolean(
            Notifications,
            bNotifications));
    TestTrue(TEXT("JSON Boolean properties retain their value"), bNotifications);
    TestEqual(TEXT("Registration confirmation is available through the dynamic escape hatch"), Body.Data.JsonObject->GetStringField(TEXT("passwordConfirm")), FString(TEXT("secret123")));

    const FOpenPocketBaseJsonValue JsonArray = UOpenPocketBaseJsonValueLibrary::AddJsonItem(
        UOpenPocketBaseJsonValueLibrary::AddJsonItem(
            UOpenPocketBaseJsonValueLibrary::MakeJsonArray(),
            UOpenPocketBaseJsonValueLibrary::JsonString(TEXT("unreal"))),
        UOpenPocketBaseJsonValueLibrary::JsonNumber(5.0));
    TestTrue(TEXT("JSON array builders create a valid value"), JsonArray.IsValid());
    FOpenPocketBaseRecordBody ArrayBody;
    ArrayBody.SetJsonField(Fields.Settings, JsonArray);
    TestTrue(TEXT("JSON arrays are accepted as field roots"), ArrayBody.IsValid());
    TestEqual(TEXT("JSON arrays preserve every item"), ArrayBody.Data.JsonObject->GetArrayField(TEXT("settings")).Num(), 2);
    int32 ArrayLength = 0;
    TestTrue(
        TEXT("JSON array length is available"),
        UOpenPocketBaseJsonValueLibrary::TryGetJsonArrayLength(JsonArray, ArrayLength));
    TestEqual(TEXT("JSON array length is correct"), ArrayLength, 2);

    const FOpenPocketBaseJsonValue JsonScalar =
        UOpenPocketBaseJsonValueLibrary::JsonString(TEXT("scalar"));
    TestTrue(TEXT("JSON scalar builders create a valid value"), JsonScalar.IsValid());
    FOpenPocketBaseRecordBody ScalarBody;
    ScalarBody.SetJsonField(Fields.Settings, JsonScalar);
    TestTrue(TEXT("JSON scalars are accepted as field roots"), ScalarBody.IsValid());
    TestEqual(TEXT("JSON scalar roots are preserved"), ScalarBody.Data.JsonObject->GetStringField(TEXT("settings")), FString(TEXT("scalar")));

    FOpenPocketBaseRecord ScalarRecord;
    ScalarRecord.CollectionId = Fields.Tasks.CollectionId;
    ScalarRecord.Data = ScalarBody.Data;
    FOpenPocketBaseJsonValue ReadScalar;
    TestTrue(
        TEXT("JSON record reads preserve scalar roots"),
        UOpenPocketBaseRecordLibrary::TryGetJsonField(
            ScalarRecord,
            Fields.Settings,
            ReadScalar));
    FString ReadString;
    TestTrue(
        TEXT("JSON scalar readers expose the original value"),
        UOpenPocketBaseJsonValueLibrary::TryGetJsonString(ReadScalar, ReadString));
    TestEqual(TEXT("JSON scalar reader output is unchanged"), ReadString, FString(TEXT("scalar")));

    FOpenPocketBaseJsonValue ParsedBoolean;
    TestTrue(
        TEXT("JSON parsing accepts scalar roots"),
        UOpenPocketBaseJsonValueLibrary::ParseJson(TEXT("true"), ParsedBoolean));
    bool bParsedBoolean = false;
    TestTrue(
        TEXT("Parsed scalar roots retain their type"),
        UOpenPocketBaseJsonValueLibrary::TryGetJsonBoolean(
            ParsedBoolean,
            bParsedBoolean));
    TestTrue(TEXT("Parsed scalar roots retain their value"), bParsedBoolean);

    const FOpenPocketBaseJsonValue JsonNull = UOpenPocketBaseJsonValueLibrary::JsonNull();
    TestTrue(TEXT("JSON null builders create a valid value"), JsonNull.IsValid());
    FOpenPocketBaseRecordBody JsonNullBody;
    JsonNullBody.SetJsonField(Fields.Settings, JsonNull);
    TestTrue(TEXT("JSON null is accepted as a field root"), JsonNullBody.IsValid());
    TestTrue(
        TEXT("JSON null roots are preserved"),
        JsonNullBody.Data.JsonObject->HasTypedField<EJson::Null>(TEXT("settings")));

    FOpenPocketBaseRecord Owner;
    Owner.Id = TEXT("user00000000001");
    Owner.CollectionId = TEXT("users_id");
    FOpenPocketBaseRecord Reviewer;
    Reviewer.Id = TEXT("user00000000002");
    Reviewer.CollectionId = TEXT("users_id");
    FOpenPocketBaseRecordBody RecordRelations;
    RecordRelations
        .SetSingleRelationRecord(Fields.Owner, Owner)
        .SetMultipleRelationRecords(Fields.Reviewers, {Owner, Reviewer});
    TestTrue(TEXT("Relation records are accepted without copying IDs"), RecordRelations.IsValid());
    TestEqual(
        TEXT("Single relation records use their IDs"),
        RecordRelations.Data.JsonObject->GetStringField(TEXT("owner")),
        Owner.Id);
    TestEqual(
        TEXT("Multiple relation records use their IDs"),
        RecordRelations.Data.JsonObject->GetArrayField(TEXT("reviewers")).Num(),
        2);

    FOpenPocketBaseRecord WrongOwner = Owner;
    WrongOwner.CollectionId = TEXT("teams_id");
    FOpenPocketBaseRecordBody WrongRelation;
    WrongRelation.SetSingleRelationRecord(Fields.Owner, WrongOwner);
    TestFalse(TEXT("Relation records from another collection are rejected"), WrongRelation.IsValid());
    TestTrue(TEXT("Relation errors identify the record"), WrongRelation.ErrorMessage.Contains(Owner.Id));

    FOpenPocketBaseRecordBody InvalidSelect;
    InvalidSelect.SetSingleSelectField(Fields.Status, TEXT("retired"));
    TestFalse(TEXT("Unknown single-select choices are rejected locally"), InvalidSelect.IsValid());
    TestTrue(
        TEXT("Invalid select errors identify the rejected choice"),
        InvalidSelect.ErrorMessage.Contains(TEXT("retired")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordMetadataAndExpansionTest,
    "OpenPocketBase.Blueprint.Records.ReadsMetadataAndExpansions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordMetadataAndExpansionTest::RunTest(const FString& Parameters)
{
    const FRecordFieldFixture Fields;
    FOpenPocketBaseRecord Record;
    Record.Id = TEXT("task000000001");
    Record.CollectionId = Fields.Tasks.CollectionId;
    Record.CollectionName = TEXT("sdk_tasks");
    Record.Created = FDateTime(2026, 8, 24, 10, 0);

    TSharedRef<FJsonObject> Owner = MakeShared<FJsonObject>();
    Owner->SetStringField(TEXT("id"), TEXT("user000000001"));
    Owner->SetStringField(TEXT("collectionId"), TEXT("other_id"));
    Owner->SetStringField(TEXT("collectionName"), TEXT("other"));
    Owner->SetStringField(TEXT("title"), TEXT("Owner"));
    Record.Expanded.JsonObject = MakeShared<FJsonObject>();
    Record.Expanded.JsonObject->SetObjectField(TEXT("owner"), Owner);

    FString Id;
    FDateTime Created;
    TestTrue(TEXT("Record IDs are read through schema fields"), UOpenPocketBaseRecordLibrary::TryGetStringField(Record, Fields.Id, Id));
    TestEqual(TEXT("The typed ID matches record metadata"), Id, Record.Id);
    TestTrue(TEXT("Created timestamps are read through schema fields"), UOpenPocketBaseRecordLibrary::TryGetDateField(Record, Fields.Created, Created));
    TestEqual(TEXT("The typed date matches record metadata"), Created, Record.Created);

    FOpenPocketBaseRecord ExpandedOwner;
    TestEqual(
        TEXT("A single expanded relation is found"),
        UOpenPocketBaseRecordLibrary::GetExpandedRecordState(Record, Fields.Owner, ExpandedOwner),
        EOpenPocketBaseFieldState::Found);
    TestEqual(TEXT("The expanded record is parsed"), ExpandedOwner.Id, FString(TEXT("user000000001")));

    FOpenPocketBaseRelationFieldRef OwnerRelation;
    static_cast<FOpenPocketBaseFieldRef&>(OwnerRelation) = Fields.Owner;
    TArray<FOpenPocketBaseRecord> PathRecords;
    TestEqual(
        TEXT("Expansion paths can be consumed directly"),
        UOpenPocketBaseRecordLibrary::FollowExpansionPath(
            Record,
            OpenPocketBase::Query::Expand(OwnerRelation),
            PathRecords),
        EOpenPocketBaseFieldState::Found);
    TestEqual(TEXT("The expansion path returns one record"), PathRecords.Num(), 1);
    return true;
}

#endif
