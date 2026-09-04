// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseAdminQueryLibrary.h"

namespace
{
template <typename AdminFilterType>
AdminFilterType FromRecordFilter(const FOpenPocketBaseFilter& Filter)
{
    AdminFilterType Result;
    Result.Expression = Filter.Expression;
    Result.bValid = Filter.bValid;
    Result.ErrorMessage = Filter.ErrorMessage;
    return Result;
}

template <typename AdminFilterType>
FOpenPocketBaseFilter ToRecordFilter(const AdminFilterType& Filter)
{
    FOpenPocketBaseFilter Result;
    Result.Expression = Filter.Expression;
    Result.bValid = Filter.bValid;
    Result.ErrorMessage = Filter.ErrorMessage;
    return Result;
}

FString CollectionTextFieldName(const EOpenPocketBaseAdminCollectionTextField Field)
{
    switch (Field)
    {
    case EOpenPocketBaseAdminCollectionTextField::Id: return TEXT("id");
    case EOpenPocketBaseAdminCollectionTextField::Name: return TEXT("name");
    }
    return {};
}

FString CollectionDateFieldName(const EOpenPocketBaseAdminCollectionDateField Field)
{
    switch (Field)
    {
    case EOpenPocketBaseAdminCollectionDateField::Created: return TEXT("created");
    case EOpenPocketBaseAdminCollectionDateField::Updated: return TEXT("updated");
    }
    return {};
}

FString CollectionTypeName(const EOpenPocketBaseCollectionType Type)
{
    switch (Type)
    {
    case EOpenPocketBaseCollectionType::Base: return TEXT("base");
    case EOpenPocketBaseCollectionType::Auth: return TEXT("auth");
    case EOpenPocketBaseCollectionType::View: return TEXT("view");
    default: return {};
    }
}

FString CollectionSortFieldName(const EOpenPocketBaseAdminCollectionSortField Field)
{
    switch (Field)
    {
    case EOpenPocketBaseAdminCollectionSortField::Random: return TEXT("@random");
    case EOpenPocketBaseAdminCollectionSortField::Id: return TEXT("id");
    case EOpenPocketBaseAdminCollectionSortField::Created: return TEXT("created");
    case EOpenPocketBaseAdminCollectionSortField::Updated: return TEXT("updated");
    case EOpenPocketBaseAdminCollectionSortField::Name: return TEXT("name");
    case EOpenPocketBaseAdminCollectionSortField::Type: return TEXT("type");
    case EOpenPocketBaseAdminCollectionSortField::System: return TEXT("system");
    }
    return {};
}

FString CollectionProjectionFieldName(
    const EOpenPocketBaseAdminCollectionProjectionField Field)
{
    switch (Field)
    {
    case EOpenPocketBaseAdminCollectionProjectionField::Id: return TEXT("id");
    case EOpenPocketBaseAdminCollectionProjectionField::Created: return TEXT("created");
    case EOpenPocketBaseAdminCollectionProjectionField::Updated: return TEXT("updated");
    case EOpenPocketBaseAdminCollectionProjectionField::Name: return TEXT("name");
    case EOpenPocketBaseAdminCollectionProjectionField::Type: return TEXT("type");
    case EOpenPocketBaseAdminCollectionProjectionField::System: return TEXT("system");
    case EOpenPocketBaseAdminCollectionProjectionField::Fields: return TEXT("fields");
    case EOpenPocketBaseAdminCollectionProjectionField::Indexes: return TEXT("indexes");
    case EOpenPocketBaseAdminCollectionProjectionField::ListRule: return TEXT("listRule");
    case EOpenPocketBaseAdminCollectionProjectionField::ViewRule: return TEXT("viewRule");
    case EOpenPocketBaseAdminCollectionProjectionField::CreateRule: return TEXT("createRule");
    case EOpenPocketBaseAdminCollectionProjectionField::UpdateRule: return TEXT("updateRule");
    case EOpenPocketBaseAdminCollectionProjectionField::DeleteRule: return TEXT("deleteRule");
    case EOpenPocketBaseAdminCollectionProjectionField::ViewQuery: return TEXT("viewQuery");
    case EOpenPocketBaseAdminCollectionProjectionField::AuthRule: return TEXT("authRule");
    case EOpenPocketBaseAdminCollectionProjectionField::ManageRule: return TEXT("manageRule");
    case EOpenPocketBaseAdminCollectionProjectionField::AuthAlert: return TEXT("authAlert");
    case EOpenPocketBaseAdminCollectionProjectionField::OAuth2: return TEXT("oauth2");
    case EOpenPocketBaseAdminCollectionProjectionField::PasswordAuth: return TEXT("passwordAuth");
    case EOpenPocketBaseAdminCollectionProjectionField::Mfa: return TEXT("mfa");
    case EOpenPocketBaseAdminCollectionProjectionField::Otp: return TEXT("otp");
    case EOpenPocketBaseAdminCollectionProjectionField::AuthToken: return TEXT("authToken");
    case EOpenPocketBaseAdminCollectionProjectionField::PasswordResetToken:
        return TEXT("passwordResetToken");
    case EOpenPocketBaseAdminCollectionProjectionField::EmailChangeToken:
        return TEXT("emailChangeToken");
    case EOpenPocketBaseAdminCollectionProjectionField::VerificationToken:
        return TEXT("verificationToken");
    case EOpenPocketBaseAdminCollectionProjectionField::FileToken: return TEXT("fileToken");
    case EOpenPocketBaseAdminCollectionProjectionField::VerificationTemplate:
        return TEXT("verificationTemplate");
    case EOpenPocketBaseAdminCollectionProjectionField::ResetPasswordTemplate:
        return TEXT("resetPasswordTemplate");
    case EOpenPocketBaseAdminCollectionProjectionField::ConfirmEmailChangeTemplate:
        return TEXT("confirmEmailChangeTemplate");
    }
    return {};
}

FString LogTextFieldName(const EOpenPocketBaseAdminLogTextField Field)
{
    switch (Field)
    {
    case EOpenPocketBaseAdminLogTextField::Id: return TEXT("id");
    case EOpenPocketBaseAdminLogTextField::Message: return TEXT("message");
    }
    return {};
}

FString LogDateFieldName(const EOpenPocketBaseAdminLogDateField Field)
{
    switch (Field)
    {
    case EOpenPocketBaseAdminLogDateField::Created: return TEXT("created");
    case EOpenPocketBaseAdminLogDateField::Updated: return TEXT("updated");
    }
    return {};
}

FString LogSortFieldName(const EOpenPocketBaseAdminLogSortField Field)
{
    switch (Field)
    {
    case EOpenPocketBaseAdminLogSortField::Random: return TEXT("@random");
    case EOpenPocketBaseAdminLogSortField::RowId: return TEXT("rowid");
    case EOpenPocketBaseAdminLogSortField::Id: return TEXT("id");
    case EOpenPocketBaseAdminLogSortField::Created: return TEXT("created");
    case EOpenPocketBaseAdminLogSortField::Updated: return TEXT("updated");
    case EOpenPocketBaseAdminLogSortField::Level: return TEXT("level");
    case EOpenPocketBaseAdminLogSortField::Message: return TEXT("message");
    }
    return {};
}

FString LogProjectionFieldName(const EOpenPocketBaseAdminLogProjectionField Field)
{
    switch (Field)
    {
    case EOpenPocketBaseAdminLogProjectionField::Id: return TEXT("id");
    case EOpenPocketBaseAdminLogProjectionField::Created: return TEXT("created");
    case EOpenPocketBaseAdminLogProjectionField::Updated: return TEXT("updated");
    case EOpenPocketBaseAdminLogProjectionField::Level: return TEXT("level");
    case EOpenPocketBaseAdminLogProjectionField::Message: return TEXT("message");
    case EOpenPocketBaseAdminLogProjectionField::Data: return TEXT("data");
    }
    return {};
}

bool IsSafeLogDataField(const FString& Field)
{
    if (Field.IsEmpty() || Field.Len() > 255 || Field.StartsWith(TEXT(".")) ||
        Field.EndsWith(TEXT(".")) || Field.Contains(TEXT("..")))
    {
        return false;
    }
    for (const TCHAR Character : Field)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('.'))
        {
            return false;
        }
    }
    return true;
}
}

