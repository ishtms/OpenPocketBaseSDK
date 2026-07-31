#include "OpenPocketBaseAdminClient.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "OpenPocketBaseAdminQueryLibrary.h"
#include "OpenPocketBaseDate.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FOpenPocketBaseError MakeAdminError(
    const EOpenPocketBaseErrorKind Kind,
    const FString& Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = Kind;
    Error.Message = Message;
    return Error;
}

FOpenPocketBaseError MakeAdminCancelledError()
{
    return MakeAdminError(
        EOpenPocketBaseErrorKind::Cancelled,
        TEXT("The privileged PocketBase request was cancelled by the caller."));
}

FOpenPocketBaseError SanitizeSensitiveError(
    FOpenPocketBaseError Error,
    const TCHAR* Message)
{
    Error.Code.Reset();
    Error.Message = Message;
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
    if (!Policy.bEnablePrivilegedRequests)
    {
        OutError = MakeAdminError(
            EOpenPocketBaseErrorKind::Unsupported,
            TEXT("Privileged Requests is disabled in the admin policy. Enable it explicitly before creating an admin client."));
        return false;
    }
    if (Policy.MaxPageSize < 1 || Policy.MaxPageSize > 500)
    {
        OutError = MakeAdminError(EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(TEXT("Admin policy Max Page Size is %d. Use a value from 1 to 500."), Policy.MaxPageSize));
        return false;
    }
    if (Policy.MaxRequestBytes < 1024 || Policy.MaxRequestBytes > 64LL * 1024 * 1024)
    {
        OutError = MakeAdminError(EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(TEXT("Admin policy Max Request Bytes is %lld. Use a value from 1024 to 67108864 bytes."), Policy.MaxRequestBytes));
        return false;
    }
    if (Policy.MaxResponseBytes < 1024 || Policy.MaxResponseBytes > 64LL * 1024 * 1024)
    {
        OutError = MakeAdminError(EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(TEXT("Admin policy Max Response Bytes is %lld. Use a value from 1024 to 67108864 bytes."), Policy.MaxResponseBytes));
        return false;
    }
    if (Policy.MaxBackupBytes < 1024 || Policy.MaxBackupBytes > 64LL * 1024 * 1024)
    {
        OutError = MakeAdminError(EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(TEXT("Admin policy Max Backup Bytes is %lld. Use a value from 1024 to 67108864 bytes."), Policy.MaxBackupBytes));
        return false;
    }
    if (Policy.MaxSqlRows < 1 || Policy.MaxSqlRows > 1000)
    {
        OutError = MakeAdminError(EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(TEXT("Admin policy Max SQL Rows is %d. Use a value from 1 to 1000."), Policy.MaxSqlRows));
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

bool IsRecordSystemField(const FString& Name)
{
    return Name == TEXT("id") || Name == TEXT("collectionId") ||
        Name == TEXT("collectionName") || Name == TEXT("created") ||
        Name == TEXT("updated") || Name == TEXT("expand");
}

FJsonObjectWrapper WrapAdminRecordData(const TSharedRef<FJsonObject>& Object)
{
    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
    {
        if (!IsRecordSystemField(Field.Key))
        {
            Data->SetField(Field.Key, Field.Value);
        }
    }
    return WrapAdminObject(Data);
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

bool FindRedactedSettingValue(
    const FString& Name,
    const TSharedPtr<FJsonValue>& Value,
    const FString& Path,
    const bool bSecretParent,
    FString& OutPath)
{
    if (!Value.IsValid())
    {
        return false;
    }
    const bool bSecret = bSecretParent || IsSecretSettingName(Name);
    if (bSecret && Value->Type == EJson::String &&
        Value->AsString() == TEXT("[REDACTED]"))
    {
        OutPath = Path;
        return true;
    }
    if (Value->Type == EJson::Object)
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field :
             Value->AsObject()->Values)
        {
            const FString ChildPath = Path.IsEmpty()
                ? Field.Key
                : Path + TEXT(".") + Field.Key;
            if (FindRedactedSettingValue(
                    Field.Key, Field.Value, ChildPath, bSecret, OutPath))
            {
                return true;
            }
        }
    }
    else if (Value->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Items = Value->AsArray();
        for (int32 Index = 0; Index < Items.Num(); ++Index)
        {
            if (FindRedactedSettingValue(
                    Name,
                    Items[Index],
                    FString::Printf(TEXT("%s[%d]"), *Path, Index),
                    bSecret,
                    OutPath))
            {
                return true;
            }
        }
    }
    return false;
}

bool FindRedactedSettingValue(
    const FJsonObjectWrapper& Document,
    FString& OutPath)
{
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field :
         Document.JsonObject->Values)
    {
        if (FindRedactedSettingValue(
                Field.Key, Field.Value, Field.Key, false, OutPath))
        {
            return true;
        }
    }
    return false;
}

