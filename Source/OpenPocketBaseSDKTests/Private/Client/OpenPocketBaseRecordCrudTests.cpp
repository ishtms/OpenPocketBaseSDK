#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseTestClientFactory.h"
#include "OpenPocketBaseRecordLibrary.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace
{
TArray<uint8> ToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

FString FromUtf8(const TArray<uint8>& Value)
{
    if (Value.IsEmpty())
    {
        return {};
    }

    const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Value.GetData()), Value.Num());
    return FString(Converted.Length(), Converted.Get());
}

class FCrudTransport final : public IOpenPocketBaseTransport
{
public:
    TArray<FOpenPocketBaseHttpRequest> Requests;
    TArray<FOpenPocketBaseHttpResponse> Responses;

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        const bool bIsDelete = Request.Method == TEXT("DELETE");
        const bool bIsFirst = Request.Method == TEXT("GET") && Request.Url.Contains(TEXT("perPage=1"));
        Requests.Add(Request);

        FOpenPocketBaseHttpResponse Response;
        if (!Responses.IsEmpty())
        {
            Response = MoveTemp(Responses[0]);
            Responses.RemoveAt(0, EAllowShrinking::No);
            Response.RequestId = Request.RequestId;
            Response.EffectiveUrl = Request.Url;
            OnComplete(MoveTemp(Response));
            return {};
        }
        Response.bTransportSucceeded = true;
        Response.HttpStatus = bIsDelete ? 204 : 200;
        Response.RequestId = Request.RequestId;
        Response.EffectiveUrl = Request.Url;
        if (bIsFirst)
        {
            Response.Body = ToUtf8(
                TEXT("{\"page\":1,\"perPage\":1,\"totalItems\":-1,\"totalPages\":-1,\"items\":[{")
                TEXT("\"id\":\"first123\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\",\"title\":\"First\"}]}"));
        }
        else if (!bIsDelete)
        {
            Response.Body = ToUtf8(
                TEXT("{\"id\":\"task123\",\"collectionId\":\"tasks_id\",\"collectionName\":\"tasks\",")
                TEXT("\"created\":\"2026-08-23 16:00:56.310Z\",\"updated\":\"2026-08-23 16:00:56.310Z\",")
                TEXT("\"title\":\"Saved\",\"futureField\":{\"kept\":true},\"expand\":{")
                TEXT("\"owner\":{\"id\":\"user123\",\"expand\":{\"team\":{\"id\":\"team123\"}}},")
                TEXT("\"tasks_via_parent\":[{\"id\":\"child123\"}]}}"));
        }
        OnComplete(MoveTemp(Response));
        return {};
    }
};

struct FCrudTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FCrudTransport, ESPMode::ThreadSafe> Transport;
    int32 CompletionCount = 0;
    bool bCreateSucceeded = false;
    bool bUpdateSucceeded = false;
    bool bDeleteSucceeded = false;
    bool bFirstSucceeded = false;
    bool bUnknownFieldRetained = false;
    bool bRecordDataClean = false;
    bool bNestedExpansionRetained = false;
    bool bBackRelationRetained = false;
    FString FirstId;
};

class FVerifyCrudContract final : public IAutomationLatentCommand
{
public:
    FVerifyCrudContract(
        const TSharedRef<FCrudTestState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount < 4)
        {
            return false;
        }

        Test->TestTrue(TEXT("Create succeeds"), State->bCreateSucceeded);
        Test->TestTrue(TEXT("Update succeeds"), State->bUpdateSucceeded);
        Test->TestTrue(TEXT("An empty 204 delete succeeds"), State->bDeleteSucceeded);
        Test->TestTrue(TEXT("First matching record succeeds"), State->bFirstSucceeded);
        Test->TestTrue(TEXT("Unknown response fields are retained"), State->bUnknownFieldRetained);
        Test->TestTrue(TEXT("Record data excludes typed system fields"), State->bRecordDataClean);
        Test->TestTrue(TEXT("Nested expansions are retained"), State->bNestedExpansionRetained);
        Test->TestTrue(TEXT("Back-relation expansions are retained"), State->bBackRelationRetained);
        Test->TestEqual(TEXT("First matching record is returned"), State->FirstId, FString(TEXT("first123")));
        Test->TestEqual(TEXT("Four requests reach the shared transport"), State->Transport->Requests.Num(), 4);
        if (State->Transport->Requests.Num() != 4)
        {
            State->Client->Shutdown();
            return true;
        }