bool FOpenPocketBaseAdminCollectionFilter::IsEmpty() const
{
    return Expression.IsEmpty();
}

bool FOpenPocketBaseAdminCollectionFilter::IsValid() const
{
    return bValid;
}

bool FOpenPocketBaseAdminLogFilter::IsEmpty() const
{
    return Expression.IsEmpty();
}

bool FOpenPocketBaseAdminLogFilter::IsValid() const
{
    return bValid;
}

FString FOpenPocketBaseAdminCollectionSort::ToQueryValue() const
{
    const FString Name = CollectionSortFieldName(Field);
    return Direction == EOpenPocketBaseSortDirection::Descending && !Name.IsEmpty()
        ? TEXT("-") + Name
        : Name;
}

FString FOpenPocketBaseAdminLogSort::ToQueryValue() const
{
    const FString Name = DynamicDataField.IsEmpty()
        ? LogSortFieldName(Field)
        : IsSafeLogDataField(DynamicDataField)
            ? TEXT("data.") + DynamicDataField
            : FString();
    return Direction == EOpenPocketBaseSortDirection::Descending && !Name.IsEmpty()
        ? TEXT("-") + Name
        : Name;
}

FOpenPocketBaseAdminCollectionListOptions&
FOpenPocketBaseAdminCollectionListOptions::Where(
    FOpenPocketBaseAdminCollectionFilter InFilter)
{
    Filter = MoveTemp(InFilter);
    return *this;
}