FOpenPocketBaseRequestOptions BoundOptions(
    FOpenPocketBaseRequestOptions Options,
    const int64 MaxResponseBytes)
{
    Options.MaxResponseBytes = FMath::Min(Options.MaxResponseBytes, MaxResponseBytes);
    return Options;
}

bool TryMakeListQueryValues(
    const int32 Page,
    const int32 PerPage,
    const FString& Filter,
    const TArray<FString>& Sort,
    const TArray<FString>& Fields,
    const FOpenPocketBaseAdminPolicy& Policy,
    TMap<FString, FString>& OutQuery,
    FOpenPocketBaseError& OutError)
{
    if (Page < 1)
    {
        OutError = MakeAdminError(EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(TEXT("Admin list Page is %d. Use page 1 or greater."), Page));
        return false;
    }
    if (PerPage < 1 || PerPage > Policy.MaxPageSize)
    {
        OutError = MakeAdminError(EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(TEXT("Admin list Per Page is %d. Use a value from 1 to the policy Max Page Size of %d."), PerPage, Policy.MaxPageSize));
        return false;
    }
    if (!IsBoundedAdminText(Filter, 8192))
    {
        OutError = MakeAdminError(EOpenPocketBaseErrorKind::InvalidArgument,
            TEXT("Admin list Filter exceeds 8192 characters or contains a control character."));
        return false;
    }
    if (Sort.Num() > 32)
    {
        OutError = MakeAdminError(EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(TEXT("Admin list Sort contains %d entries. Use at most 32."), Sort.Num()));
        return false;
    }
    if (Fields.Num() > 64)
    {
        OutError = MakeAdminError(EOpenPocketBaseErrorKind::InvalidArgument,
            FString::Printf(TEXT("Admin list Fields contains %d entries. Use at most 64."), Fields.Num()));
        return false;
    }
    for (int32 SortIndex = 0; SortIndex < Sort.Num(); ++SortIndex)
    {
        const FString& Value = Sort[SortIndex];
        if (!IsBoundedAdminText(Value, 255) || Value.IsEmpty())
        {
            OutError = MakeAdminError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(TEXT("Admin list Sort entry %d is empty, exceeds 255 characters, or contains a control character."), SortIndex + 1));
            return false;
        }
    }
    for (int32 FieldIndex = 0; FieldIndex < Fields.Num(); ++FieldIndex)
    {
        const FString& Value = Fields[FieldIndex];
        if (!IsBoundedAdminText(Value, 255) || Value.IsEmpty())
        {
            OutError = MakeAdminError(
                EOpenPocketBaseErrorKind::InvalidArgument,
                FString::Printf(TEXT("Admin list Fields entry %d is empty, exceeds 255 characters, or contains a control character."), FieldIndex + 1));
            return false;
        }
    }
    OutQuery.Add(TEXT("page"), LexToString(Page));
    OutQuery.Add(TEXT("perPage"), LexToString(PerPage));
    if (!Filter.IsEmpty()) OutQuery.Add(TEXT("filter"), Filter);
    if (!Sort.IsEmpty()) OutQuery.Add(TEXT("sort"), FString::Join(Sort, TEXT(",")));
    if (!Fields.IsEmpty())
    {
        OutQuery.Add(TEXT("fields"), FString::Join(Fields, TEXT(",")));
    }
    OutError = FOpenPocketBaseError();
    return true;
}

