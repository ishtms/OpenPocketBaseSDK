#include "OpenPocketBaseAdminClient.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "OpenPocketBaseDate.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FOpenPocketBaseError MakeAdminError(
    const EOpenPocketBaseErrorKind Kind,
    const TCHAR* Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = Kind;
    Error.ServerMessage = Message;
    return Error;
}

FOpenPocketBaseError MakeAdminCancelledError()
{
    return MakeAdminError(
        EOpenPocketBaseErrorKind::Cancelled,
        TEXT("The privileged request was cancelled."));
}

FOpenPocketBaseError SanitizeSensitiveError(
    FOpenPocketBaseError Error,
    const TCHAR* Message)
{
    Error.ServerCode.Reset();
    Error.ServerMessage = Message;
    Error.FieldErrors.Reset();
    return Error;
}

template <typename ValueType>
class TAdminCompletion final
{
public:
    explicit TAdminCompletion(
        TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> InCallback)
        : Callback(MoveTemp(InCallback))
    {
    }

    void Invoke(TOpenPocketBaseResult<ValueType>&& Result)
    {
        if (Callback)
        {
            TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> Local =
                MoveTemp(Callback);
            Local(MoveTemp(Result));
        }
    }

private:
    TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> Callback;
};

template <typename ValueType>
void DispatchAdminFailure(
    TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> Callback,
    FOpenPocketBaseError Error)
{
    if (!Callback)
    {
        return;
    }
    AsyncTask(
        ENamedThreads::GameThread,
        [Callback = MoveTemp(Callback), Error = MoveTemp(Error)]() mutable
        {
            Callback(TOpenPocketBaseResult<ValueType>::Failure(MoveTemp(Error)));
        });
}

bool IsSafeAdminSegment(const FString& Value, const int32 MaxLength = 255)
{
    if (Value.IsEmpty() || Value.Len() > MaxLength || Value == TEXT(".") ||
        Value == TEXT(".."))
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (Character == TEXT('/') || Character == TEXT('\\') || Character == TEXT('%') ||
            Character == TEXT('?') || Character == TEXT('#') || FChar::IsControl(Character))
        {
            return false;
        }
    }
    return true;
}

bool IsBoundedAdminText(const FString& Value, const int32 MaxLength)
{
    if (Value.Len() > MaxLength)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (FChar::IsControl(Character))
        {
            return false;
        }
    }
    return true;
}

bool IsSafeBackupKey(const FString& Value)
{
    if (Value.Len() < 5 || Value.Len() > 150 || !Value.EndsWith(TEXT(".zip")))
    {
        return false;
    }
    for (const TCHAR Character : Value.LeftChop(4))
    {
        if (!FChar::IsLower(Character) && !FChar::IsDigit(Character) &&
            Character != TEXT('_') && Character != TEXT('-'))
        {
            return false;
        }
    }
    return true;
}

bool ValidatePolicy(
    const FOpenPocketBaseAdminPolicy& Policy,
    FOpenPocketBaseError& OutError)
{
    if (!Policy.bEnablePrivilegedRequests || Policy.MaxPageSize < 1 ||
        Policy.MaxPageSize > 500 || Policy.MaxRequestBytes < 1024 ||
        Policy.MaxRequestBytes > 64LL * 1024 * 1024 ||
        Policy.MaxResponseBytes < 1024 || Policy.MaxResponseBytes > 64LL * 1024 * 1024 ||
        Policy.MaxBackupBytes < 1024 || Policy.MaxBackupBytes > 64LL * 1024 * 1024 ||
        Policy.MaxSqlRows < 1 || Policy.MaxSqlRows > 1000)
    {
        OutError = MakeAdminError(
            EOpenPocketBaseErrorKind::Unsupported,
            TEXT("Privileged requests require an explicit bounded runtime policy."));
        return false;
    }
#if UE_BUILD_SHIPPING && !OPENPOCKETBASESDK_ADMIN_SHIPPING_ENABLED
    OutError = MakeAdminError(
        EOpenPocketBaseErrorKind::Unsupported,
        TEXT("The privileged module is not enabled for Shipping builds."));
    return false;
#else
#if UE_BUILD_SHIPPING
    if (!Policy.bAllowInShipping)
    {
        OutError = MakeAdminError(
            EOpenPocketBaseErrorKind::Unsupported,
            TEXT("Privileged requests are disabled by the Shipping runtime policy."));
        return false;
    }
#endif
    OutError = FOpenPocketBaseError();
    return true;
#endif
}

FJsonObjectWrapper WrapAdminObject(const TSharedRef<FJsonObject>& Object)
{
    FJsonObjectWrapper Wrapper;
    Wrapper.JsonObject = Object;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Wrapper.JsonString);
    FJsonSerializer::Serialize(Object, Writer);
    return Wrapper;
}

bool IsSecretSettingName(const FString& Name)
{
    const FString Lower = Name.ToLower();
    return Lower.Contains(TEXT("password")) || Lower.Contains(TEXT("secret")) ||
        Lower.Contains(TEXT("token")) || Lower.Contains(TEXT("privatekey")) ||
        Lower.Contains(TEXT("accesskey"));
}

TSharedPtr<FJsonValue> SanitizeSettingValue(
    const FString& Name,
    const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return MakeShared<FJsonValueNull>();
    }
    if (IsSecretSettingName(Name) && Value->Type != EJson::Object &&
        Value->Type != EJson::Array && Value->Type != EJson::Null)
    {
        return MakeShared<FJsonValueString>(TEXT("[REDACTED]"));
    }
    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Source = Value->AsObject();
        const TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Source->Values)
        {
            Target->SetField(Field.Key, SanitizeSettingValue(Field.Key, Field.Value));
        }
        return MakeShared<FJsonValueObject>(Target);
    }
    if (Value->Type == EJson::Array)
    {
        TArray<TSharedPtr<FJsonValue>> Target;
        for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
        {
            Target.Add(SanitizeSettingValue(Name, Item));
        }
        return MakeShared<FJsonValueArray>(MoveTemp(Target));
    }
    return Value;
}

FOpenPocketBaseAdminDocument SanitizeSettingsDocument(
    const FJsonObjectWrapper& Source)
{
    FOpenPocketBaseAdminDocument Document;
    const TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Source.JsonObject->Values)
    {
        Target->SetField(Field.Key, SanitizeSettingValue(Field.Key, Field.Value));
    }
    Document.Data = WrapAdminObject(Target);
    return Document;
}