FOpenPocketBaseAdminCollectionListOptions&
FOpenPocketBaseAdminCollectionListOptions::ThenSortBy(
    const EOpenPocketBaseAdminCollectionSortField Field,
    const EOpenPocketBaseSortDirection Direction)
{
    FOpenPocketBaseAdminCollectionSort Value;
    Value.Field = Field;
    Value.Direction = Direction;
    Sort.Add(Value);
    return *this;
}

FOpenPocketBaseAdminCollectionListOptions&
FOpenPocketBaseAdminCollectionListOptions::Select(
    const EOpenPocketBaseAdminCollectionProjectionField Field)
{
    Fields.AddUnique(Field);
    return *this;
}

FOpenPocketBaseAdminLogListOptions& FOpenPocketBaseAdminLogListOptions::Where(
    FOpenPocketBaseAdminLogFilter InFilter)
{
    Filter = MoveTemp(InFilter);
    return *this;
}

FOpenPocketBaseAdminLogListOptions& FOpenPocketBaseAdminLogListOptions::ThenSortBy(
    const EOpenPocketBaseAdminLogSortField Field,
    const EOpenPocketBaseSortDirection Direction)
{
    FOpenPocketBaseAdminLogSort Value;
    Value.Field = Field;
    Value.Direction = Direction;
    Sort.Add(Value);
    return *this;
}

FOpenPocketBaseAdminLogListOptions&
FOpenPocketBaseAdminLogListOptions::ThenSortByDynamicDataField(
    FString DataField,
    const EOpenPocketBaseSortDirection Direction)
{
    FOpenPocketBaseAdminLogSort Value;
    Value.DynamicDataField = MoveTemp(DataField);
    Value.Direction = Direction;
    Sort.Add(MoveTemp(Value));
    return *this;
}

FOpenPocketBaseAdminLogListOptions& FOpenPocketBaseAdminLogListOptions::Select(
    const EOpenPocketBaseAdminLogProjectionField Field)
{
    Fields.AddUnique(Field);
    return *this;
}