bool TryMakeListQuery(
    const FOpenPocketBaseAdminCollectionListOptions& Options,
    const FOpenPocketBaseAdminPolicy& Policy,
    TMap<FString, FString>& OutQuery,
    FOpenPocketBaseError& OutError)
{
    if (!Options.Filter.IsValid())
    {
        OutError = MakeAdminError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            Options.Filter.ErrorMessage.IsEmpty()
                ? TEXT("Admin collection Filter is invalid. Rebuild it with the admin collection filter nodes.")
                : FString::Printf(TEXT("Admin collection Filter is invalid. %s"), *Options.Filter.ErrorMessage));
        return false;
    }
    TArray<FString> Sort;
    Sort.Reserve(Options.Sort.Num());
    for (const FOpenPocketBaseAdminCollectionSort& Value : Options.Sort)
    {
        Sort.Add(Value.ToQueryValue());
    }
    TArray<FString> Fields;
    Fields.Reserve(Options.Fields.Num());
    for (const EOpenPocketBaseAdminCollectionProjectionField Value : Options.Fields)
    {
        Fields.Add(OpenPocketBase::AdminQuery::CollectionProjection(Value));
    }
    return TryMakeListQueryValues(
        Options.Page,
        Options.PerPage,
        Options.Filter.Expression,
        Sort,
        Fields,
        Policy,
        OutQuery,
        OutError);
}

bool TryMakeListQuery(
    const FOpenPocketBaseAdminLogListOptions& Options,
    const FOpenPocketBaseAdminPolicy& Policy,
    TMap<FString, FString>& OutQuery,
    FOpenPocketBaseError& OutError)
{
    if (!Options.Filter.IsValid())
    {
        OutError = MakeAdminError(
            EOpenPocketBaseErrorKind::InvalidArgument,
            Options.Filter.ErrorMessage.IsEmpty()
                ? TEXT("Admin log Filter is invalid. Rebuild it with the admin log filter nodes.")
                : FString::Printf(TEXT("Admin log Filter is invalid. %s"), *Options.Filter.ErrorMessage));
        return false;
    }
    TArray<FString> Sort;
    Sort.Reserve(Options.Sort.Num());
    for (const FOpenPocketBaseAdminLogSort& Value : Options.Sort)
    {
        Sort.Add(Value.ToQueryValue());
    }
    TArray<FString> Fields;
    Fields.Reserve(Options.Fields.Num());
    for (const EOpenPocketBaseAdminLogProjectionField Value : Options.Fields)
    {
        Fields.Add(OpenPocketBase::AdminQuery::LogProjection(Value));
    }
    return TryMakeListQueryValues(
        Options.Page,
        Options.PerPage,
        Options.Filter.Expression,
        Sort,
        Fields,
        Policy,
        OutQuery,
        OutError);
}

bool TryMakeListQuery(
    const FOpenPocketBaseDynamicAdminListOptions& Options,
    const FOpenPocketBaseAdminPolicy& Policy,
    TMap<FString, FString>& OutQuery,
    FOpenPocketBaseError& OutError)
{
    return TryMakeListQueryValues(
        Options.Page,
        Options.PerPage,
        Options.DynamicFilter,
        Options.DynamicSort,
        Options.DynamicFields,
        Policy,
        OutQuery,
        OutError);
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
    const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("rows"), *Rows);
    OutResult.Data = WrapAdminObject(Data);
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
    OutRecord = FOpenPocketBaseRecord();
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
    OutRecord.Data = WrapAdminRecordData(Object);
    const TSharedPtr<FJsonObject>* Expanded = nullptr;
    if (Object->TryGetObjectField(TEXT("expand"), Expanded) && Expanded != nullptr)
    {
        OutRecord.Expanded = WrapAdminObject(Expanded->ToSharedRef());
    }
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
    const FString& Message,
    const EOpenPocketBaseErrorKind Kind = EOpenPocketBaseErrorKind::InvalidArgument)
{
    DispatchAdminFailure<ValueType>(
        MoveTemp(OnComplete),
        MakeAdminError(Kind, Message));
    return {};
}
}

