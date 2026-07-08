#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Clock/OpenPocketBaseClock.h"
#include "OpenPocketBaseClient.h"
#include "OpenPocketBaseCustomRoute.h"
#include "SecureStorage/OpenPocketBaseSecureStore.h"

namespace
{
TArray<uint8> CustomUtf8(const FString& Value)
{
    const FTCHARToUTF8 Converted(*Value);
    TArray<uint8> Bytes;
    Bytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
    return Bytes;
}

FString CustomBody(const FOpenPocketBaseHttpRequest& Request)
{
    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()),
        Request.Body.Num());
    return FString(Converted.Length(), Converted.Get());
}

class FCustomRouteClock final : public IOpenPocketBaseClock
{
public:
    virtual FDateTime UtcNow() const override
    {
        return FDateTime(2026, 8, 23, 0, 0, 0);
    }

    virtual double MonotonicSeconds() const override
    {
        Seconds += 0.025;
        return Seconds;
    }

    virtual FOpenPocketBaseClockHandle Schedule(
        const double DelaySeconds,
        TUniqueFunction<void()> Callback) override
    {
        return {};
    }

private:
    mutable double Seconds = 10.0;
};

class FCustomRouteTransport final : public IOpenPocketBaseTransport
{
public:
    virtual FOpenPocketBaseTransportHandle Send(
        FOpenPocketBaseHttpRequest&& Request,
        FOpenPocketBaseHttpChunkCallback OnChunk,
        FOpenPocketBaseHttpCompleteCallback OnComplete) override
    {
        if (Request.BodyStream.IsValid())
        {
            Request.Body.SetNumUninitialized(Request.BodyLength);
            Request.BodyStream->Serialize(Request.Body.GetData(), Request.BodyLength);
            Request.BodyStream.Reset();
        }
        const int32 Index = Requests.Add(MoveTemp(Request));
        FOpenPocketBaseHttpResponse Response = MoveTemp(Responses[0]);
        Responses.RemoveAt(0, EAllowShrinking::No);
        Response.RequestId = Requests[Index].RequestId;
        Response.EffectiveUrl = Requests[Index].Url;
        OnComplete(MoveTemp(Response));
        return {};
    }

    void AddResponse(
        const int32 Status,
        const FString& ContentType,
        TArray<uint8> Body)
    {
        FOpenPocketBaseHttpResponse Response;
        Response.bTransportSucceeded = true;
        Response.HttpStatus = Status;
        Response.Headers.Add(TEXT("Content-Type"), ContentType);
        Response.Body = MoveTemp(Body);
        Responses.Add(MoveTemp(Response));
    }

    TArray<FOpenPocketBaseHttpRequest> Requests;
    TArray<FOpenPocketBaseHttpResponse> Responses;
};

struct FCustomRouteState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FCustomRouteTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bSucceeded = true;
    FOpenPocketBaseHealthResult Health;
    TArray<FOpenPocketBaseCustomRouteResponse> Responses;
};

struct FCustomRouteValidationState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FCustomRouteTransport, ESPMode::ThreadSafe> Transport;
    int32 CompletionCount = 0;
    TArray<FOpenPocketBaseError> Errors;
};

struct FCustomRouteJsonRootState
{
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Client;
    TSharedPtr<FCustomRouteTransport, ESPMode::ThreadSafe> Transport;
    bool bCompleted = false;
    bool bArrayRetained = false;
};