namespace OpenPocketBase::AdminQuery
{
FOpenPocketBaseAdminCollectionFilter CollectionText(
    const EOpenPocketBaseAdminCollectionTextField Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return FromRecordFilter<FOpenPocketBaseAdminCollectionFilter>(
        FOpenPocketBaseFilter::DynamicString(CollectionTextFieldName(Field), Comparison, Value));
}

FOpenPocketBaseAdminCollectionFilter CollectionType(
    const EOpenPocketBaseStringComparison Comparison,
    const EOpenPocketBaseCollectionType Value)
{
    return FromRecordFilter<FOpenPocketBaseAdminCollectionFilter>(
        FOpenPocketBaseFilter::DynamicString(TEXT("type"), Comparison, CollectionTypeName(Value)));
}

FOpenPocketBaseAdminCollectionFilter CollectionSystem(
    const EOpenPocketBaseBooleanComparison Comparison,
    const bool bValue)
{
    return FromRecordFilter<FOpenPocketBaseAdminCollectionFilter>(
        FOpenPocketBaseFilter::DynamicBoolean(TEXT("system"), Comparison, bValue));
}

FOpenPocketBaseAdminCollectionFilter CollectionDate(
    const EOpenPocketBaseAdminCollectionDateField Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime& Value)
{
    return FromRecordFilter<FOpenPocketBaseAdminCollectionFilter>(
        FOpenPocketBaseFilter::DynamicDate(CollectionDateFieldName(Field), Comparison, Value));
}

FOpenPocketBaseAdminCollectionFilter And(
    const FOpenPocketBaseAdminCollectionFilter& A,
    const FOpenPocketBaseAdminCollectionFilter& B)
{
    return FromRecordFilter<FOpenPocketBaseAdminCollectionFilter>(
        ToRecordFilter(A).And(ToRecordFilter(B)));
}

FOpenPocketBaseAdminCollectionFilter Or(
    const FOpenPocketBaseAdminCollectionFilter& A,
    const FOpenPocketBaseAdminCollectionFilter& B)
{
    return FromRecordFilter<FOpenPocketBaseAdminCollectionFilter>(
        ToRecordFilter(A).Or(ToRecordFilter(B)));
}

FOpenPocketBaseAdminCollectionFilter DynamicCollectionFilter(FString Expression)
{
    return FromRecordFilter<FOpenPocketBaseAdminCollectionFilter>(
        FOpenPocketBaseFilter::DynamicRaw(MoveTemp(Expression)));
}

FString CollectionProjection(const EOpenPocketBaseAdminCollectionProjectionField Field)
{
    return CollectionProjectionFieldName(Field);
}

FOpenPocketBaseAdminLogFilter LogText(
    const EOpenPocketBaseAdminLogTextField Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return FromRecordFilter<FOpenPocketBaseAdminLogFilter>(
        FOpenPocketBaseFilter::DynamicString(LogTextFieldName(Field), Comparison, Value));
}

FOpenPocketBaseAdminLogFilter LogLevel(
    const EOpenPocketBaseNumberComparison Comparison,
    const double Value)
{
    return FromRecordFilter<FOpenPocketBaseAdminLogFilter>(
        FOpenPocketBaseFilter::DynamicNumber(TEXT("level"), Comparison, Value));
}

FOpenPocketBaseAdminLogFilter LogDate(
    const EOpenPocketBaseAdminLogDateField Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime& Value)
{
    return FromRecordFilter<FOpenPocketBaseAdminLogFilter>(
        FOpenPocketBaseFilter::DynamicDate(LogDateFieldName(Field), Comparison, Value));
}

FOpenPocketBaseAdminLogFilter And(
    const FOpenPocketBaseAdminLogFilter& A,
    const FOpenPocketBaseAdminLogFilter& B)
{
    return FromRecordFilter<FOpenPocketBaseAdminLogFilter>(
        ToRecordFilter(A).And(ToRecordFilter(B)));
}

FOpenPocketBaseAdminLogFilter Or(
    const FOpenPocketBaseAdminLogFilter& A,
    const FOpenPocketBaseAdminLogFilter& B)
{
    return FromRecordFilter<FOpenPocketBaseAdminLogFilter>(
        ToRecordFilter(A).Or(ToRecordFilter(B)));
}

FOpenPocketBaseAdminLogFilter DynamicLogFilter(FString Expression)
{
    return FromRecordFilter<FOpenPocketBaseAdminLogFilter>(
        FOpenPocketBaseFilter::DynamicRaw(MoveTemp(Expression)));
}


FString LogProjection(const EOpenPocketBaseAdminLogProjectionField Field)
{
    return LogProjectionFieldName(Field);
}
}

