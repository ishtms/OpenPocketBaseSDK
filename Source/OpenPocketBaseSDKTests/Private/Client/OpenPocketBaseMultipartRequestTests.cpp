#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseFile.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace
{
class FMultipartRequestTransport final : public IOpenPocketBaseTransport
{
public:
    TArray<FOpenPocketBaseHttpRequest> Requests;

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        Requests.Add(Request);

        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = 200;
        Response.RequestId = Request.RequestId;
        Response.EffectiveUrl = Request.Url;
        const FString Json = FString::Printf(
            TEXT("{\"id\":\"task123\",\"collectionId\":\"tasks_id\",")
            TEXT("\"collectionName\":\"tasks\",\"title\":\"Uploaded\"}"));
        FTCHARToUTF8 Converted(*Json);
        Response.Body.Append(
            reinterpret_cast<const uint8*>(Converted.Get()),
            Converted.Length());
        OnComplete(MoveTemp(Response));
        return {};
    }
};

struct FMultipartRequestState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FMultipartRequestTransport, ESPMode::ThreadSafe> Transport;
    int32 CompletionCount = 0;
    bool bCreateSucceeded = false;
    bool bUpdateSucceeded = false;
};

class FVerifyMultipartRequests final : public IAutomationLatentCommand
{
public:
    FVerifyMultipartRequests(
        TSharedRef<FMultipartRequestState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount < 2)
        {
            return false;
        }

        Test->TestTrue(TEXT("Multipart create succeeds"), State->bCreateSucceeded);
        Test->TestTrue(TEXT("Multipart update succeeds"), State->bUpdateSucceeded);
        Test->TestEqual(TEXT("Two requests reach the transport"), State->Transport->Requests.Num(), 2);
        if (State->Transport->Requests.Num() == 2)
        {
            const FOpenPocketBaseHttpRequest& CreateRequest = State->Transport->Requests[0];
            const FOpenPocketBaseHttpRequest& UpdateRequest = State->Transport->Requests[1];
            Test->TestEqual(TEXT("Multipart create uses POST"), CreateRequest.Method, FString(TEXT("POST")));
            Test->TestEqual(TEXT("Multipart update uses PATCH"), UpdateRequest.Method, FString(TEXT("PATCH")));
            Test->TestTrue(TEXT("Multipart create uses a stream"), CreateRequest.BodyStream.IsValid());
            Test->TestTrue(TEXT("Multipart update uses a stream"), UpdateRequest.BodyStream.IsValid());
            Test->TestTrue(TEXT("Multipart requests do not duplicate bytes"), CreateRequest.Body.IsEmpty());
            Test->TestTrue(
                TEXT("Multipart content type is set"),
                CreateRequest.Headers.FindRef(TEXT("Content-Type")).StartsWith(TEXT("multipart/form-data; boundary=")));
            Test->TestEqual(
                TEXT("Content-Length matches the stream"),
                CreateRequest.Headers.FindRef(TEXT("Content-Length")),
                LexToString(CreateRequest.BodyLength));
            Test->TestEqual(
                TEXT("The body length matches the archive"),
                CreateRequest.BodyLength,
                CreateRequest.BodyStream.IsValid() ? CreateRequest.BodyStream->TotalSize() : 0);
        }

        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FMultipartRequestState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseMultipartRequestTest,
    "OpenPocketBase.Client.Files.CreateAndUpdateUseMultipartStreams",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseMultipartRequestTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FMultipartRequestState, ESPMode::ThreadSafe> State =
        MakeShared<FMultipartRequestState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FMultipartRequestTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError CreateError;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), CreateError);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseRecordBody Body;
    Body.SetStringField(TEXT("title"), TEXT("Uploaded"));
    FOpenPocketBaseFileInput File;
    File.FieldName = TEXT("attachment");
    File.FileName = TEXT("sample.bin");
    File.bUseFilePath = false;
    File.Bytes = {0x01, 0x02, 0x03};

    State->Client->Collection(TEXT("tasks")).CreateWithFiles(
        Body,
        {File},
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bCreateSucceeded = Result.IsSuccess();
            ++State->CompletionCount;
        });
    State->Client->Collection(TEXT("tasks")).UpdateWithFiles(
        TEXT("task123"),
        Body,
        {File},
        [State](TOpenPocketBaseResult<FOpenPocketBaseRecord>&& Result)
        {
            State->bUpdateSucceeded = Result.IsSuccess();
            ++State->CompletionCount;
        });
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyMultipartRequests(State, this));
    return true;
}

#endif
