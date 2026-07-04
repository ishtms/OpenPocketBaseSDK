#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseFile.h"
#include "Transport/OpenPocketBaseTransport.h"

namespace
{
TArray<uint8> FileServiceUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

class FFileServiceTransport final : public IOpenPocketBaseTransport
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
        Response.Body = Request.Url.EndsWith(TEXT("/api/files/token"))
            ? FileServiceUtf8(TEXT("{\"token\":\"short-lived-file-token\"}"))
            : FileServiceUtf8(
                TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
                TEXT("\"id\":\"user123\",\"collectionId\":\"users_id\",")
                TEXT("\"collectionName\":\"users\"}}"));
        OnComplete(MoveTemp(Response));
        return {};
    }
};

struct FFileServiceState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FFileServiceTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bTokenSucceeded = false;
    bool bTokenIsSet = false;
};

class FVerifyFileServiceToken final : public IAutomationLatentCommand
{
public:
    FVerifyFileServiceToken(
        TSharedRef<FFileServiceState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!State->bCompleted)
        {
            return false;
        }

        Test->TestTrue(TEXT("A protected-file token is parsed"), State->bTokenSucceeded);
        Test->TestTrue(TEXT("The opaque token is set"), State->bTokenIsSet);
        Test->TestEqual(TEXT("Login and token requests are sent"), State->Transport->Requests.Num(), 2);
        if (State->Transport->Requests.Num() == 2)
        {
            const FOpenPocketBaseHttpRequest& Request = State->Transport->Requests[1];
            Test->TestEqual(TEXT("Token acquisition uses POST"), Request.Method, FString(TEXT("POST")));
            Test->TestTrue(TEXT("Token acquisition uses the pinned route"), Request.Url.EndsWith(TEXT("/api/files/token")));
            Test->TestFalse(TEXT("Token acquisition carries auth"), Request.Headers.FindRef(TEXT("Authorization")).IsEmpty());
        }
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FFileServiceState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseFileUrlTest,
    "OpenPocketBase.Client.Files.BuildsValidatedUrls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseFileUrlTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client =
        FOpenPocketBaseClient::Create(Config, Error);
    if (!TestNotNull(TEXT("The client is created"), Client.Get()))
    {
        return false;
    }

    FOpenPocketBaseFileUrlOptions Options;
    Options.Thumbnail.Width = 70;
    Options.Thumbnail.Height = 50;
    Options.Thumbnail.Mode = EOpenPocketBaseThumbnailMode::CropTop;
    Options.bForceDownload = true;
    FString Url;
    TestTrue(
        TEXT("A valid record file URL is built"),
        Client->Files().TryBuildUrl(
            TEXT("tasks id"),
            TEXT("record-1"),
            TEXT("report final.png"),
            Options,
            Url,
            Error));
    TestEqual(
        TEXT("Every path and query value is encoded"),
        Url,
        FString(TEXT("https://pb.example.com/api/files/tasks%20id/record-1/report%20final.png?thumb=70x50t&download=true")));

    Options.Thumbnail.Width = 0;
    Options.Thumbnail.Height = 0;
    TestFalse(
        TEXT("A thumbnail cannot have two zero dimensions"),
        Client->Files().TryBuildUrl(TEXT("tasks"), TEXT("record-1"), TEXT("image.png"), Options, Url, Error));
    TestEqual(TEXT("Invalid thumbnails use InvalidArgument"), Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);
    TestFalse(
        TEXT("Traversal-like filenames are rejected"),
        Client->Files().TryBuildUrl(TEXT("tasks"), TEXT("record-1"), TEXT("../image.png"), {}, Url, Error));

    Client->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseFileTokenTest,
    "OpenPocketBase.Client.Files.RequestsProtectedToken",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseFileTokenTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FFileServiceState, ESPMode::ThreadSafe> State =
        MakeShared<FFileServiceState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FFileServiceTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.com");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(Config, State->Transport.ToSharedRef(), Error);
    if (!TestNotNull(TEXT("The client is created"), State->Client.Get()))
    {
        return false;
    }

    State->Client->Collection(TEXT("users")).AuthWithPassword(
        TEXT("player@example.com"),
        TEXT("correct-password"),
        [State](TOpenPocketBaseResult<FOpenPocketBaseAuthResult>&& AuthResult)
        {
            if (!AuthResult.IsSuccess())
            {
                State->bCompleted = true;
                return;
            }
            State->Client->Files().GetToken(
                [State](TOpenPocketBaseResult<FOpenPocketBaseFileToken>&& Result)
                {
                    State->bTokenSucceeded = Result.IsSuccess();
                    State->bTokenIsSet = Result.IsSuccess() && Result.GetValue().IsSet();
                    State->bCompleted = true;
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyFileServiceToken(State, this));
    return true;
}

#endif