FOpenPocketBaseAdminCollectionListOptions
UOpenPocketBaseAdminQueryLibrary::CollectionListOptions(const int32 Page, const int32 PerPage)
{
    FOpenPocketBaseAdminCollectionListOptions Options;
    Options.Page = Page;
    Options.PerPage = PerPage;
    return Options;
}

FOpenPocketBaseAdminCollectionListOptions
UOpenPocketBaseAdminQueryLibrary::WhereCollections(
    FOpenPocketBaseAdminCollectionListOptions Options,
    FOpenPocketBaseAdminCollectionFilter Filter)
{
    return MoveTemp(Options.Where(MoveTemp(Filter)));
}

FOpenPocketBaseAdminCollectionListOptions
UOpenPocketBaseAdminQueryLibrary::ThenSortCollectionsBy(
    FOpenPocketBaseAdminCollectionListOptions Options,
    const EOpenPocketBaseAdminCollectionSortField Field,
    const EOpenPocketBaseSortDirection Direction)
{
    return MoveTemp(Options.ThenSortBy(Field, Direction));
}

FOpenPocketBaseAdminCollectionListOptions
UOpenPocketBaseAdminQueryLibrary::SelectCollectionField(
    FOpenPocketBaseAdminCollectionListOptions Options,
    const EOpenPocketBaseAdminCollectionProjectionField Field)
{
    return MoveTemp(Options.Select(Field));
}

FOpenPocketBaseAdminCollectionFilter UOpenPocketBaseAdminQueryLibrary::CollectionTextFilter(
    const EOpenPocketBaseAdminCollectionTextField Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return OpenPocketBase::AdminQuery::CollectionText(Field, Comparison, Value);
}

FOpenPocketBaseAdminCollectionFilter UOpenPocketBaseAdminQueryLibrary::CollectionTypeFilter(
    const EOpenPocketBaseStringComparison Comparison,
    const EOpenPocketBaseCollectionType Value)
{
    return OpenPocketBase::AdminQuery::CollectionType(Comparison, Value);
}

FOpenPocketBaseAdminCollectionFilter UOpenPocketBaseAdminQueryLibrary::CollectionSystemFilter(
    const EOpenPocketBaseBooleanComparison Comparison,
    const bool bValue)
{
    return OpenPocketBase::AdminQuery::CollectionSystem(Comparison, bValue);
}

FOpenPocketBaseAdminCollectionFilter UOpenPocketBaseAdminQueryLibrary::CollectionDateFilter(
    const EOpenPocketBaseAdminCollectionDateField Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime Value)
{
    return OpenPocketBase::AdminQuery::CollectionDate(Field, Comparison, Value);
}

FOpenPocketBaseAdminCollectionFilter UOpenPocketBaseAdminQueryLibrary::AndCollectionFilters(
    FOpenPocketBaseAdminCollectionFilter A,
    FOpenPocketBaseAdminCollectionFilter B)
{
    return OpenPocketBase::AdminQuery::And(A, B);
}