class FCustomRouteFlow final
    : public TSharedFromThis<FCustomRouteFlow, ESPMode::ThreadSafe>
{
public:
    explicit FCustomRouteFlow(TSharedRef<FCustomRouteState, ESPMode::ThreadSafe> InState)
        : State(MoveTemp(InState))
    {
    }

    void Start()
    {
        const TSharedRef<FCustomRouteFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->Health(
            [Self](TOpenPocketBaseResult<FOpenPocketBaseHealthResult>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                if (Result.IsSuccess())
                {
                    Self->State->Health = MoveTemp(Result.GetValue());
                }
                Self->SendJson();
            });
    }

private:
    void SendJson()
    {
        FOpenPocketBaseCustomRouteRequest Request;
        Request.Method = EOpenPocketBaseCustomRouteMethod::Post;
        Request.Path = TEXT("/api/project/echo");
        Request.Query.Add(TEXT("view"), TEXT("full value"));
        Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Json;
        Request.JsonBody.JsonObject = MakeShared<FJsonObject>();
        Request.JsonBody.JsonObject->SetStringField(TEXT("kind"), TEXT("json"));
        Request.Options.AdditionalHeaders.Add(TEXT("X-Project-Trace"), TEXT("trace-one"));
        Request.Options.TraceParent =
            TEXT("00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01");
        Send(MoveTemp(Request), &FCustomRouteFlow::SendForm);
    }

    void SendForm()
    {
        FOpenPocketBaseCustomRouteRequest Request;
        Request.Method = EOpenPocketBaseCustomRouteMethod::Post;
        Request.Path = TEXT("/api/project/form");
        Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Form;
        Request.FormFields.Add(TEXT("title"), TEXT("hello world"));
        Send(MoveTemp(Request), &FCustomRouteFlow::SendMultipart);
    }

    void SendMultipart()
    {
        FOpenPocketBaseCustomRouteRequest Request;
        Request.Method = EOpenPocketBaseCustomRouteMethod::Post;
        Request.Path = TEXT("/api/project/upload");
        Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Multipart;
        Request.FormFields.Add(TEXT("description"), TEXT("small upload"));
        FOpenPocketBaseFileInput File;
        File.FieldName = TEXT("asset");
        File.FileName = TEXT("asset.txt");
        File.ContentType = TEXT("text/plain");
        File.bUseFilePath = false;
        File.Bytes = {'f', 'i', 'l', 'e'};
        Request.Files.Add(MoveTemp(File));
        Send(MoveTemp(Request), &FCustomRouteFlow::SendRaw);
    }

    void SendRaw()
    {
        FOpenPocketBaseCustomRouteRequest Request;
        Request.Method = EOpenPocketBaseCustomRouteMethod::Put;
        Request.Path = TEXT("/api/project/raw");
        Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Raw;
        Request.ContentType = TEXT("text/plain; charset=utf-8");
        Request.Body = CustomUtf8(TEXT("raw-body"));
        Send(MoveTemp(Request), &FCustomRouteFlow::SendBinary);
    }

    void SendBinary()
    {
        FOpenPocketBaseCustomRouteRequest Request;
        Request.Method = EOpenPocketBaseCustomRouteMethod::Patch;
        Request.Path = TEXT("/api/project/binary");
        Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Binary;
        Request.ContentType = TEXT("application/octet-stream");
        Request.Body = {0, 1, 2, 255};
        Send(MoveTemp(Request), nullptr);
    }

    using FNext = void (FCustomRouteFlow::*)();

    void Send(FOpenPocketBaseCustomRouteRequest Request, const FNext Next)
    {
        const TSharedRef<FCustomRouteFlow, ESPMode::ThreadSafe> Self = AsShared();
        State->Client->SendCustomRoute(
            MoveTemp(Request),
            [Self, Next](TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>&& Result)
            {
                Self->State->bSucceeded = Self->State->bSucceeded && Result.IsSuccess();
                if (Result.IsSuccess())
                {
                    Self->State->Responses.Add(MoveTemp(Result.GetValue()));
                }
                if (Next != nullptr)
                {
                    (Self.Get().*Next)();
                }
                else
                {
                    Self->State->bCompleted = true;
                }
            });
    }

    TSharedRef<FCustomRouteState, ESPMode::ThreadSafe> State;
};

