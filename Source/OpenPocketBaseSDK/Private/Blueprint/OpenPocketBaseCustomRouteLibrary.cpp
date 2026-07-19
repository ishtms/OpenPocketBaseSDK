#include "OpenPocketBaseCustomRouteLibrary.h"

FOpenPocketBaseCustomRouteRequest UOpenPocketBaseCustomRouteLibrary::NoBodyRoute(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    return OpenPocketBase::DynamicRoute::NoBody(
        Method,
        MoveTemp(Path),
        bUseAuth,
        MoveTemp(Query),
        MoveTemp(Options));
}

FOpenPocketBaseCustomRouteRequest UOpenPocketBaseCustomRouteLibrary::JsonRoute(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    FJsonObjectWrapper Body,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    return OpenPocketBase::DynamicRoute::Json(
        Method,
        MoveTemp(Path),
        MoveTemp(Body),
        bUseAuth,
        MoveTemp(Query),
        MoveTemp(Options));
}

FOpenPocketBaseCustomRouteRequest UOpenPocketBaseCustomRouteLibrary::FormRoute(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    TMap<FString, FString> Fields,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    return OpenPocketBase::DynamicRoute::Form(
        Method,
        MoveTemp(Path),
        MoveTemp(Fields),
        bUseAuth,
        MoveTemp(Query),
        MoveTemp(Options));
}

FOpenPocketBaseCustomRouteRequest UOpenPocketBaseCustomRouteLibrary::MultipartRoute(
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
    return OpenPocketBase::DynamicRoute::Multipart(
        Method,
        MoveTemp(Path),
        MoveTemp(Fields),
        MoveTemp(Files),
        bUseAuth,
        MoveTemp(Query),
        UploadLimits,
        MaxRequestBytes,
        MoveTemp(Options));
}

FOpenPocketBaseCustomRouteRequest UOpenPocketBaseCustomRouteLibrary::TextRoute(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    FString Body,
    FString ContentType,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    return OpenPocketBase::DynamicRoute::Text(
        Method,
        MoveTemp(Path),
        Body,
        MoveTemp(ContentType),
        bUseAuth,
        MoveTemp(Query),
        MoveTemp(Options));
}

FOpenPocketBaseCustomRouteRequest UOpenPocketBaseCustomRouteLibrary::BinaryRoute(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    TArray<uint8> Body,
    FString ContentType,
    const bool bUseAuth,
    TMap<FString, FString> Query,
    FOpenPocketBaseRequestOptions Options)
{
    return OpenPocketBase::DynamicRoute::Binary(
        Method,
        MoveTemp(Path),
        MoveTemp(Body),
        MoveTemp(ContentType),
        bUseAuth,
        MoveTemp(Query),
        MoveTemp(Options));
}