FOpenPocketBaseAdminBackupInput FOpenPocketBaseAdminBackupInput::FromPath(
    FString InFilePath,
    FString InFileName)
{
    FOpenPocketBaseAdminBackupInput Input;
    Input.FileName = InFileName.IsEmpty()
        ? FPaths::GetCleanFilename(InFilePath)
        : MoveTemp(InFileName);
    Input.FilePath = MoveTemp(InFilePath);
    Input.bUseFilePath = true;
    return Input;
}

FOpenPocketBaseAdminBackupInput FOpenPocketBaseAdminBackupInput::FromBytes(
    TArray<uint8> InBytes,
    FString InFileName)
{
    FOpenPocketBaseAdminBackupInput Input;
    Input.FileName = MoveTemp(InFileName);
    Input.Bytes = MoveTemp(InBytes);
    return Input;
}

bool FOpenPocketBaseAdminBackupInput::IsValid() const
{
    return !FileName.IsEmpty() && FileName.EndsWith(TEXT(".zip"), ESearchCase::IgnoreCase) &&
        (bUseFilePath ? !FilePath.IsEmpty() : !Bytes.IsEmpty());
}

FOpenPocketBaseFileInput FOpenPocketBaseAdminBackupInput::ToFileInput() &&
{
    return bUseFilePath
        ? FOpenPocketBaseFileInput::DynamicFromPath(
            TEXT("file"),
            MoveTemp(FilePath),
            MoveTemp(FileName),
            TEXT("application/zip"))
        : FOpenPocketBaseFileInput::DynamicFromBytes(
            TEXT("file"),
            MoveTemp(Bytes),
            MoveTemp(FileName),
            TEXT("application/zip"));
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
    if (!CoreClient.IsValid() || CoreClient->IsShutdown())
    {
        return FailAdminRequest<FOpenPocketBaseAdminIdentity>(
            MoveTemp(OnComplete),
            TEXT("The privileged PocketBase client is missing or has already shut down. Create an active admin client before logging in."));
    }
    if (Email.IsEmpty() || Email.Len() > 320 || !IsBoundedAdminText(Email, 320))
    {
        return FailAdminRequest<FOpenPocketBaseAdminIdentity>(
            MoveTemp(OnComplete),
            TEXT("Superuser Email is empty, exceeds 320 characters, or contains a control character."));
    }
    if (Password.IsEmpty() || Password.Len() > 1024 || !IsBoundedAdminText(Password, 1024))
    {
        return FailAdminRequest<FOpenPocketBaseAdminIdentity>(
            MoveTemp(OnComplete),
            TEXT("Superuser Password is empty, exceeds 1024 characters, or contains a control character."));
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
    FOpenPocketBaseRequestHandle Child = CoreClient->DynamicCollection(TEXT("_superusers"))
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
                                TEXT("Superuser authentication failed. Check the email and password, and confirm the account exists in the _superusers collection.")));
                    }
                    if (Result.GetValue().Status !=
                            EOpenPocketBaseAuthAttemptStatus::Authenticated ||
                        Result.GetValue().Authentication.Record.CollectionName !=
                            TEXT("_superusers"))
                    {
                        return TOpenPocketBaseResult<FOpenPocketBaseAdminIdentity>::Failure(
                            MakeAdminError(
                                EOpenPocketBaseErrorKind::Authentication,
                                TEXT("PocketBase accepted the auth request but did not return an authenticated _superusers record. Check the server version and auth collection.")));
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
    FOpenPocketBaseAdminCollectionListOptions Options,
    FOpenPocketBaseAdminPageCallback OnComplete)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminPage>(
            MoveTemp(OnComplete), TEXT("Log in as a PocketBase superuser before listing collections."),
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
                        TEXT("PocketBase collection list response must contain valid page, perPage, totalItems, totalPages, and items fields, and items cannot exceed the admin policy page bound. Check the PocketBase version and any response hook.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::DynamicListCollections(
    FOpenPocketBaseDynamicAdminListOptions Options,
    FOpenPocketBaseAdminPageCallback OnComplete)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminPage>(
            MoveTemp(OnComplete), TEXT("Log in as a PocketBase superuser before listing collections."),
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
                        TEXT("PocketBase collection list response must contain valid page, perPage, totalItems, totalPages, and items fields, and items cannot exceed the admin policy page bound. Check the PocketBase version and any response hook.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::GetCollection(
    FOpenPocketBaseCollectionRef Collection,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseCollectionRef Current;
    if (!Collection.ResolveCurrent(Current))
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(
            MoveTemp(OnComplete),
            TEXT("Collection is missing or stale. Choose a collection from the current imported schema."));
    }
    return DynamicGetCollection(
        MoveTemp(Current.Name), MoveTemp(OnComplete), MoveTemp(Options));
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::DynamicGetCollection(
    FString Collection,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before getting a collection."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!IsSafeAdminSegment(Collection))
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Collection Name is empty, exceeds 255 characters, or contains an unsafe path character."));
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
                        TEXT("PocketBase collection response must be a JSON object. Check the server version and collection endpoint.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::CreateCollection(
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before creating a collection."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Collection Document has no valid JSON object. Build the collection definition before creating it."));
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
                        TEXT("PocketBase create-collection response must contain the created collection as a JSON object.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::UpdateCollection(
    FOpenPocketBaseCollectionRef Collection,
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseCollectionRef Current;
    if (!Collection.ResolveCurrent(Current))
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(
            MoveTemp(OnComplete),
            TEXT("Collection is missing or stale. Choose a collection from the current imported schema."));
    }
    return DynamicUpdateCollection(
        MoveTemp(Current.Name),
        MoveTemp(Body),
        MoveTemp(OnComplete),
        MoveTemp(Options));
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::DynamicUpdateCollection(
    FString Collection,
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before updating a collection."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!IsSafeAdminSegment(Collection))
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Collection Name is empty, exceeds 255 characters, or contains an unsafe path character."));
    }
    if (!Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Collection Document has no valid JSON object. Build the update document before sending it."));
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
                        TEXT("PocketBase update-collection response must contain the updated collection as a JSON object.")));
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
    FOpenPocketBaseCollectionRef Collection,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseCollectionRef Current;
    if (!Collection.ResolveCurrent(Current))
    {
        return FailAdminRequest<bool>(
            MoveTemp(OnComplete),
            TEXT("Collection is missing or stale. Choose a collection from the current imported schema."));
    }
    return DynamicDeleteCollection(
        MoveTemp(Current.Name), MoveTemp(OnComplete), MoveTemp(Options));
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::DynamicDeleteCollection(
    FString Collection,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before deleting a collection."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!IsSafeAdminSegment(Collection))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Collection Name is empty, exceeds 255 characters, or contains an unsafe path character."));
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
    if (!IsAuthenticated())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before importing collections."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Collection Import Document has no valid JSON object."));
    }
    if (!Body.Data.JsonObject->TryGetArrayField(TEXT("collections"), Collections) ||
        Collections == nullptr)
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Collection Import Document must contain a collections array."));
    }
    if (Collections->IsEmpty() || Collections->Num() > 500)
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            FString::Printf(TEXT("Collection Import contains %d collections. Use from 1 to 500 collections."), Collections->Num()));
    }
    if (Body.Data.JsonObject->TryGetBoolField(TEXT("deleteMissing"), bDeleteMissing) &&
        bDeleteMissing && !Policy.bAllowDestructiveCollectionImport)
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Collection Import requests deleteMissing, but destructive collection import is disabled by the admin policy."),
            EOpenPocketBaseErrorKind::Unsupported);
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
            TEXT("Log in as a PocketBase superuser before reading server settings."), EOpenPocketBaseErrorKind::Authentication);
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
                        TEXT("PocketBase settings response must be a JSON object before sensitive values can be redacted.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::UpdateSettings(
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before updating server settings."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Settings Document has no valid JSON object. Build the settings update before sending it."));
    }
    FString RedactedPath;
    if (FindRedactedSettingValue(Body.Data, RedactedPath))
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(
            MoveTemp(OnComplete),
            FString::Printf(
                TEXT("Settings Document contains the literal [REDACTED] placeholder in secret field '%s'. Replace it with the real secret or omit that field before updating settings."),
                *RedactedPath));
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
                        TEXT("PocketBase update-settings response must be a JSON object before sensitive values can be redacted.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::TestS3(
    FOpenPocketBaseAdminDocument Body,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before testing S3 settings."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("S3 Test Document has no valid JSON object. Supply the bounded S3 test settings."));
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
    if (!IsAuthenticated())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before testing email settings."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!Body.Data.JsonObject.IsValid())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Email Test Document has no valid JSON object. Supply the bounded email test settings."));
    }
    return SendAdminRequest<bool>(CoreClient,
        MakeAdminJsonRequest(EOpenPocketBaseCustomRouteMethod::Post,
            TEXT("/api/settings/test/email"), MoveTemp(Body), MoveTemp(Options), Policy),
        MoveTemp(OnComplete), ParseAdminEmpty);
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::ListLogs(
    FOpenPocketBaseAdminLogListOptions Options,
    FOpenPocketBaseAdminPageCallback OnComplete)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminPage>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before listing logs."), EOpenPocketBaseErrorKind::Authentication);
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
                        TEXT("PocketBase log list response must contain valid page, perPage, totalItems, totalPages, and items fields, and items cannot exceed the admin policy page bound. Check the PocketBase version and any response hook.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::DynamicListLogs(
    FOpenPocketBaseDynamicAdminListOptions Options,
    FOpenPocketBaseAdminPageCallback OnComplete)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminPage>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before listing logs."), EOpenPocketBaseErrorKind::Authentication);
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
                        TEXT("PocketBase log list response must contain valid page, perPage, totalItems, totalPages, and items fields, and items cannot exceed the admin policy page bound. Check the PocketBase version and any response hook.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::GetLog(
    FString LogId,
    FOpenPocketBaseAdminDocumentCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before getting a log entry."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!IsSafeAdminSegment(LogId))
    {
        return FailAdminRequest<FOpenPocketBaseAdminDocument>(MoveTemp(OnComplete),
            TEXT("Log ID is empty, exceeds 255 characters, or contains an unsafe path character."));
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
                        TEXT("PocketBase log response must contain the log entry as a JSON object.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::ListBackups(
    FOpenPocketBaseAdminBackupListCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminBackupList>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before listing backups."), EOpenPocketBaseErrorKind::Authentication);
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
                        TEXT("PocketBase backup list response must be a bounded JSON array of backup entries with valid keys and sizes.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::CreateBackup(
    FString Name,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before creating a backup."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!IsSafeBackupKey(Name))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Backup Name must be 5 to 150 characters, end with .zip, and use only lowercase letters, numbers, underscores, or hyphens before the extension."));
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
    FOpenPocketBaseAdminBackupInput Backup,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before uploading a backup."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!Backup.IsValid())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Backup Input must contain a non-empty .zip file name and either a file path or inline bytes."));
    }
    FOpenPocketBaseFileInput File = MoveTemp(Backup).ToFileInput();
    if (!IsSafeBackupKey(File.FileName))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Backup File Name must be 5 to 150 characters, end with .zip, and use only lowercase letters, numbers, underscores, or hyphens before the extension."));
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
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminBackupDownload>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before downloading a backup."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!IsSafeBackupKey(Key))
    {
        return FailAdminRequest<FOpenPocketBaseAdminBackupDownload>(MoveTemp(OnComplete),
            TEXT("Backup Key must be 5 to 150 characters, end with .zip, and use only lowercase letters, numbers, underscores, or hyphens before the extension."));
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
                        TEXT("PocketBase backup token response must contain a non-empty token no longer than 4096 characters."))
                    : SanitizeSensitiveError(TokenResult.GetError(),
                        TEXT("The backup file token request failed. Check the error kind, HTTP status, and request ID without logging the token."));
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
                                    TEXT("The backup download failed. Check the error kind, HTTP status, and request ID without logging the token or backup contents.")));
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
    if (!IsAuthenticated())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before restoring a backup."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!Policy.bAllowBackupRestore)
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Backup Restore is disabled by the admin policy. Enable Allow Backup Restore explicitly before using it."),
            EOpenPocketBaseErrorKind::Unsupported);
    }
    if (!IsSafeBackupKey(Key))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Backup Key must be 5 to 150 characters, end with .zip, and use only lowercase letters, numbers, underscores, or hyphens before the extension."));
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
    if (!IsAuthenticated())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before deleting a backup."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!IsSafeBackupKey(Key))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Backup Key must be 5 to 150 characters, end with .zip, and use only lowercase letters, numbers, underscores, or hyphens before the extension."));
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
            TEXT("Log in as a PocketBase superuser before listing cron jobs."), EOpenPocketBaseErrorKind::Authentication);
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
                        TEXT("PocketBase cron response must be a bounded JSON array of cron job objects.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::RunCron(
    FString CronId,
    FOpenPocketBaseBoolCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before running a cron job."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!IsSafeAdminSegment(CronId))
    {
        return FailAdminRequest<bool>(MoveTemp(OnComplete),
            TEXT("Cron ID is empty, exceeds 255 characters, or contains an unsafe path character."));
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
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminSqlResult>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before running SQL."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (Trimmed.IsEmpty() || Trimmed.Len() > 5000)
    {
        return FailAdminRequest<FOpenPocketBaseAdminSqlResult>(MoveTemp(OnComplete),
            FString::Printf(TEXT("SQL Query contains %d characters after trimming. Use from 1 to 5000 characters."), Trimmed.Len()));
    }
    if (!Policy.bAllowSqlWrites && !IsConservativeReadOnlySql(Trimmed))
    {
        return FailAdminRequest<FOpenPocketBaseAdminSqlResult>(MoveTemp(OnComplete),
            TEXT("SQL Query is not recognized as a single read-only statement, and SQL writes are disabled by the admin policy."),
            EOpenPocketBaseErrorKind::Unsupported);
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
                        TEXT("The SQL request failed. Check the error kind, HTTP status, and request ID. Query text and result data were removed from this error."))));
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
                        TEXT("PocketBase SQL response exceeds Max SQL Rows or does not contain a valid columns-and-rows result.")));
        });
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::Impersonate(
    FOpenPocketBaseAuthCollectionRef AuthCollection,
    FString RecordId,
    const int64 DurationSeconds,
    FOpenPocketBaseAdminImpersonationCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    FOpenPocketBaseAuthCollectionRef Current;
    if (!AuthCollection.ResolveCurrentAs(Current))
    {
        return FailAdminRequest<FOpenPocketBaseAdminImpersonationResult>(
            MoveTemp(OnComplete),
            TEXT("Auth Collection is missing, stale, or is not an auth collection. Choose it from the current imported schema."));
    }
    return DynamicImpersonate(
        MoveTemp(Current.Name),
        MoveTemp(RecordId),
        DurationSeconds,
        MoveTemp(OnComplete),
        MoveTemp(Options));
}