        const FOpenPocketBaseHttpRequest& CreateRequest = State->Transport->Requests[0];
        Test->TestEqual(TEXT("Create uses POST"), CreateRequest.Method, FString(TEXT("POST")));
        Test->TestTrue(
            TEXT("Create accepts a collection name and record projections"),
            CreateRequest.Url.Contains(TEXT("/api/collections/tasks/records?")) &&
                CreateRequest.Url.Contains(TEXT("expand=owner.team")) &&
                CreateRequest.Url.Contains(TEXT("fields=")));

        TSharedPtr<FJsonObject> BodyObject;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FromUtf8(CreateRequest.Body));
        Test->TestTrue(TEXT("Create sends a JSON object"), FJsonSerializer::Deserialize(Reader, BodyObject));
        if (BodyObject.IsValid())
        {
            Test->TestEqual(TEXT("Replace uses the plain field name"), BodyObject->GetStringField(TEXT("title")), FString(TEXT("Created")));
            Test->TestEqual(TEXT("Append uses the suffix modifier"), BodyObject->GetNumberField(TEXT("score+")), 2.0);
            Test->TestTrue(TEXT("Prepend uses the prefix modifier"), BodyObject->HasField(TEXT("+tags")));
            Test->TestTrue(TEXT("Remove uses the suffix modifier"), BodyObject->HasField(TEXT("tags-")));
        }

        const FOpenPocketBaseHttpRequest& UpdateRequest = State->Transport->Requests[1];
        Test->TestEqual(TEXT("Update uses PATCH"), UpdateRequest.Method, FString(TEXT("PATCH")));
        Test->TestTrue(
            TEXT("Update accepts a collection ID and record ID"),
            UpdateRequest.Url.Contains(TEXT("/api/collections/tasks_id/records/task123")));

        const FOpenPocketBaseHttpRequest& DeleteRequest = State->Transport->Requests[2];
        Test->TestEqual(TEXT("Delete uses DELETE"), DeleteRequest.Method, FString(TEXT("DELETE")));
        Test->TestTrue(TEXT("Delete sends no body"), DeleteRequest.Body.IsEmpty());

        const FOpenPocketBaseHttpRequest& FirstRequest = State->Transport->Requests[3];
        Test->TestEqual(TEXT("First matching record uses GET"), FirstRequest.Method, FString(TEXT("GET")));
        Test->TestTrue(TEXT("First matching record requests one item"), FirstRequest.Url.Contains(TEXT("page=1&perPage=1")));
        Test->TestTrue(TEXT("First matching record skips totals"), FirstRequest.Url.Contains(TEXT("skipTotal=true")));
        Test->TestTrue(TEXT("First matching record sends the filter"), FirstRequest.Url.Contains(TEXT("filter=")));

        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FCrudTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordCrudContractTest,
    "OpenPocketBase.Client.Records.CrudUsesSharedLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordCrudContractTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FCrudTestState, ESPMode::ThreadSafe> State =
        MakeShared<FCrudTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FCrudTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError CreateError;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), CreateError);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRecordBody Body;
    Body.SetDynamicStringField(TEXT("title"), TEXT("Created"));
    Body.SetDynamicNumberField(TEXT("score"), 2.0, EOpenPocketBaseFieldModifier::Append);
    Body.SetDynamicStringArrayField(TEXT("tags"), {TEXT("urgent")}, EOpenPocketBaseFieldModifier::Prepend);
    Body.SetDynamicStringArrayField(TEXT("tags"), {TEXT("old")}, EOpenPocketBaseFieldModifier::Remove);

    UOpenPocketBaseSchema* Schema = NewObject<UOpenPocketBaseSchema>();
    Schema->SchemaId = FGuid(34, 55, 89, 144);
    FOpenPocketBaseSchemaCollection Tasks;
    Tasks.Id = TEXT("tasks_id");
    Tasks.Name = TEXT("tasks");
    FOpenPocketBaseSchemaField Id;
    Id.Id = TEXT("id_id");
    Id.Name = TEXT("id");
    Id.Type = EOpenPocketBaseFieldType::Text;
    FOpenPocketBaseSchemaField Title;
    Title.Id = TEXT("title_id");
    Title.Name = TEXT("title");
    Title.Type = EOpenPocketBaseFieldType::Text;
    FOpenPocketBaseSchemaField Owner;
    Owner.Id = TEXT("owner_id");
    Owner.Name = TEXT("owner");
    Owner.Type = EOpenPocketBaseFieldType::Relation;
    Owner.RelatedCollectionId = TEXT("users_id");
    Tasks.Fields = {Id, Title, Owner};
    FOpenPocketBaseSchemaCollection Users;
    Users.Id = TEXT("users_id");
    Users.Name = TEXT("users");
    FOpenPocketBaseSchemaField Team;
    Team.Id = TEXT("team_id");
    Team.Name = TEXT("team");
    Team.Type = EOpenPocketBaseFieldType::Relation;
    Team.RelatedCollectionId = TEXT("teams_id");
    Users.Fields = {Team};
    Schema->Collections = {Tasks, Users};

    FOpenPocketBaseCollectionRef TasksRef;
    FOpenPocketBaseCollectionRef UsersRef;
    Schema->MakeCollectionRef(Tasks.Id, TasksRef);
    Schema->MakeCollectionRef(Users.Id, UsersRef);
    FOpenPocketBaseAnyFieldRef IdRef;
    FOpenPocketBaseStringFieldRef TitleRef;
    FOpenPocketBaseRelationFieldRef OwnerRef;
    FOpenPocketBaseRelationFieldRef TeamRef;
    Schema->MakeTypedFieldRef(TasksRef, Id.Id, IdRef);
    Schema->MakeTypedFieldRef(TasksRef, Title.Id, TitleRef);
    Schema->MakeTypedFieldRef(TasksRef, Owner.Id, OwnerRef);
    Schema->MakeTypedFieldRef(UsersRef, Team.Id, TeamRef);

    const FOpenPocketBaseExpand OwnerExpand = OpenPocketBase::Query::Expand(OwnerRef);
    FOpenPocketBaseRecordOptions RecordOptions;
    RecordOptions
        .Including(OpenPocketBase::Query::ThenExpand(OwnerExpand, TeamRef))
        .Selecting(OpenPocketBase::Query::Select(IdRef))
        .Selecting(OpenPocketBase::Query::SelectExcerpt(TitleRef, 40, true))
        .Selecting(OpenPocketBase::Query::SelectExpandedRecord(OwnerExpand));

    State->Client->DynamicCollection(TEXT("tasks")).Create(
        Body,
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bCreateSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                const FOpenPocketBaseRecord& Record = Result.GetValue();
                State->bUnknownFieldRetained =
                    Record.Data.JsonObject.IsValid() && Record.Data.JsonObject->HasField(TEXT("futureField"));
                State->bRecordDataClean = Record.Data.JsonObject.IsValid() &&
                    Record.Data.JsonObject->HasField(TEXT("title")) &&
                    !Record.Data.JsonObject->HasField(TEXT("id")) &&
                    !Record.Data.JsonObject->HasField(TEXT("collectionId")) &&
                    !Record.Data.JsonObject->HasField(TEXT("collectionName")) &&
                    !Record.Data.JsonObject->HasField(TEXT("created")) &&
                    !Record.Data.JsonObject->HasField(TEXT("updated")) &&
                    !Record.Data.JsonObject->HasField(TEXT("expand"));
                if (Record.Expanded.JsonObject.IsValid())
                {
                    const TSharedPtr<FJsonObject>* Owner = nullptr;
                    const TArray<TSharedPtr<FJsonValue>>* BackRelations = nullptr;
                    State->bNestedExpansionRetained =
                        Record.Expanded.JsonObject->TryGetObjectField(TEXT("owner"), Owner) &&
                        Owner != nullptr && (*Owner)->HasField(TEXT("expand"));
                    State->bBackRelationRetained =
                        Record.Expanded.JsonObject->TryGetArrayField(TEXT("tasks_via_parent"), BackRelations) &&
                        BackRelations != nullptr && BackRelations->Num() == 1;
                }
            }
            ++State->CompletionCount;
        },
        RecordOptions);

    State->Client->DynamicCollection(TEXT("tasks_id")).Update(
        TEXT("task123"),
        Body,
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bUpdateSucceeded = Result.IsSuccess();
            ++State->CompletionCount;
        });

    State->Client->DynamicCollection(TEXT("tasks")).Delete(
        TEXT("task123"),
        [State](TOpenPocketBaseResult<bool>&& Result)
        {
            State->bDeleteSucceeded = Result.IsSuccess() && Result.GetValue();
            ++State->CompletionCount;
        });

    State->Client->DynamicCollection(TEXT("tasks")).GetFirstListItem(
        FOpenPocketBaseFilter::DynamicString(
            TEXT("status"),
            EOpenPocketBaseStringComparison::Equals,
            TEXT("open")),
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bFirstSucceeded = Result.IsSuccess();
            if (Result.IsSuccess())
            {
                State->FirstId = Result.GetValue().Id;
            }
            ++State->CompletionCount;
        },
        RecordOptions);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyCrudContract(State, this));
    return true;
}