FOpenPocketBaseAdminCollectionFilter UOpenPocketBaseAdminQueryLibrary::OrCollectionFilters(
    FOpenPocketBaseAdminCollectionFilter A,
    FOpenPocketBaseAdminCollectionFilter B)
{
    return OpenPocketBase::AdminQuery::Or(A, B);
}

FOpenPocketBaseAdminCollectionFilter UOpenPocketBaseAdminQueryLibrary::DynamicCollectionFilter(
    const FString& Expression)
{
    return OpenPocketBase::AdminQuery::DynamicCollectionFilter(Expression);
}

FOpenPocketBaseAdminLogListOptions UOpenPocketBaseAdminQueryLibrary::LogListOptions(
    const int32 Page,
    const int32 PerPage)
{
    FOpenPocketBaseAdminLogListOptions Options;
    Options.Page = Page;
    Options.PerPage = PerPage;
    return Options;
}

FOpenPocketBaseAdminLogListOptions UOpenPocketBaseAdminQueryLibrary::WhereLogs(
    FOpenPocketBaseAdminLogListOptions Options,
    FOpenPocketBaseAdminLogFilter Filter)
{
    return MoveTemp(Options.Where(MoveTemp(Filter)));
}

FOpenPocketBaseAdminLogListOptions UOpenPocketBaseAdminQueryLibrary::ThenSortLogsBy(
    FOpenPocketBaseAdminLogListOptions Options,
    const EOpenPocketBaseAdminLogSortField Field,
    const EOpenPocketBaseSortDirection Direction)
{
    return MoveTemp(Options.ThenSortBy(Field, Direction));
}

FOpenPocketBaseAdminLogListOptions
UOpenPocketBaseAdminQueryLibrary::ThenSortLogsByDynamicDataField(
    FOpenPocketBaseAdminLogListOptions Options,
    const FString& DataField,
    const EOpenPocketBaseSortDirection Direction)
{
    return MoveTemp(Options.ThenSortByDynamicDataField(DataField, Direction));
}

FOpenPocketBaseAdminLogListOptions UOpenPocketBaseAdminQueryLibrary::SelectLogField(
    FOpenPocketBaseAdminLogListOptions Options,
    const EOpenPocketBaseAdminLogProjectionField Field)
{
    return MoveTemp(Options.Select(Field));
}

FOpenPocketBaseAdminLogFilter UOpenPocketBaseAdminQueryLibrary::LogTextFilter(
    const EOpenPocketBaseAdminLogTextField Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return OpenPocketBase::AdminQuery::LogText(Field, Comparison, Value);
}

FOpenPocketBaseAdminLogFilter UOpenPocketBaseAdminQueryLibrary::LogLevelFilter(
    const EOpenPocketBaseNumberComparison Comparison,
    const double Value)
{
    return OpenPocketBase::AdminQuery::LogLevel(Comparison, Value);
}

FOpenPocketBaseAdminLogFilter UOpenPocketBaseAdminQueryLibrary::LogDateFilter(
    const EOpenPocketBaseAdminLogDateField Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime Value)
{
    return OpenPocketBase::AdminQuery::LogDate(Field, Comparison, Value);
}

FOpenPocketBaseAdminLogFilter UOpenPocketBaseAdminQueryLibrary::AndLogFilters(
    FOpenPocketBaseAdminLogFilter A,
    FOpenPocketBaseAdminLogFilter B)
{
    return OpenPocketBase::AdminQuery::And(A, B);
}

FOpenPocketBaseAdminLogFilter UOpenPocketBaseAdminQueryLibrary::OrLogFilters(
    FOpenPocketBaseAdminLogFilter A,
    FOpenPocketBaseAdminLogFilter B)
{
    return OpenPocketBase::AdminQuery::Or(A, B);
}

FOpenPocketBaseAdminLogFilter UOpenPocketBaseAdminQueryLibrary::DynamicLogFilter(
    const FString& Expression)
{
    return OpenPocketBase::AdminQuery::DynamicLogFilter(Expression);
}