FOpenPocketBaseAdminRequestHandle FOpenPocketBaseAdminClient::DynamicImpersonate(
    FString AuthCollection,
    FString RecordId,
    const int64 DurationSeconds,
    FOpenPocketBaseAdminImpersonationCallback OnComplete,
    FOpenPocketBaseRequestOptions Options)
{
    if (!IsAuthenticated())
    {
        return FailAdminRequest<FOpenPocketBaseAdminImpersonationResult>(MoveTemp(OnComplete),
            TEXT("Log in as a PocketBase superuser before impersonating a user."),
            EOpenPocketBaseErrorKind::Authentication);
    }
    if (!Policy.bAllowImpersonation)
    {
        return FailAdminRequest<FOpenPocketBaseAdminImpersonationResult>(MoveTemp(OnComplete),
            TEXT("Impersonation is disabled by the admin policy. Enable Allow Impersonation explicitly before using it."),
            EOpenPocketBaseErrorKind::Unsupported);
    }
    if (!IsSafeAdminSegment(AuthCollection))
    {
        return FailAdminRequest<FOpenPocketBaseAdminImpersonationResult>(MoveTemp(OnComplete),
            TEXT("Auth Collection is empty, exceeds 255 characters, or contains an unsafe path character."));
    }
    if (!IsSafeAdminSegment(RecordId))
    {
        return FailAdminRequest<FOpenPocketBaseAdminImpersonationResult>(MoveTemp(OnComplete),
            TEXT("Record ID is empty, exceeds 255 characters, or contains an unsafe path character."));
    }
    if (DurationSeconds < 0 || DurationSeconds > 365LL * 24 * 60 * 60)
    {
        return FailAdminRequest<FOpenPocketBaseAdminImpersonationResult>(MoveTemp(OnComplete),
            FString::Printf(TEXT("Impersonation Duration Seconds is %lld. Use a value from 0 to 31536000 seconds."), DurationSeconds));
    }
    FOpenPocketBaseAdminDocument Body;
    Body.Data.JsonObject = MakeShared<FJsonObject>();
    Body.Data.JsonObject->SetNumberField(TEXT("duration"), DurationSeconds);
    FOpenPocketBaseClientConfig ImpersonatedConfig = CoreConfig;
    ImpersonatedConfig.DefaultHeaders.Reset();
    const FOpenPocketBaseClientConfig CapturedConfig = MoveTemp(ImpersonatedConfig);
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
                        TEXT("PocketBase impersonation response must contain a token and auth record for the requested user.")));
            }
            FOpenPocketBaseClientDependencies Dependencies;
            Dependencies.Transport = CapturedTransport;
            FOpenPocketBaseClientResult ClientResult =
                FOpenPocketBaseClient::CreateDynamicEphemeralAuthenticated(
                    CapturedConfig,
                    MoveTemp(Token),
                    AuthCollection,
                    Record,
                    MoveTemp(Dependencies));
            if (!ClientResult.IsSuccess())
            {
                return TOpenPocketBaseResult<FOpenPocketBaseAdminImpersonationResult>::Failure(
                    SanitizeSensitiveError(ClientResult.GetError(),
                        TEXT("PocketBase returned an impersonation session, but the SDK could not create its client. Check the error kind and confirm the returned auth collection still exists.")));
            }
            FOpenPocketBaseAdminImpersonationResult Result;
            Result.Client = ClientResult.TakeValue();
            Result.Record = MoveTemp(Record);
            return TOpenPocketBaseResult<FOpenPocketBaseAdminImpersonationResult>::Success(
                MoveTemp(Result));
        });
}