namespace
{
struct FMutationErrorTestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FCrudTransport, ESPMode::ThreadSafe> Transport;
    int32 CompletionCount = 0;
    FOpenPocketBaseError ValidationError;
    FOpenPocketBaseError RuleError;
    FOpenPocketBaseError AmbiguousError;
};

class FVerifyMutationErrors final : public IAutomationLatentCommand
{
public:
    FVerifyMutationErrors(
        const TSharedRef<FMutationErrorTestState, ESPMode::ThreadSafe>& InState,
        FAutomationTestBase* InTest)
        : State(InState)
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount < 3)
        {
            return false;
        }

        Test->TestEqual(TEXT("Validation errors keep their HTTP status"), State->ValidationError.HttpStatus, 400);
        const FOpenPocketBaseFieldError* TitleError = State->ValidationError.FieldErrors.Find(TEXT("title"));
        Test->TestNotNull(TEXT("Validation errors retain field details"), TitleError);
        if (TitleError != nullptr)
        {
            Test->TestEqual(TEXT("Validation error codes are retained"), TitleError->Code, FString(TEXT("validation_required")));
        }
        Test->TestEqual(TEXT("Rule-protected responses keep their HTTP status"), State->RuleError.HttpStatus, 403);
        Test->TestEqual(TEXT("Ambiguous mutation failures are transport errors"), State->AmbiguousError.Kind, EOpenPocketBaseErrorKind::Transport);
        Test->TestEqual(TEXT("Mutations are never retried automatically"), State->Transport->Requests.Num(), 3);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FMutationErrorTestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseRecordMutationErrorTest,
    "OpenPocketBase.Client.Records.MutationsPreserveErrorsWithoutRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseRecordMutationErrorTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FMutationErrorTestState, ESPMode::ThreadSafe> State =
        MakeShared<FMutationErrorTestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FCrudTransport, ESPMode::ThreadSafe>();

    FOpenPocketBaseHttpResponse ValidationResponse;
    ValidationResponse.bTransportSucceeded = true;
    ValidationResponse.HttpStatus = 400;
    ValidationResponse.Body = ToUtf8(
        TEXT("{\"status\":400,\"message\":\"Failed to create record.\",\"data\":{")
        TEXT("\"title\":{\"code\":\"validation_required\",\"message\":\"Missing required value.\"}}}"));
    State->Transport->Responses.Add(MoveTemp(ValidationResponse));

    FOpenPocketBaseHttpResponse RuleResponse;
    RuleResponse.bTransportSucceeded = true;
    RuleResponse.HttpStatus = 403;
    RuleResponse.Body = ToUtf8(
        TEXT("{\"status\":403,\"message\":\"Only authenticated users can perform this action.\",\"data\":{}}"));
    State->Transport->Responses.Add(MoveTemp(RuleResponse));

    FOpenPocketBaseHttpResponse AmbiguousResponse;
    AmbiguousResponse.ErrorMessage = TEXT("Connection closed before a response was received.");
    State->Transport->Responses.Add(MoveTemp(AmbiguousResponse));

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError CreateError;
    State->Client = CreateOpenPocketBaseTestClient(Config, State->Transport.ToSharedRef(), CreateError);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRecordBody Body;
    Body.SetDynamicStringField(TEXT("title"), TEXT("Created"));
    FOpenPocketBaseRecordOptions Options;
    Options.RequestOptions.MaxReadRetries = 5;

    State->Client->DynamicCollection(TEXT("tasks")).Create(
        Body,
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->ValidationError = Result.GetError();
            }
            ++State->CompletionCount;
        },
        Options);
    State->Client->DynamicCollection(TEXT("tasks")).Update(
        TEXT("task123"),
        Body,
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->RuleError = Result.GetError();
            }
            ++State->CompletionCount;
        },
        Options);
    State->Client->DynamicCollection(TEXT("tasks")).Delete(
        TEXT("task123"),
        [State](TOpenPocketBaseResult<bool>&& Result)
        {
            if (!Result.IsSuccess())
            {
                State->AmbiguousError = Result.GetError();
            }
            ++State->CompletionCount;
        },
        Options.RequestOptions);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyMutationErrors(State, this));
    return true;
}

#endif
