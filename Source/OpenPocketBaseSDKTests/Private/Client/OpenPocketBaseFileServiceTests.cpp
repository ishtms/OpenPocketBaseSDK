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

class FProtectedFileErrorTransport final : public IOpenPocketBaseTransport
{
public:
    TArray<FOpenPocketBaseHttpRequest> Requests;

    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        const int32 RequestIndex = Requests.Num();
        Requests.Add(Request);
        FOpenPocketBaseHttpResponse Response;
        Response.RequestId = Request.RequestId;
        Response.EffectiveUrl = Request.Url;
        if (RequestIndex == 0)
        {
            Response.bTransportSucceeded = true;
            Response.HttpStatus = 200;
            Response.Body = FileServiceUtf8(
                TEXT("{\"token\":\"header.payload.signature\",\"record\":{")
                TEXT("\"id\":\"user123\",\"collectionId\":\"users_id\",")
                TEXT("\"collectionName\":\"users\"}}"));
        }
        else if (RequestIndex == 1)
        {
            Response.bTransportSucceeded = true;
            Response.HttpStatus = 200;
            Response.Body = FileServiceUtf8(TEXT("{\"token\":\"top-secret-file-token\"}"));
        }
        else
        {
            Response.bTimedOut = true;
            Response.ErrorMessage = TEXT("Timeout while fetching ") + Request.Url;
        }
        OnComplete(MoveTemp(Response));
        return {};
    }
};

struct FProtectedFileErrorState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FProtectedFileErrorTransport, ESPMode::ThreadSafe> Transport;
    FOpenPocketBaseError Error;
    bool bCompleted = false;
};

class FVerifyProtectedFileError final : public IAutomationLatentCommand
{
public:
    FVerifyProtectedFileError(
        TSharedRef<FProtectedFileErrorState, ESPMode::ThreadSafe> InState,
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
        Test->TestEqual(TEXT("The download preserves timeout classification"), State->Error.Kind, EOpenPocketBaseErrorKind::Timeout);
        Test->TestFalse(TEXT("The protected token is redacted"), State->Error.ServerMessage.Contains(TEXT("top-secret-file-token")));
        Test->TestFalse(TEXT("The protected query is redacted"), State->Error.ServerMessage.Contains(TEXT("token=")));
        Test->TestEqual(TEXT("A protected download is never retried"), State->Transport->Requests.Num(), 3);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FProtectedFileErrorState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseProtectedFileErrorTest,
    "OpenPocketBase.Client.Files.RedactsProtectedDownloadErrors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseProtectedFileErrorTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FProtectedFileErrorState, ESPMode::ThreadSafe> State =
        MakeShared<FProtectedFileErrorState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FProtectedFileErrorTransport, ESPMode::ThreadSafe>();
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
                State->Error = AuthResult.GetError();
                State->bCompleted = true;
                return;
            }
            State->Client->Files().GetToken(
                [State](TOpenPocketBaseResult<FOpenPocketBaseFileToken>&& TokenResult)
                {
                    if (!TokenResult.IsSuccess())
                    {
                        State->Error = TokenResult.GetError();
                        State->bCompleted = true;
                        return;
                    }
                    FOpenPocketBaseFileDownloadOptions Options;
                    Options.MaxBytes = 1024;
                    State->Client->Files().Download(
                        TEXT("tasks"),
                        TEXT("record-1"),
                        TEXT("protected.txt"),
                        Options,
                        [State](TOpenPocketBaseResult<FOpenPocketBaseFileDownloadResult>&& Result)
                        {
                            if (!Result.IsSuccess())
                            {
                                State->Error = Result.GetError();
                            }
                            State->bCompleted = true;
                        },
                        MoveTemp(TokenResult.GetValue()));
                });
        });

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyProtectedFileError(State, this));
    return true;
}

#endif