FOpenPocketBaseRequestOptions BoundOptions(
    FOpenPocketBaseRequestOptions Options,
    const int64 MaxResponseBytes)
{
    Options.MaxResponseBytes = FMath::Min(Options.MaxResponseBytes, MaxResponseBytes);
    return Options;
}

bool TryMakeListQuery(
    const FOpenPocketBaseAdminListOptions& Options,
    const FOpenPocketBaseAdminPolicy& Policy,
    TMap<FString, FString>& OutQuery,
    FOpenPocketBaseError& OutError)
{
    if (Options.Page < 1 || Options.PerPage < 1 || Options.PerPage > Policy.MaxPageSize ||
        !IsBoundedAdminText(Options.Filter, 8192) || Options.Sort.Num() > 32 ||
        Options.Fields.Num() > 64)
    {
        OutError = MakeAdminError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Privileged list options exceed their configured bounds."));
        return false;
    }
    for (const FString& Value : Options.Sort)
    {
        if (!IsBoundedAdminText(Value, 255) || Value.IsEmpty())
        {
            OutError = MakeAdminError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A privileged sort term is invalid."));
            return false;
        }
    }
    for (const FString& Value : Options.Fields)
    {
        if (!IsBoundedAdminText(Value, 255) || Value.IsEmpty())
        {
            OutError = MakeAdminError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                TEXT("A privileged field projection is invalid."));
            return false;
        }
    }
    OutQuery.Add(TEXT("page"), LexToString(Options.Page));
    OutQuery.Add(TEXT("perPage"), LexToString(Options.PerPage));
    if (!Options.Filter.IsEmpty()) OutQuery.Add(TEXT("filter"), Options.Filter);
    if (!Options.Sort.IsEmpty()) OutQuery.Add(TEXT("sort"), FString::Join(Options.Sort, TEXT(",")));
    if (!Options.Fields.IsEmpty())
    {
        OutQuery.Add(TEXT("fields"), FString::Join(Options.Fields, TEXT(",")));
    }
    OutError = FOpenPocketBaseError();
    return true;
}

bool TryParseAdminDocument(
    const FOpenPocketBaseCustomRouteResponse& Response,
    FOpenPocketBaseAdminDocument& OutDocument)
{
    if (!Response.bHasJson ||
        Response.JsonRootType != EOpenPocketBaseJsonRootType::Object ||
        !Response.JsonBody.JsonObject.IsValid())
    {
        return false;
    }
    OutDocument.Data = Response.JsonBody;
    return true;
}

bool TryParseAdminPage(
    const FOpenPocketBaseCustomRouteResponse& Response,
    const int32 MaxItems,
    FOpenPocketBaseAdminPage& OutPage)
{
    const TSharedPtr<FJsonObject> Root = Response.JsonBody.JsonObject;
    double Page = 0;
    double PerPage = 0;
    double TotalItems = 0;
    double TotalPages = 0;
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Response.bHasJson || Response.JsonRootType != EOpenPocketBaseJsonRootType::Object ||
        !Root.IsValid() || !Root->TryGetNumberField(TEXT("page"), Page) ||
        !Root->TryGetNumberField(TEXT("perPage"), PerPage) ||
        !Root->TryGetNumberField(TEXT("totalItems"), TotalItems) ||
        !Root->TryGetNumberField(TEXT("totalPages"), TotalPages) ||
        !Root->TryGetArrayField(TEXT("items"), Items) || Items == nullptr ||
        Items->Num() > MaxItems || Page < 1 || PerPage < 1 || TotalItems < 0 ||
        TotalPages < 0)
    {
        return false;
    }
    OutPage.Page = static_cast<int32>(Page);
    OutPage.PerPage = static_cast<int32>(PerPage);
    OutPage.TotalItems = static_cast<int64>(TotalItems);
    OutPage.TotalPages = static_cast<int32>(TotalPages);
    OutPage.Items.Reserve(Items->Num());
    for (const TSharedPtr<FJsonValue>& Item : *Items)
    {
        if (!Item.IsValid() || Item->Type != EJson::Object || !Item->AsObject().IsValid())
        {
            return false;
        }
        OutPage.Items.Add(WrapAdminObject(Item->AsObject().ToSharedRef()));
    }
    return true;
}

bool TryParseDocumentList(
    const FOpenPocketBaseCustomRouteResponse& Response,
    const int32 MaxItems,
    FOpenPocketBaseAdminDocumentList& OutList)
{
    const TSharedPtr<FJsonValue>& Root = Response.GetParsedJson();
    if (!Response.bHasJson || Response.JsonRootType != EOpenPocketBaseJsonRootType::Array ||
        !Root.IsValid() || Root->AsArray().Num() > MaxItems)
    {
        return false;
    }
    OutList.Items.Reserve(Root->AsArray().Num());
    for (const TSharedPtr<FJsonValue>& Item : Root->AsArray())
    {
        if (!Item.IsValid() || Item->Type != EJson::Object || !Item->AsObject().IsValid())
        {
            return false;
        }
        OutList.Items.Add(WrapAdminObject(Item->AsObject().ToSharedRef()));
    }
    return true;
}

bool TryParseBackups(
    const FOpenPocketBaseCustomRouteResponse& Response,
    FOpenPocketBaseAdminBackupList& OutList)
{
    FOpenPocketBaseAdminDocumentList Documents;
    if (!TryParseDocumentList(Response, 1000, Documents))
    {
        return false;
    }
    OutList.Items.Reserve(Documents.Items.Num());
    for (const FJsonObjectWrapper& Document : Documents.Items)
    {
        FOpenPocketBaseAdminBackup Backup;
        double Size = -1;
        FString Modified;
        if (!Document.JsonObject->TryGetStringField(TEXT("key"), Backup.Key) ||
            !IsSafeBackupKey(Backup.Key) ||
            !Document.JsonObject->TryGetNumberField(TEXT("size"), Size) || Size < 0 ||
            Size > static_cast<double>(MAX_int64) ||
            !Document.JsonObject->TryGetStringField(TEXT("modified"), Modified) ||
            !OpenPocketBase::Date::TryParse(Modified, Backup.Modified))
        {
            return false;
        }
        Backup.Size = static_cast<int64>(Size);
        OutList.Items.Add(MoveTemp(Backup));
    }
    return true;
}

