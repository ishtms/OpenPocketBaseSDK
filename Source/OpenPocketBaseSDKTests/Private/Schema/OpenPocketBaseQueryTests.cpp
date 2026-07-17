#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
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
    FOpenPocketBaseAnyFieldRef ScoreRef;
    Schema->MakeTypedFieldRef(TasksRef, TEXT("score_id"), ScoreRef);
    const FOpenPocketBaseSort Sort = OpenPocketBase::Query::Sort(
        ScoreRef,
        EOpenPocketBaseSortDirection::Descending);
    TestTrue(TEXT("Typed sort values are valid"), Sort.IsSet());
    TestTrue(TEXT("Sort values retain collection ownership"), Sort.BelongsTo(TasksRef));
    TestEqual(TEXT("Descending sort syntax is generated"), Sort.ToQueryValue(), FString(TEXT("-score")));

    FOpenPocketBaseRelationFieldRef OwnerRef;
    Schema->MakeTypedFieldRef(TasksRef, TEXT("owner_id"), OwnerRef);
    FOpenPocketBaseCollectionRef UsersRef;
    Schema->MakeCollectionRef(TEXT("users_id"), UsersRef);
    FOpenPocketBaseRelationFieldRef ProfileRef;
    Schema->MakeTypedFieldRef(UsersRef, TEXT("profile_id"), ProfileRef);
    FOpenPocketBaseExpand Expand = OpenPocketBase::Query::Expand(OwnerRef);
    Expand = OpenPocketBase::Query::ThenExpand(MoveTemp(Expand), ProfileRef);
    TestTrue(TEXT("Related fields form a valid nested expand"), Expand.IsSet());
    TestTrue(TEXT("Expand values retain their root collection"), Expand.BelongsTo(TasksRef));
    TestEqual(TEXT("Nested expand syntax is generated"), Expand.ToQueryValue(), FString(TEXT("owner.profile")));

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