class FVerifyCustomRoutes final : public IAutomationLatentCommand
{
public:
    FVerifyCustomRoutes(
        TSharedRef<FCustomRouteState, ESPMode::ThreadSafe> InState,
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
        Test->TestTrue(TEXT("Health and every custom body format succeed"),
            State->bSucceeded);
        Test->TestTrue(TEXT("Health returns the typed server code"),
            State->Health.bHealthy && State->Health.Code == 200);
        Test->TestTrue(TEXT("Health includes measured timing"),
            State->Health.DurationSeconds > 0.0);
        Test->TestEqual(TEXT("Six requests are sent"), State->Transport->Requests.Num(), 6);
        Test->TestEqual(TEXT("Five custom responses are returned"),
            State->Responses.Num(), 5);
        if (State->Transport->Requests.Num() == 6)
        {
            Test->TestEqual(TEXT("Health uses the pinned route"),
                State->Transport->Requests[0].Url,
                FString(TEXT("https://pb.example.test/api/health")));
            Test->TestTrue(TEXT("Custom query values are encoded"),
                State->Transport->Requests[1].Url.EndsWith(TEXT("?view=full%20value")));
            Test->TestEqual(TEXT("The approved custom header is forwarded"),
                State->Transport->Requests[1].Headers.FindRef(TEXT("X-Project-Trace")),
                FString(TEXT("trace-one")));
            Test->TestTrue(TEXT("The dedicated trace context is forwarded"),
                State->Transport->Requests[1].Headers.Contains(TEXT("traceparent")));
            Test->TestTrue(TEXT("JSON is serialized once"),
                CustomBody(State->Transport->Requests[1]).Contains(
                    TEXT("\"kind\": \"json\"")));
            Test->TestEqual(TEXT("Form fields use URL encoding"),
                CustomBody(State->Transport->Requests[2]),
                FString(TEXT("title=hello%20world")));
            Test->TestTrue(TEXT("Multipart includes fields without JSON conventions"),
                CustomBody(State->Transport->Requests[3]).Contains(
                    TEXT("name=\"description\"")) &&
                !CustomBody(State->Transport->Requests[3]).Contains(TEXT("@jsonPayload")));
            Test->TestTrue(TEXT("Multipart includes the file bytes"),
                CustomBody(State->Transport->Requests[3]).Contains(TEXT("file")));
            Test->TestEqual(TEXT("Raw bytes are preserved"),
                CustomBody(State->Transport->Requests[4]), FString(TEXT("raw-body")));
            Test->TestEqual(TEXT("Binary bytes are preserved"),
                State->Transport->Requests[5].Body,
                TArray<uint8>({0, 1, 2, 255}));
        }
        if (State->Responses.Num() == 5)
        {
            Test->TestTrue(TEXT("JSON custom responses retain parsed JSON"),
                State->Responses[0].bHasJson &&
                State->Responses[0].JsonBody.JsonObject.IsValid());
            Test->TestEqual(TEXT("Binary custom responses retain exact bytes"),
                State->Responses[4].Body,
                TArray<uint8>({9, 8, 7, 6}));
        }
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FCustomRouteState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyCustomRouteValidation final : public IAutomationLatentCommand
{
public:
    FVerifyCustomRouteValidation(
        TSharedRef<FCustomRouteValidationState, ESPMode::ThreadSafe> InState,
        FAutomationTestBase* InTest)
        : State(MoveTemp(InState))
        , Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (State->CompletionCount != 4)
        {
            return false;
        }
        Test->TestEqual(TEXT("Invalid custom routes never reach the transport"),
            State->Transport->Requests.Num(), 0);
        Test->TestEqual(TEXT("Every invalid custom route returns an error"),
            State->Errors.Num(), 4);
        for (const FOpenPocketBaseError& Error : State->Errors)
        {
            Test->TestEqual(TEXT("The validation failure is typed"),
                Error.Kind, EOpenPocketBaseErrorKind::InvalidArgument);
        }
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FCustomRouteValidationState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};

class FVerifyCustomRouteJsonRoot final : public IAutomationLatentCommand
{
public:
    FVerifyCustomRouteJsonRoot(
        TSharedRef<FCustomRouteJsonRootState, ESPMode::ThreadSafe> InState,
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
        Test->TestTrue(TEXT("A JSON array root is parsed and retained"),
            State->bArrayRetained);
        State->Client->Shutdown();
        return true;
    }

private:
    TSharedRef<FCustomRouteJsonRootState, ESPMode::ThreadSafe> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseCustomRouteTest,
    "OpenPocketBase.Client.CustomRoutes.HealthAndBoundedBodyFormats",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseCustomRouteTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FCustomRouteState, ESPMode::ThreadSafe> State =
        MakeShared<FCustomRouteState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FCustomRouteTransport, ESPMode::ThreadSafe>();
    State->Transport->AddResponse(
        200,
        TEXT("application/json"),
        CustomUtf8(TEXT("{\"code\":200,\"message\":\"API is healthy.\",\"data\":{}}")));
    State->Transport->AddResponse(
        200, TEXT("application/json"), CustomUtf8(TEXT("{\"echo\":true}")));
    State->Transport->AddResponse(
        200, TEXT("text/plain"), CustomUtf8(TEXT("form-ok")));
    State->Transport->AddResponse(
        201, TEXT("text/plain"), CustomUtf8(TEXT("upload-ok")));
    State->Transport->AddResponse(
        200, TEXT("text/plain"), CustomUtf8(TEXT("raw-ok")));
    State->Transport->AddResponse(
        200, TEXT("application/octet-stream"), {9, 8, 7, 6});

    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        MakeShared<FCustomRouteClock, ESPMode::ThreadSafe>(),
        Error);
    if (!TestTrue(TEXT("The custom-route client is created"), State->Client.IsValid()))
    {
        return false;
    }

    const TSharedRef<FCustomRouteFlow, ESPMode::ThreadSafe> Flow =
        MakeShared<FCustomRouteFlow, ESPMode::ThreadSafe>(State);
    Flow->Start();
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyCustomRoutes(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseCustomRouteValidationTest,
    "OpenPocketBase.Client.CustomRoutes.ProtectsTransportBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseCustomRouteValidationTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FCustomRouteValidationState, ESPMode::ThreadSafe> State =
        MakeShared<FCustomRouteValidationState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FCustomRouteTransport, ESPMode::ThreadSafe>();
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        MakeShared<FCustomRouteClock, ESPMode::ThreadSafe>(),
        Error);
    if (!TestTrue(TEXT("The validation client is created"), State->Client.IsValid()))
    {
        return false;
    }

    const auto OnFailure = [State](
        TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>&& Result)
    {
        if (!Result.IsSuccess())
        {
            State->Errors.Add(Result.GetError());
        }
        ++State->CompletionCount;
    };

    FOpenPocketBaseCustomRouteRequest UnsafePath;
    UnsafePath.Path = TEXT("https://other.example/api");
    State->Client->SendCustomRoute(MoveTemp(UnsafePath), OnFailure);

    FOpenPocketBaseCustomRouteRequest ProtectedHeader;
    ProtectedHeader.Path = TEXT("/api/project/echo");
    ProtectedHeader.Options.AdditionalHeaders.Add(
        TEXT("Authorization"), TEXT("must-not-be-forwarded"));
    State->Client->SendCustomRoute(MoveTemp(ProtectedHeader), OnFailure);

    FOpenPocketBaseCustomRouteRequest GetWithBody;
    GetWithBody.Path = TEXT("/api/project/echo");
    GetWithBody.BodyFormat = EOpenPocketBaseCustomBodyFormat::Raw;
    GetWithBody.ContentType = TEXT("text/plain");
    GetWithBody.Body = {'n', 'o'};
    State->Client->SendCustomRoute(MoveTemp(GetWithBody), OnFailure);

    FOpenPocketBaseCustomRouteRequest InvalidTrace;
    InvalidTrace.Path = TEXT("/api/project/echo");
    InvalidTrace.Options.TraceParent =
        TEXT("00-4BF92F3577B34DA6A3CE929D0E0E4736-00f067aa0ba902b7-01");
    State->Client->SendCustomRoute(MoveTemp(InvalidTrace), OnFailure);

    ADD_LATENT_AUTOMATION_COMMAND(FVerifyCustomRouteValidation(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseCustomRouteJsonRootTest,
    "OpenPocketBase.Client.CustomRoutes.RetainsJsonArrayRoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseCustomRouteJsonRootTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FCustomRouteJsonRootState, ESPMode::ThreadSafe> State =
        MakeShared<FCustomRouteJsonRootState, ESPMode::ThreadSafe>();
    State->Transport = MakeShared<FCustomRouteTransport, ESPMode::ThreadSafe>();
    State->Transport->AddResponse(
        200,
        TEXT("application/json"),
        CustomUtf8(TEXT("[{\"id\":\"one\"}]")));
    FOpenPocketBaseClientConfig Config;
    Config.BaseUrl = TEXT("https://pb.example.test");
    FOpenPocketBaseError Error;
    State->Client = FOpenPocketBaseClient::Create(
        Config,
        State->Transport.ToSharedRef(),
        CreateOpenPocketBaseSecureStore(),
        MakeShared<FCustomRouteClock, ESPMode::ThreadSafe>(),
        Error);
    if (!TestTrue(TEXT("The JSON-root client is created"), State->Client.IsValid()))
    {
        return false;
    }

    FOpenPocketBaseCustomRouteRequest Request;
    Request.Path = TEXT("/api/project/items");
    State->Client->SendCustomRoute(
        MoveTemp(Request),
        [State](TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>&& Result)
        {
            if (Result.IsSuccess())
            {
                const TSharedPtr<FJsonValue>& Parsed = Result.GetValue().GetParsedJson();
                State->bArrayRetained = Result.GetValue().bHasJson &&
                    Result.GetValue().JsonRootType == EOpenPocketBaseJsonRootType::Array &&
                    Parsed.IsValid() && Parsed->Type == EJson::Array &&
                    Parsed->AsArray().Num() == 1;
            }
            State->bCompleted = true;
        });
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyCustomRouteJsonRoot(State, this));
    return true;
}

#endif