bool TryParseSql(
    const FOpenPocketBaseCustomRouteResponse& Response,
    const int32 MaxRows,
    FOpenPocketBaseAdminSqlResult& OutResult)
{
    const TSharedPtr<FJsonObject> Root = Response.JsonBody.JsonObject;
    double Execution = -1;
    double Affected = -1;
    const TArray<TSharedPtr<FJsonValue>>* Columns = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!Response.bHasJson || Response.JsonRootType != EOpenPocketBaseJsonRootType::Object ||
        !Root.IsValid() || !Root->TryGetNumberField(TEXT("execTime"), Execution) ||
        !Root->TryGetNumberField(TEXT("affectedRows"), Affected) || Execution < 0 ||
        Affected < 0 || !Root->TryGetArrayField(TEXT("columns"), Columns) ||
        Columns == nullptr || Columns->Num() > 256 ||
        !Root->TryGetArrayField(TEXT("rows"), Rows) || Rows == nullptr ||
        Rows->Num() > MaxRows)
    {
        return false;
    }
    OutResult.ExecutionTimeMilliseconds = static_cast<int64>(Execution);
    OutResult.AffectedRows = static_cast<int64>(Affected);
    OutResult.RowCount = Rows->Num();
    OutResult.Data = Response.JsonBody;
    OutResult.Columns.Reserve(Columns->Num());
    for (const TSharedPtr<FJsonValue>& Value : *Columns)
    {
        const TSharedPtr<FJsonObject> ColumnObject =
            Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
        FOpenPocketBaseAdminSqlColumn Column;
        if (!ColumnObject.IsValid() ||
            !ColumnObject->TryGetStringField(TEXT("name"), Column.Name) ||
            !ColumnObject->TryGetStringField(TEXT("type"), Column.Type) ||
            !ColumnObject->TryGetBoolField(TEXT("nullable"), Column.bNullable) ||
            !IsBoundedAdminText(Column.Name, 255) || !IsBoundedAdminText(Column.Type, 255))
        {
            return false;
        }
        OutResult.Columns.Add(MoveTemp(Column));
    }
    return true;
}

bool TryParseAdminRecord(
    const TSharedRef<FJsonObject>& Object,
    FOpenPocketBaseRecord& OutRecord)
{
    if (!Object->TryGetStringField(TEXT("id"), OutRecord.Id) || OutRecord.Id.IsEmpty())
    {
        return false;
    }
    Object->TryGetStringField(TEXT("collectionId"), OutRecord.CollectionId);
    Object->TryGetStringField(TEXT("collectionName"), OutRecord.CollectionName);
    FString Date;
    if (Object->TryGetStringField(TEXT("created"), Date) &&
        !OpenPocketBase::Date::TryParse(Date, OutRecord.Created))
    {
        return false;
    }
    if (Object->TryGetStringField(TEXT("updated"), Date) &&
        !OpenPocketBase::Date::TryParse(Date, OutRecord.Updated))
    {
        return false;
    }
    OutRecord.Data = WrapAdminObject(Object);
    return true;
}

bool IsConservativeReadOnlySql(const FString& Query)
{
    FString Trimmed = Query;
    Trimmed.TrimStartAndEndInline();
    const FString Upper = Trimmed.ToUpper();
    if (!Upper.StartsWith(TEXT("SELECT")) ||
        (Upper.Len() > 6 && !FChar::IsWhitespace(Upper[6])))
    {
        return false;
    }
    const int32 Semicolon = Trimmed.Find(TEXT(";"));
    return Semicolon == INDEX_NONE || Semicolon == Trimmed.Len() - 1;
}
}

class FOpenPocketBaseAdminRequestState final
{
public:
    explicit FOpenPocketBaseAdminRequestState(TUniqueFunction<void()> InOnCancelled)
        : OnCancelled(MoveTemp(InOnCancelled))
    {
    }

    void Attach(FOpenPocketBaseRequestHandle InHandle)
    {
        bool bCancelNow = false;
        {
            FScopeLock Lock(&Mutex);
            if (bActive)
            {
                Child = MoveTemp(InHandle);
            }
            else
            {
                bCancelNow = true;
            }
        }
        if (bCancelNow)
        {
            InHandle.Cancel();
        }
    }

    bool TryComplete(TUniqueFunction<void()> Action)
    {
        {
            FScopeLock Lock(&Mutex);
            if (!bActive)
            {
                return false;
            }
            bActive = false;
            Child = {};
            OnCancelled = {};
        }
        if (Action)
        {
            Action();
        }
        return true;
    }

    void Cancel()
    {
        FOpenPocketBaseRequestHandle LocalChild;
        TUniqueFunction<void()> LocalCancelled;
        {
            FScopeLock Lock(&Mutex);
            if (!bActive)
            {
                return;
            }
            bActive = false;
            LocalChild = MoveTemp(Child);
            LocalCancelled = MoveTemp(OnCancelled);
        }
        LocalChild.Cancel();
        if (LocalCancelled)
        {
            AsyncTask(ENamedThreads::GameThread, [Cancelled = MoveTemp(LocalCancelled)]() mutable
            {
                Cancelled();
            });
        }
    }

    bool IsActive() const
    {
        FScopeLock Lock(&Mutex);
        return bActive;
    }

private:
    mutable FCriticalSection Mutex;
    FOpenPocketBaseRequestHandle Child;
    TUniqueFunction<void()> OnCancelled;
    bool bActive = true;
};

namespace
{
template <typename ValueType, typename ParserType>
FOpenPocketBaseAdminRequestHandle SendAdminRequest(
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe>& CoreClient,
    FOpenPocketBaseCustomRouteRequest Request,
    TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> OnComplete,
    ParserType Parser)
{
    const TSharedRef<TAdminCompletion<ValueType>, ESPMode::ThreadSafe> Completion =
        MakeShared<TAdminCompletion<ValueType>, ESPMode::ThreadSafe>(MoveTemp(OnComplete));
    const TSharedRef<FOpenPocketBaseAdminRequestState, ESPMode::ThreadSafe> State =
        MakeShared<FOpenPocketBaseAdminRequestState, ESPMode::ThreadSafe>(
            [Completion]()
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<ValueType>::Failure(MakeAdminCancelledError()));
            });
    FOpenPocketBaseRequestHandle Child = CoreClient->SendCustomRoute(
        MoveTemp(Request),
        [State, Completion, Parser = MoveTemp(Parser)](
            TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>&& Response) mutable
        {
            TOpenPocketBaseResult<ValueType> Result = Response.IsSuccess()
                ? Parser(Response.GetValue())
                : TOpenPocketBaseResult<ValueType>::Failure(Response.GetError());
            State->TryComplete(
                [Completion, Result = MoveTemp(Result)]() mutable
                {
                    Completion->Invoke(MoveTemp(Result));
                });
        });
    State->Attach(MoveTemp(Child));
    return FOpenPocketBaseAdminRequestHandle(State);
}

