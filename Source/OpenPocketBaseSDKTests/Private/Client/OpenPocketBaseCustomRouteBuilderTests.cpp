#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "OpenPocketBaseCustomRouteLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseCustomRouteBuilderTest,
    "OpenPocketBase.Client.CustomRoutes.BuildsFocusedRequests",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseCustomRouteBuilderTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseRequestOptions Options;
    Options.ActivityTimeoutSeconds = 7.0;
    TMap<FString, FString> Query;
    Query.Add(TEXT("preview"), TEXT("true"));

    const FOpenPocketBaseCustomRouteRequest NoBody = OpenPocketBase::DynamicRoute::NoBody(
        EOpenPocketBaseCustomRouteMethod::Get,
        TEXT("/api/project/status"),
        true,
        Query,
        Options);
    TestEqual(TEXT("No-body routes retain the method"), NoBody.Method, EOpenPocketBaseCustomRouteMethod::Get);
    TestEqual(TEXT("No-body routes retain the path"), NoBody.Path, FString(TEXT("/api/project/status")));
    TestTrue(TEXT("No-body routes retain auth policy"), NoBody.bUseAuth);
    TestEqual(TEXT("No-body routes retain query values"), NoBody.Query.FindRef(TEXT("preview")), FString(TEXT("true")));
    TestEqual(TEXT("No-body routes retain request options"), NoBody.Options.ActivityTimeoutSeconds, 7.0);
    TestEqual(TEXT("No-body routes select no body format"), NoBody.BodyFormat, EOpenPocketBaseCustomBodyFormat::None);

    FJsonObjectWrapper Json;
    Json.JsonObject = MakeShared<FJsonObject>();
    Json.JsonObject->SetStringField(TEXT("title"), TEXT("hello"));
    const FOpenPocketBaseCustomRouteRequest JsonRequest = OpenPocketBase::DynamicRoute::Json(
        EOpenPocketBaseCustomRouteMethod::Post,
        TEXT("/api/project/json"),
        Json,
        false,
        {},
        {});
    TestEqual(TEXT("JSON routes select JSON format"), JsonRequest.BodyFormat, EOpenPocketBaseCustomBodyFormat::Json);
    TestTrue(TEXT("JSON routes retain their object"), JsonRequest.JsonBody.JsonObject.IsValid());

    const FOpenPocketBaseCustomRouteRequest TextRequest = OpenPocketBase::DynamicRoute::Text(
        EOpenPocketBaseCustomRouteMethod::Put,
        TEXT("/api/project/text"),
        TEXT("hello 世界"),
        TEXT("text/plain; charset=utf-8"),
        false,
        {},
        {});
    TestEqual(TEXT("Text routes select raw format"), TextRequest.BodyFormat, EOpenPocketBaseCustomBodyFormat::Raw);
    TestEqual(TEXT("Text routes retain content type"), TextRequest.ContentType, FString(TEXT("text/plain; charset=utf-8")));
    const FUTF8ToTCHAR Decoded(
        reinterpret_cast<const ANSICHAR*>(TextRequest.Body.GetData()),
        TextRequest.Body.Num());
    TestEqual(TEXT("Text bodies are UTF-8"), FString(Decoded.Length(), Decoded.Get()), FString(TEXT("hello 世界")));

    const UFunction* BlueprintBuilder =
        UOpenPocketBaseCustomRouteLibrary::StaticClass()->FindFunctionByName(TEXT("JsonRoute"));
    TestNotNull(TEXT("Blueprint exposes the focused JSON builder"), BlueprintBuilder);
    if (BlueprintBuilder != nullptr)
    {
        TestTrue(TEXT("The JSON builder is pure"), BlueprintBuilder->HasAnyFunctionFlags(FUNC_BlueprintPure));
        TestFalse(TEXT("Request options stay visible"), BlueprintBuilder->HasMetaData(TEXT("AdvancedDisplay")));
    }
    return true;
}

#endif
