#include "OpenPocketBaseCustomRoute.h"

namespace
{
FOpenPocketBaseCustomRouteRequest MakeRequest(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseCustomRouteRequest Request;
    Request.Method = Method;
    Request.Path = MoveTemp(Path);
    Request.Query = MoveTemp(Query);
    Request.bUseAuth = bUseAuth;
    Request.Options = MoveTemp(Options);
    return Request;
}
}

FOpenPocketBaseCustomRouteRequest OpenPocketBase::DynamicRoute::NoBody(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    return MakeRequest(Method, MoveTemp(Path), bUseAuth, MoveTemp(Query), MoveTemp(Options));
}

FOpenPocketBaseCustomRouteRequest OpenPocketBase::DynamicRoute::Json(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    FJsonObjectWrapper Body,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseCustomRouteRequest Request =
        MakeRequest(Method, MoveTemp(Path), bUseAuth, MoveTemp(Query), MoveTemp(Options));
    Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Json;
    Request.JsonBody = MoveTemp(Body);
    return Request;
}

FOpenPocketBaseCustomRouteRequest OpenPocketBase::DynamicRoute::Form(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    TMap<FString, FString> Fields,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseCustomRouteRequest Request =
        MakeRequest(Method, MoveTemp(Path), bUseAuth, MoveTemp(Query), MoveTemp(Options));
    Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Form;
    Request.FormFields = MoveTemp(Fields);
    return Request;
}

FOpenPocketBaseCustomRouteRequest OpenPocketBase::DynamicRoute::Multipart(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    TMap<FString, FString> Fields,
    TArray<FOpenPocketBaseFileInput> Files,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseUploadLimits UploadLimits,
    const int64 MaxRequestBytes,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseCustomRouteRequest Request =
        MakeRequest(Method, MoveTemp(Path), bUseAuth, MoveTemp(Query), MoveTemp(Options));
    Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Multipart;
    Request.FormFields = MoveTemp(Fields);
    Request.Files = MoveTemp(Files);
    Request.UploadLimits = UploadLimits;
    Request.MaxRequestBytes = MaxRequestBytes;
    return Request;
}

FOpenPocketBaseCustomRouteRequest OpenPocketBase::DynamicRoute::Text(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    const FString& Body,
    FString ContentType,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseCustomRouteRequest Request =
        MakeRequest(Method, MoveTemp(Path), bUseAuth, MoveTemp(Query), MoveTemp(Options));
    Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Raw;
    Request.ContentType = MoveTemp(ContentType);
    const FTCHARToUTF8 Utf8(*Body);
    Request.Body.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
    return Request;
}

FOpenPocketBaseCustomRouteRequest OpenPocketBase::DynamicRoute::Binary(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    TArray<uint8> Body,
    FString ContentType,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseCustomRouteRequest Request =
        MakeRequest(Method, MoveTemp(Path), bUseAuth, MoveTemp(Query), MoveTemp(Options));
    Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Binary;
    Request.Body = MoveTemp(Body);
    Request.ContentType = MoveTemp(ContentType);
    return Request;
}