FOpenPocketBaseCustomRouteRequest MakeAdminRequest(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    FOpenPocketBaseRequestOptions Options,
    const FOpenPocketBaseAdminPolicy& Policy)
{
    FOpenPocketBaseCustomRouteRequest Request;
    Request.Method = Method;
    Request.Path = MoveTemp(Path);
    Request.bUseAuth = true;
    Request.MaxRequestBytes = Policy.MaxRequestBytes;
    Request.Options = BoundOptions(MoveTemp(Options), Policy.MaxResponseBytes);
    return Request;
}

FOpenPocketBaseCustomRouteRequest MakeAdminJsonRequest(
    const EOpenPocketBaseCustomRouteMethod Method,
    FString Path,
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseRequestOptions Options,
    const FOpenPocketBaseAdminPolicy& Policy)
{
    FOpenPocketBaseCustomRouteRequest Request = MakeAdminRequest(
        Method, MoveTemp(Path), MoveTemp(Options), Policy);
    Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Json;
    Request.JsonBody = MoveTemp(Body.Data);
    return Request;
}

template <typename ValueType>
FOpenPocketBaseAdminRequestHandle FailAdminRequest(
    TUniqueFunction<void(TOpenPocketBaseResult<ValueType>&&)> OnComplete,
    const TCHAR* Message,
    const EOpenPocketBaseErrorKind Kind = EOpenPocketBaseErrorKind::InvalidArgument)
{
    DispatchAdminFailure<ValueType>(
        MoveTemp(OnComplete),
        MakeAdminError(Kind, Message));
    return {};
}
}

FOpenPocketBaseAdminRequestHandle::FOpenPocketBaseAdminRequestHandle(
    TSharedPtr<FOpenPocketBaseAdminRequestState, ESPMode::ThreadSafe> InState)
    : State(MoveTemp(InState))
{
}

void FOpenPocketBaseAdminRequestHandle::Cancel() const
{
    if (State.IsValid())
    {
        State->Cancel();
    }
}

bool FOpenPocketBaseAdminRequestHandle::IsActive() const
{
    return State.IsValid() && State->IsActive();
}

FOpenPocketBaseAdminClient::FOpenPocketBaseAdminClient(
    FOpenPocketBaseClientConfig InCoreConfig,
    FOpenPocketBaseAdminPolicy InPolicy,
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> InCoreClient,
    TSharedPtr<IOpenPocketBaseTransport, ESPMode::ThreadSafe> InInjectedTransport)
    : CoreConfig(MoveTemp(InCoreConfig))
    , Policy(MoveTemp(InPolicy))
    , CoreClient(MoveTemp(InCoreClient))
    , InjectedTransport(MoveTemp(InInjectedTransport))
{
}

FOpenPocketBaseAdminClientResult
FOpenPocketBaseAdminClient::Create(
    const FOpenPocketBaseClientConfig& InCoreConfig,
    const FOpenPocketBaseAdminPolicy& InPolicy,
    FOpenPocketBaseClientDependencies Dependencies)
{
    FOpenPocketBaseError Error;
    if (!ValidatePolicy(InPolicy, Error))
    {
        return FOpenPocketBaseAdminClientResult::Failure(MoveTemp(Error));
    }
    FOpenPocketBaseClientConfig CoreConfig = InCoreConfig;
    CoreConfig.SessionPersistence = EOpenPocketBaseSessionPersistence::MemoryOnly;
    CoreConfig.bEnableAssistedOAuth = false;
    const TSharedPtr<IOpenPocketBaseTransport, ESPMode::ThreadSafe> InjectedTransport =
        Dependencies.Transport;
    FOpenPocketBaseClientResult CoreResult =
        FOpenPocketBaseClient::Create(CoreConfig, MoveTemp(Dependencies));
    if (!CoreResult.IsSuccess())
    {
        return FOpenPocketBaseAdminClientResult::Failure(CoreResult.GetError());
    }
    TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> Core =
        CoreResult.TakeValue();
    TSharedPtr<FOpenPocketBaseAdminClient, ESPMode::ThreadSafe> Client =
        MakeShareable(new FOpenPocketBaseAdminClient(
            MoveTemp(CoreConfig), InPolicy, MoveTemp(Core), InjectedTransport));
    return FOpenPocketBaseAdminClientResult::Success(Client.ToSharedRef());
}

bool FOpenPocketBaseAdminClient::IsAuthenticated() const
{
    if (!CoreClient.IsValid() || CoreClient->IsShutdown())
    {
        return false;
    }
    FOpenPocketBaseSessionSnapshot Session;
    return CoreClient->GetCurrentSession(Session) &&
        Session.AuthCollection == TEXT("_superusers");
}

TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe>
FOpenPocketBaseAdminClient::GetCoreClient() const
{
    return CoreClient;
}

void FOpenPocketBaseAdminClient::Logout()
{
    if (CoreClient.IsValid()) CoreClient->Logout();
}

