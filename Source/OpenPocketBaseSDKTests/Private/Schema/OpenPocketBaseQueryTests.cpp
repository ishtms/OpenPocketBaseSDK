#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseFilter.h"
#include "OpenPocketBaseQuery.h"
#include "OpenPocketBaseSchema.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseTypedQueryTest,
    "OpenPocketBase.Schema.BuildsTypedQueries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseTypedQueryTest::RunTest(const FString& Parameters)
{
    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    Schema->SchemaId = FGuid(13, 21, 34, 55);

    FOpenPocketBaseSchemaCollection Tasks;
    Tasks.Id = TEXT("tasks_id");
    Tasks.Name = TEXT("sdk_tasks");
    FOpenPocketBaseSchemaField Score;
    Score.Id = TEXT("score_id");
    Score.Name = TEXT("score");
    Score.Type = EOpenPocketBaseFieldType::Number;
    Tasks.Fields.Add(Score);
    FOpenPocketBaseSchemaField Title;
    Title.Id = TEXT("title_id");
    Title.Name = TEXT("title");
    Title.Type = EOpenPocketBaseFieldType::Text;
    Tasks.Fields.Add(Title);
    FOpenPocketBaseSchemaField Owner;
    Owner.Id = TEXT("owner_id");
    Owner.Name = TEXT("owner");
    Owner.Type = EOpenPocketBaseFieldType::Relation;
    Owner.RelatedCollectionId = TEXT("users_id");
    Tasks.Fields.Add(Owner);

    FOpenPocketBaseSchemaCollection Users;
    Users.Id = TEXT("users_id");
    Users.Name = TEXT("sdk_users");
    Users.Type = EOpenPocketBaseCollectionType::Auth;
    FOpenPocketBaseSchemaField UserName;
    UserName.Id = TEXT("user_name_id");
    UserName.Name = TEXT("name");
    UserName.Type = EOpenPocketBaseFieldType::Text;
    Users.Fields.Add(UserName);
    FOpenPocketBaseSchemaField Profile;
    Profile.Id = TEXT("profile_id");
    Profile.Name = TEXT("profile");
    Profile.Type = EOpenPocketBaseFieldType::Relation;
    Profile.RelatedCollectionId = TEXT("profiles_id");
    Users.Fields.Add(Profile);

    FOpenPocketBaseSchemaCollection Other;
    Other.Id = TEXT("other_id");
    Other.Name = TEXT("other");
    FOpenPocketBaseSchemaField Wrong;
    Wrong.Id = TEXT("wrong_id");
    Wrong.Name = TEXT("wrong");
    Wrong.Type = EOpenPocketBaseFieldType::Relation;
    Wrong.RelatedCollectionId = TEXT("profiles_id");
    Other.Fields.Add(Wrong);
    Schema->Collections = {Tasks, Users, Other};

    FOpenPocketBaseCollectionRef TasksRef;
    Schema->MakeCollectionRef(TEXT("tasks_id"), TasksRef);
    FOpenPocketBaseNumberFieldRef ScoreRef;
    Schema->MakeTypedFieldRef(TasksRef, TEXT("score_id"), ScoreRef);
    const FOpenPocketBaseSort Sort = OpenPocketBase::Query::Sort(
        ScoreRef,
        EOpenPocketBaseSortDirection::Descending);
    TestTrue(TEXT("Typed sort values are valid"), Sort.IsSet());
    TestTrue(TEXT("Sort values retain collection ownership"), Sort.BelongsTo(TasksRef));
    TestEqual(TEXT("Descending sort syntax is generated"), Sort.ToQueryValue(), FString(TEXT("-score")));

    const FOpenPocketBaseFieldSelection ScoreSelection = OpenPocketBase::Query::Select(ScoreRef);
    TestTrue(TEXT("Selected fields retain collection ownership"), ScoreSelection.BelongsTo(TasksRef));
    TestEqual(TEXT("Selected fields generate PocketBase syntax"), ScoreSelection.ToQueryValue(), FString(TEXT("score")));

    FOpenPocketBaseStringFieldRef TitleRef;
    Schema->MakeTypedFieldRef(TasksRef, Title.Id, TitleRef);
    const FOpenPocketBaseFieldSelection TitleSelection = OpenPocketBase::Query::Select(TitleRef);
    TestEqual(
        TEXT("Specific field references can be selected directly"),
        TitleSelection.ToQueryValue(),
        FString(TEXT("title")));
    const FOpenPocketBaseFilter NullTitle = FOpenPocketBaseFilter::Null(TitleRef);
    TestEqual(
        TEXT("Specific field references can build null filters"),
        NullTitle.ToString(),
        FString(TEXT("title = null")));
    const FOpenPocketBaseFieldSelection Excerpt = OpenPocketBase::Query::SelectExcerpt(
        TitleRef,
        40,
        true);
    TestEqual(
        TEXT("Excerpt selections generate PocketBase syntax"),
        Excerpt.ToQueryValue(),
        FString(TEXT("title:excerpt(40,true)")));

    FOpenPocketBaseRelationFieldRef OwnerRef;
    Schema->MakeTypedFieldRef(TasksRef, TEXT("owner_id"), OwnerRef);
    FOpenPocketBaseCollectionRef UsersRef;
    Schema->MakeCollectionRef(TEXT("users_id"), UsersRef);
    FOpenPocketBaseStringFieldRef UserNameRef;
    Schema->MakeTypedFieldRef(UsersRef, UserName.Id, UserNameRef);
    UserNameRef.Type = EOpenPocketBaseFieldType::Unknown;
    const FOpenPocketBaseFilter RelatedName = FOpenPocketBaseFilter::RelatedString(
        OpenPocketBase::Query::Expand(OwnerRef),
        UserNameRef,
        EOpenPocketBaseStringComparison::Equals,
        TEXT("Ada"));
    TestTrue(TEXT("Related filters resolve serialized field references through the schema"), RelatedName.IsValid());
    TestEqual(
        TEXT("Related string filters use the resolved field"),
        RelatedName.ToString(),
        FString(TEXT("owner.name = \"Ada\"")));
    FOpenPocketBaseRelationFieldRef ProfileRef;
    Schema->MakeTypedFieldRef(UsersRef, TEXT("profile_id"), ProfileRef);
    FOpenPocketBaseExpand Expand = OpenPocketBase::Query::Expand(OwnerRef);
    Expand = OpenPocketBase::Query::ThenExpand(MoveTemp(Expand), ProfileRef);
    TestTrue(TEXT("Related fields form a valid nested expand"), Expand.IsSet());
    TestTrue(TEXT("Expand values retain their root collection"), Expand.BelongsTo(TasksRef));
    TestEqual(TEXT("Nested expand syntax is generated"), Expand.ToQueryValue(), FString(TEXT("owner.profile")));

    const FOpenPocketBaseFieldSelection ExpandedRecord =
        OpenPocketBase::Query::SelectExpandedRecord(OpenPocketBase::Query::Expand(OwnerRef));
    TestTrue(TEXT("Expanded record selections retain root ownership"), ExpandedRecord.BelongsTo(TasksRef));
    TestEqual(
        TEXT("Expanded record selections generate PocketBase syntax"),
        ExpandedRecord.ToQueryValue(),
        FString(TEXT("expand.owner.*")));

    FOpenPocketBaseCollectionRef OtherRef;
    Schema->MakeCollectionRef(TEXT("other_id"), OtherRef);
    FOpenPocketBaseRelationFieldRef WrongRef;
    Schema->MakeTypedFieldRef(OtherRef, TEXT("wrong_id"), WrongRef);
    const FOpenPocketBaseExpand Invalid = OpenPocketBase::Query::ThenExpand(
        OpenPocketBase::Query::Expand(OwnerRef),
        WrongRef);
    TestFalse(TEXT("Unrelated collections cannot form an expand path"), Invalid.bValid);
    TestFalse(TEXT("Invalid expand paths explain the mismatch"), Invalid.ErrorMessage.IsEmpty());
    return true;
}

#endif