void FOpenPocketBaseAdminClient::Shutdown()
{
    if (CoreClient.IsValid()) CoreClient->Shutdown();
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::AuthenticateSuperuser(
    FString Email,
    FString Password,
    FOpenPocketBaseAdminIdentityCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!CoreClient.IsValid() || CoreClient->IsShutdown() || Email.IsEmpty() ||
        Email.Len() > 320 || Password.IsEmpty() || Password.Len() > 1024)
    {
        return FailAdminRequest<FOpenPocketBaseAdminIdentity>(
            MoveTemp(OnComplete),
            TEXT("A ready privileged client and bounded credentials are required."));
    }
    const TSharedRef<TAdminCompletion<FOpenPocketBaseAdminIdentity>, ESPMode::ThreadSafe>
        Completion = MakeShared<TAdminCompletion<FOpenPocketBaseAdminIdentity>, ESPMode::ThreadSafe>(
            MoveTemp(OnComplete));
    const TSharedRef<FOpenPocketBaseAdminRequestState, ESPMode::ThreadSafe> State =
        MakeShared<FOpenPocketBaseAdminRequestState, ESPMode::ThreadSafe>(
            [Completion]()
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseAdminIdentity>::Failure(
                        MakeAdminCancelledError()));
            });
    FOpenPocketBaseRequestHandle Child = CoreClient->Collection(TEXT("_superusers"))
        .AuthWithPassword(
            MoveTemp(Email),
            MoveTemp(Password),
            [State, Completion](TOpenPocketBaseResult<FOpenPocketBaseAuthAttempt>&& Result)
            {
                TOpenPocketBaseResult<FOpenPocketBaseAdminIdentity> Identity = [&Result]()
                {
                    if (!Result.IsSuccess())
                    {
                        return TOpenPocketBaseResult<FOpenPocketBaseAdminIdentity>::Failure(
                            SanitizeSensitiveError(
                                Result.GetError(),
                                TEXT("Superuser authentication failed.")));
                    }
                    if (Result.GetValue().Status !=
                            EOpenPocketBaseAuthAttemptStatus::Authenticated ||
                        Result.GetValue().Authentication.Record.CollectionName !=
                            TEXT("_superusers"))
                    {
                        return TOpenPocketBaseResult<FOpenPocketBaseAdminIdentity>::Failure(
                            MakeAdminError(
                                EOpenPocketBaseErrorKind::Authentication,
                                TEXT("PocketBase did not return a superuser session.")));
                    }
                    FOpenPocketBaseAdminIdentity Value;
                    Value.Id = Result.GetValue().Authentication.Record.Id;
                    if (Result.GetValue().Authentication.Record.Data.JsonObject.IsValid())
                    {
                        Result.GetValue().Authentication.Record.Data.JsonObject->TryGetStringField(
                            TEXT("email"), Value.Email);
                    }
                    return TOpenPocketBaseResult<FOpenPocketBaseAdminIdentity>::Success(
                        MoveTemp(Value));
                }();
                State->TryComplete(
                    [Completion, Identity = MoveTemp(Identity)]() mutable
                    {
                        Completion->Invoke(MoveTemp(Identity));
                    });
            },
            MoveTemp(Options));
    State->Attach(MoveTemp(Child));
    return FOpenPocketBaseAdminRequestHandle(State);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::ListCollections(
    FOpenPocketBaseAdminListOptions Options,
    FOpenPocketBaseAdminPageCallback OnComplete)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminPage>(
            MoveTemp(OnComplete), TEXT("A superuser session is required."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    FOpenPocketBaseCustomRouteRequest Request = MakeAdminRequest(
        EOpenPocketBaseCustomRouteMethod::Get,
        TEXT("/api/collections"),
        Options.RequestOptions,
        Policy);
    FOpenPocketBaseError Error;
    if (!TryMakeListQuery(Options, Policy, Request.Query, Error))
    {
        DispatchAdminFailure<FOpenPocketBaseAdminPage>(MoveTemp(OnComplete), MoveTemp(Error));
        return {};
    }
    const int32 MaxItems = Policy.MaxPageSize;
    return SendAdminRequest<FOpenPocketBaseAdminPage>(
        CoreClient,
        MoveTemp(Request),
        MoveTemp(OnComplete),
        [MaxItems](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminPage Page;
            return TryParseAdminPage(Response, MaxItems, Page)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminPage>::Success(MoveTemp(Page))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminPage>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid collection page.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::GetCollection(
    FString Collection,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !IsSafeAdminSegment(Collection))
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("A superuser session and valid collection are required."));
    }
    return SendAdminRequest<FOpenPocketBaseAdminDocument>(
        CoreClient,
        MakeAdminRequest(EOpenPocketBaseCustomRouteMethod::Get,
            TEXT("/api/collections/") + Collection, MoveTemp(Options), Policy),
        MoveTemp(OnComplete),
        [](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminDocument Document;
            return TryParseAdminDocument(Response, Document)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Success(MoveTemp(Document))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid collection.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::CreateCollection(
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("A superuser session and collection document are required."));
    }
    return SendAdminRequest<FOpenPocketBaseAdminDocument>(
        CoreClient,
        MakeAdminJsonRequest(EOpenPocketBaseCustomRouteMethod::Post,
            TEXT("/api/collections"), MoveTemp(Body), MoveTemp(Options), Policy),
        MoveTemp(OnComplete),
        [](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminDocument Document;
            return TryParseAdminDocument(Response, Document)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Success(MoveTemp(Document))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid created collection.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::UpdateCollection(
    FString Collection,
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !IsSafeAdminSegment(Collection) ||
        !Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("A superuser session, collection, and document are required."));
    }
    return SendAdminRequest<FOpenPocketBaseAdminDocument>(
        CoreClient,
        MakeAdminJsonRequest(EOpenPocketBaseCustomRouteMethod::Patch,
            TEXT("/api/collections/") + Collection,
            MoveTemp(Body), MoveTemp(Options), Policy),
        MoveTemp(OnComplete),
        [](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminDocument Document;
            return TryParseAdminDocument(Response, Document)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Success(MoveTemp(Document))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid updated collection.")));
        });
}

namespace
{
TOpenPocketBaseResult<bool> ParseAdminEmpty(const FOpenPocketBaseCustomRouteResponse& Response)
{
    return TOpenPocketBaseResult<bool>::Success(true);
}
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::DeleteCollection(
    FString Collection,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !IsSafeAdminSegment(Collection))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("A superuser session and valid collection are required."));
    }
    return SendAdminRequest<bool>(CoreClient,
        MakeAdminRequest(EOpenPocketBaseCustomRouteMethod::Delete,
            TEXT("/api/collections/") + Collection, MoveTemp(Options), Policy),
        MoveTemp(OnComplete), ParseAdminEmpty);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::ImportCollections(
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    const TArray<TSharedPtr<FJsonValue>>* Collections = nullptr;
    bool bDeleteMissing = false;
    if (!IsAuthenticated() || !Body.Data.JsonObject.IsValid() ||
        !Body.Data.JsonObject->TryGetArrayField(TEXT("collections"), Collections) ||
        Collections == nullptr || Collections->IsEmpty() || Collections->Num() > 500 ||
        (Body.Data.JsonObject->TryGetBoolField(TEXT("deleteMissing"), bDeleteMissing) &&
            bDeleteMissing && !Policy.bAllowDestructiveCollectionImport))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Collection import is invalid or disabled by policy."));
    }
    return SendAdminRequest<bool>(CoreClient,
        MakeAdminJsonRequest(EOpenPocketBaseCustomRouteMethod::Put,
            TEXT("/api/collections/import"), MoveTemp(Body), MoveTemp(Options), Policy),
        MoveTemp(OnComplete), ParseAdminEmpty);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::GetSettings(
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("A superuser session is required."), EOpenPocketBaseErrorKind::Authentication);
    }
    return SendAdminRequest<FOpenPocketBaseAdminDocument>(CoreClient,
        MakeAdminRequest(EOpenPocketBaseCustomRouteMethod::Get,
            TEXT("/api/settings"), MoveTemp(Options), Policy),
        MoveTemp(OnComplete),
        [](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminDocument Document;
            return TryParseAdminDocument(Response, Document)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Success(
                    SanitizeSettingsDocument(Document.Data))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned invalid redacted settings.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::UpdateSettings(
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("A superuser session and settings document are required."));
    }
    return SendAdminRequest<FOpenPocketBaseAdminDocument>(CoreClient,
        MakeAdminJsonRequest(EOpenPocketBaseCustomRouteMethod::Patch,
            TEXT("/api/settings"), MoveTemp(Body), MoveTemp(Options), Policy),
        MoveTemp(OnComplete),
        [](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminDocument Document;
            return TryParseAdminDocument(Response, Document)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Success(
                    SanitizeSettingsDocument(Document.Data))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned invalid redacted settings.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::TestS3(
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("A superuser session and S3 test document are required."));
    }
    return SendAdminRequest<bool>(CoreClient,
        MakeAdminJsonRequest(EOpenPocketBaseCustomRouteMethod::Post,
            TEXT("/api/settings/test/s3"), MoveTemp(Body), MoveTemp(Options), Policy),
        MoveTemp(OnComplete), ParseAdminEmpty);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::TestEmail(
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("A superuser session and email test document are required."));
    }
    return SendAdminRequest<bool>(CoreClient,
        MakeAdminJsonRequest(EOpenPocketBaseCustomRouteMethod::Post,
            TEXT("/api/settings/test/email"), MoveTemp(Body), MoveTemp(Options), Policy),
        MoveTemp(OnComplete), ParseAdminEmpty);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::ListLogs(
    FOpenPocketBaseAdminListOptions Options,
    FOpenPocketBaseAdminPageCallback OnComplete)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminPage>(MoveTemp(OnComplete),
            TEXT("A superuser session is required."), EOpenPocketBaseErrorKind::Authentication);
    }
    FOpenPocketBaseCustomRouteRequest Request = MakeAdminRequest(
        EOpenPocketBaseCustomRouteMethod::Get, TEXT("/api/logs"),
        Options.RequestOptions, Policy);
    FOpenPocketBaseError Error;
    if (!TryMakeListQuery(Options, Policy, Request.Query, Error))
    {
        DispatchAdminFailure<FOpenPocketBaseAdminPage>(MoveTemp(OnComplete), MoveTemp(Error));
        return {};
    }
    const int32 MaxItems = Policy.MaxPageSize;
    return SendAdminRequest<FOpenPocketBaseAdminPage>(CoreClient, MoveTemp(Request),
        MoveTemp(OnComplete),
        [MaxItems](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminPage Page;
            return TryParseAdminPage(Response, MaxItems, Page)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminPage>::Success(MoveTemp(Page))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminPage>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid log page.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::GetLog(
    FString LogId,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !IsSafeAdminSegment(LogId))
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("A superuser session and valid log ID are required."));
    }
    return SendAdminRequest<FOpenPocketBaseAdminDocument>(CoreClient,
        MakeAdminRequest(EOpenPocketBaseCustomRouteMethod::Get,
            TEXT("/api/logs/") + LogId, MoveTemp(Options), Policy),
        MoveTemp(OnComplete),
        [](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminDocument Document;
            return TryParseAdminDocument(Response, Document)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Success(MoveTemp(Document))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminDocument>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid log entry.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::ListBackups(
    FOpenPocketBaseAdminBackupListCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminBackupList>(MoveTemp(OnComplete),
            TEXT("A superuser session is required."), EOpenPocketBaseErrorKind::Authentication);
    }
    return SendAdminRequest<FOpenPocketBaseAdminBackupList>(CoreClient,
        MakeAdminRequest(EOpenPocketBaseCustomRouteMethod::Get,
            TEXT("/api/backups"), MoveTemp(Options), Policy),
        MoveTemp(OnComplete),
        [](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminBackupList List;
            return TryParseBackups(Response, List)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminBackupList>::Success(MoveTemp(List))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminBackupList>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid backup list.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::CreateBackup(
    FString Name,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !IsSafeBackupKey(Name))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("A superuser session and valid unique backup name are required."));
    }
    FOpenPocketBaseAdminDocument Body;
    Body.Data.JsonObject = MakeShared<FJsonObject>();
    Body.Data.JsonObject->SetStringField(TEXT("name"), Name);
    return SendAdminRequest<bool>(CoreClient,
        MakeAdminJsonRequest(EOpenPocketBaseCustomRouteMethod::Post,
            TEXT("/api/backups"), MoveTemp(Body), MoveTemp(Options), Policy),
        MoveTemp(OnComplete), ParseAdminEmpty);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::UploadBackup(
    FOpenPocketBaseFileInput File,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || File.FieldName != TEXT("file") ||
        !IsSafeBackupKey(File.FileName) || File.ContentType != TEXT("application/zip") ||
        File.Modifier != EOpenPocketBaseFieldModifier::Replace)
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("A superuser session and bounded ZIP backup file are required."));
    }
    FOpenPocketBaseCustomRouteRequest Request = MakeAdminRequest(
        EOpenPocketBaseCustomRouteMethod::Post,
        TEXT("/api/backups/upload"), MoveTemp(Options), Policy);
    Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Multipart;
    Request.Files.Add(MoveTemp(File));
    Request.MaxRequestBytes = Policy.MaxBackupBytes;
    Request.UploadLimits.MaxTotalBodyBytes = Policy.MaxBackupBytes;
    Request.UploadLimits.MaxInlineFileBytes = FMath::Min(
        Request.UploadLimits.MaxInlineFileBytes, Policy.MaxBackupBytes);
    Request.UploadLimits.MaxSourceFileBytes = FMath::Min(
        Request.UploadLimits.MaxSourceFileBytes, Policy.MaxBackupBytes);
    return SendAdminRequest<bool>(CoreClient, MoveTemp(Request),
        MoveTemp(OnComplete), ParseAdminEmpty);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::DownloadBackup(
    FString Key,
    FOpenPocketBaseAdminBackupDownloadCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !IsSafeBackupKey(Key))
    {
        return FailAdminRequest<FOpenPocketBaseAdminBackupDownload>(MoveTemp(OnComplete),
            TEXT("A superuser session and valid backup key are required."));
    }
    const TSharedRef<TAdminCompletion<FOpenPocketBaseAdminBackupDownload>, ESPMode::ThreadSafe>
        Completion =
            MakeShared<TAdminCompletion<FOpenPocketBaseAdminBackupDownload>, ESPMode::ThreadSafe>(
                MoveTemp(OnComplete));
    const TSharedRef<FOpenPocketBaseAdminRequestState, ESPMode::ThreadSafe> State =
        MakeShared<FOpenPocketBaseAdminRequestState, ESPMode::ThreadSafe>(
            [Completion]()
            {
                Completion->Invoke(
                    TOpenPocketBaseResult<FOpenPocketBaseAdminBackupDownload>::Failure(
                        MakeAdminCancelledError()));
            });
    const TSharedPtr<FOpenPocketBaseClient, ESPMode::ThreadSafe> PinnedCore = CoreClient;
    const FOpenPocketBaseAdminPolicy CapturedPolicy = Policy;
    FOpenPocketBaseCustomRouteRequest TokenRequest = MakeAdminRequest(
        EOpenPocketBaseCustomRouteMethod::Post,
        TEXT("/api/files/token"), Options, Policy);
    FOpenPocketBaseRequestHandle TokenHandle = CoreClient->SendCustomRoute(
        MoveTemp(TokenRequest),
        [State, Completion, PinnedCore, CapturedPolicy, Key,
            Options = MoveTemp(Options)](
            TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>&& TokenResult) mutable
        {
            FString Token;
            if (!TokenResult.IsSuccess() || !TokenResult.GetValue().JsonBody.JsonObject.IsValid() ||
                !TokenResult.GetValue().JsonBody.JsonObject->TryGetStringField(
                    TEXT("token"), Token) || Token.IsEmpty() || Token.Len() > 4096)
            {
                FOpenPocketBaseError Error = TokenResult.IsSuccess()
                    ? MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid backup file token."))
                    : SanitizeSensitiveError(TokenResult.GetError(),
                        TEXT("The backup file token request failed."));
                State->TryComplete([Completion, Error = MoveTemp(Error)]() mutable
                {
                    Completion->Invoke(
                        TOpenPocketBaseResult<FOpenPocketBaseAdminBackupDownload>::Failure(
                            MoveTemp(Error)));
                });
                return;
            }
            FOpenPocketBaseCustomRouteRequest Download = MakeAdminRequest(
                EOpenPocketBaseCustomRouteMethod::Get,
                TEXT("/api/backups/") + Key,
                BoundOptions(MoveTemp(Options), CapturedPolicy.MaxBackupBytes),
                CapturedPolicy);
            Download.bUseAuth = false;
            Download.Options.MaxResponseBytes = CapturedPolicy.MaxBackupBytes;
            Download.Query.Add(TEXT("token"), MoveTemp(Token));
            FOpenPocketBaseRequestHandle DownloadHandle = PinnedCore->SendCustomRoute(
                MoveTemp(Download),
                [State, Completion](
                    TOpenPocketBaseResult<FOpenPocketBaseCustomRouteResponse>&& Result) mutable
                {
                    TOpenPocketBaseResult<FOpenPocketBaseAdminBackupDownload> DownloadResult =
                        [&Result]()
                    {
                        if (!Result.IsSuccess())
                        {
                            return TOpenPocketBaseResult<FOpenPocketBaseAdminBackupDownload>::Failure(
                                SanitizeSensitiveError(Result.GetError(),
                                    TEXT("The backup download failed.")));
                        }
                        FOpenPocketBaseAdminBackupDownload Value;
                        Value.Bytes = MoveTemp(Result.GetValue().Body);
                        Value.ContentType = MoveTemp(Result.GetValue().ContentType);
                        return TOpenPocketBaseResult<FOpenPocketBaseAdminBackupDownload>::Success(
                            MoveTemp(Value));
                    }();
                    State->TryComplete(
                        [Completion, DownloadResult = MoveTemp(DownloadResult)]() mutable
                        {
                            Completion->Invoke(MoveTemp(DownloadResult));
                        });
                });
            State->Attach(MoveTemp(DownloadHandle));
        });
    State->Attach(MoveTemp(TokenHandle));
    return FOpenPocketBaseAdminRequestHandle(State);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::RestoreBackup(
    FString Key,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !Policy.bAllowBackupRestore || !IsSafeBackupKey(Key))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Backup restore is invalid or disabled by policy."));
    }
    return SendAdminRequest<bool>(CoreClient,
        MakeAdminRequest(EOpenPocketBaseCustomRouteMethod::Post,
            TEXT("/api/backups/") + Key + TEXT("/restore"), MoveTemp(Options), Policy),
        MoveTemp(OnComplete), ParseAdminEmpty);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::DeleteBackup(
    FString Key,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !IsSafeBackupKey(Key))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("A superuser session and valid backup key are required."));
    }
    return SendAdminRequest<bool>(CoreClient,
        MakeAdminRequest(EOpenPocketBaseCustomRouteMethod::Delete,
            TEXT("/api/backups/") + Key, MoveTemp(Options), Policy),
        MoveTemp(OnComplete), ParseAdminEmpty);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::ListCrons(
    FOpenPocketBaseAdminDocumentListCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocumentList>(MoveTemp(OnComplete),
            TEXT("A superuser session is required."), EOpenPocketBaseErrorKind::Authentication);
    }
    return SendAdminRequest<FOpenPocketBaseAdminDocumentList>(CoreClient,
        MakeAdminRequest(EOpenPocketBaseCustomRouteMethod::Get,
            TEXT("/api/crons"), MoveTemp(Options), Policy),
        MoveTemp(OnComplete),
        [](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminDocumentList List;
            return TryParseDocumentList(Response, 1000, List)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminDocumentList>::Success(MoveTemp(List))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminDocumentList>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid cron list.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::RunCron(
    FString CronId,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !IsSafeAdminSegment(CronId))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("A superuser session and valid cron ID are required."));
    }
    return SendAdminRequest<bool>(CoreClient,
        MakeAdminRequest(EOpenPocketBaseCustomRouteMethod::Post,
            TEXT("/api/crons/") + CronId, MoveTemp(Options), Policy),
        MoveTemp(OnComplete), ParseAdminEmpty);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::RunSql(
    FString Query,
    FOpenPocketBaseAdminSqlCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    FString Trimmed = Query;
    Trimmed.TrimStartAndEndInline();
    if (!IsAuthenticated() || Trimmed.IsEmpty() || Trimmed.Len() > 5000 ||
        (!Policy.bAllowSqlWrites && !IsConservativeReadOnlySql(Trimmed)))
    {
        return FailAdminRequest<FOpenPocketBaseAdminSqlResult>(MoveTemp(OnComplete),
            TEXT("The raw SQL request is invalid or disabled by policy."));
    }
    FOpenPocketBaseAdminDocument Body;
    Body.Data.JsonObject = MakeShared<FJsonObject>();
    Body.Data.JsonObject->SetStringField(TEXT("query"), MoveTemp(Trimmed));
    const int32 MaxRows = Policy.MaxSqlRows;
    FOpenPocketBaseAdminSqlCallback SanitizedComplete =
        [OnComplete = MoveTemp(OnComplete)](
            TOpenPocketBaseResult<FOpenPocketBaseAdminSqlResult>&& Result) mutable
        {
            if (!OnComplete)
            {
                return;
            }
            if (!Result.IsSuccess())
            {
                OnComplete(TOpenPocketBaseResult<FOpenPocketBaseAdminSqlResult>::Failure(
                    SanitizeSensitiveError(
                        Result.GetError(),
                        TEXT("The SQL request failed without exposing query or result material."))));
                return;
            }
            OnComplete(MoveTemp(Result));
        };
    return SendAdminRequest<FOpenPocketBaseAdminSqlResult>(CoreClient,
        MakeAdminJsonRequest(EOpenPocketBaseCustomRouteMethod::Post,
            TEXT("/api/sql"), MoveTemp(Body), MoveTemp(Options), Policy),
        MoveTemp(SanitizedComplete),
        [MaxRows](const FOpenPocketBaseCustomRouteResponse& Response)
        {
            FOpenPocketBaseAdminSqlResult Result;
            return TryParseSql(Response, MaxRows, Result)
                ? TOpenPocketBaseResult<FOpenPocketBaseAdminSqlResult>::Success(MoveTemp(Result))
                : TOpenPocketBaseResult<FOpenPocketBaseAdminSqlResult>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid bounded SQL result.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::Impersonate(
    FString AuthCollection,
    FString RecordId,
    const int64 DurationSeconds,
    FOpenPocketBaseAdminImpersonationCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated() || !Policy.bAllowImpersonation ||
        !IsSafeAdminSegment(AuthCollection) || !IsSafeAdminSegment(RecordId) ||
        DurationSeconds < 0 || DurationSeconds > 365LL * 24 * 60 * 60)
    {
        return FailAdminRequest<FOpenPocketBaseAdminImpersonationResult>(MoveTemp(OnComplete),
            TEXT("Impersonation is invalid or disabled by policy."));
    }
    FOpenPocketBaseAdminDocument Body;
    Body.Data.JsonObject = MakeShared<FJsonObject>();
    Body.Data.JsonObject->SetNumberField(TEXT("duration"), DurationSeconds);
    const FOpenPocketBaseClientConfig CapturedConfig = CoreConfig;
    const TSharedPtr<IOpenPocketBaseTransport, ESPMode::ThreadSafe> CapturedTransport =
        InjectedTransport;
    return SendAdminRequest<FOpenPocketBaseAdminImpersonationResult>(CoreClient,
        MakeAdminJsonRequest(EOpenPocketBaseCustomRouteMethod::Post,
            TEXT("/api/collections/") + AuthCollection + TEXT("/impersonate/") + RecordId,
            MoveTemp(Body), MoveTemp(Options), Policy),
        MoveTemp(OnComplete),
        [CapturedConfig, CapturedTransport, AuthCollection](
            const FOpenPocketBaseCustomRouteResponse& Response)
        {
            const TSharedPtr<FJsonObject> Root = Response.JsonBody.JsonObject;
            const TSharedPtr<FJsonObject>* RecordObject = nullptr;
            FString Token;
            FOpenPocketBaseRecord Record;
            if (!Root.IsValid() || !Root->TryGetStringField(TEXT("token"), Token) ||
                Token.IsEmpty() || Token.Len() > 8192 ||
                !Root->TryGetObjectField(TEXT("record"), RecordObject) ||
                RecordObject == nullptr || !TryParseAdminRecord(RecordObject->ToSharedRef(), Record))
            {
                return TOpenPocketBaseResult<FOpenPocketBaseAdminImpersonationResult>::Failure(
                    MakeAdminError(EOpenPocketBaseErrorKind::Serialization,
                        TEXT("PocketBase returned an invalid impersonation session.")));
            }
            FOpenPocketBaseClientDependencies Dependencies;
            Dependencies.Transport = CapturedTransport;
            FOpenPocketBaseClientResult ClientResult =
                FOpenPocketBaseClient::CreateEphemeralAuthenticated(
                    CapturedConfig,
                    MoveTemp(Token),
                    AuthCollection,
                    Record,
                    MoveTemp(Dependencies));
            if (!ClientResult.IsSuccess())
            {
                return TOpenPocketBaseResult<FOpenPocketBaseAdminImpersonationResult>::Failure(
                    SanitizeSensitiveError(ClientResult.GetError(),
                        TEXT("The impersonated client could not be created.")));
            }
            FOpenPocketBaseAdminImpersonationResult Result;
            Result.Client = ClientResult.TakeValue();
            Result.Record = MoveTemp(Record);
            return TOpenPocketBaseResult<FOpenPocketBaseAdminImpersonationResult>::Success(
                MoveTemp(Result));
        });
}
