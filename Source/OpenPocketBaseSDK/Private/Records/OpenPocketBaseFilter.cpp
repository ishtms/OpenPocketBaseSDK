#include "OpenPocketBaseFilter.h"

#include "OpenPocketBaseDate.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

namespace
{
using FCondensedJsonWriter = TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

bool IsValidParameterName(const FString& Name)
{
    if (Name.IsEmpty() || (!FChar::IsAlpha(Name[0]) && Name[0] != TEXT('_')))
    {
        return false;
    }
    for (const TCHAR Character : Name)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
        {
            return false;
        }
    }
    return true;
}

bool IsValidField(const FString& Field)
{
    if (Field.IsEmpty() || Field.Len() > 512)
    {
        return false;
    }

    bool bAtSegmentStart = true;
    for (const TCHAR Character : Field)
    {
        if (Character == TEXT('.'))
        {
            if (bAtSegmentStart)
            {
                return false;
            }
            bAtSegmentStart = true;
            continue;
        }
        if (bAtSegmentStart && !FChar::IsAlpha(Character) && Character != TEXT('_'))
        {
            return false;
        }
        if (!bAtSegmentStart && !FChar::IsAlnum(Character) && Character != TEXT('_'))
        {
            return false;
        }
        bAtSegmentStart = false;
    }
    return !bAtSegmentStart;
}

FString EncodeString(const FString& Value)
{
    FString Json;
    const TSharedRef<FCondensedJsonWriter> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    Writer->WriteValue(Value);
    Writer->Close();
    return Json;
}

FString EncodeNumber(const double Value)
{
    FString Json;
    const TSharedRef<FCondensedJsonWriter> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    Writer->WriteArrayStart();
    Writer->WriteValue(Value);
    Writer->WriteArrayEnd();
    Writer->Close();
    return Json.Mid(1, Json.Len() - 2);
}

template <typename WriteValuesType>
FString EncodeArray(WriteValuesType&& WriteValues)
{
    FString Json;
    const TSharedRef<FCondensedJsonWriter> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    Writer->WriteArrayStart();
    WriteValues(Writer);
    Writer->WriteArrayEnd();
    Writer->Close();
    return EncodeString(Json);
}

FOpenPocketBaseError MakeBindingError(const TCHAR* Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    Error.ServerMessage = Message;
    return Error;
}

FOpenPocketBaseFilter MakeInvalidFilter(FString Message)
{
    FOpenPocketBaseFilter Filter;
    Filter.bValid = false;
    Filter.ErrorMessage = MoveTemp(Message);
    return Filter;
}

FOpenPocketBaseFilter MakeComparison(
    FString Field,
    const TCHAR* Operator,
    FString EncodedValue)
{
    Field.TrimStartAndEndInline();
    if (!IsValidField(Field))
    {
        return MakeInvalidFilter(TEXT("Filter fields must be valid collection field paths."));
    }

    FOpenPocketBaseFilter Filter;
    Filter.Expression = FString::Printf(TEXT("%s %s %s"), *Field, Operator, *EncodedValue);
    return Filter;
}

const TCHAR* StringOperator(const EOpenPocketBaseStringComparison Comparison)
{
    switch (Comparison)
    {
    case EOpenPocketBaseStringComparison::NotEquals: return TEXT("!=");
    case EOpenPocketBaseStringComparison::Contains: return TEXT("~");
    case EOpenPocketBaseStringComparison::DoesNotContain: return TEXT("!~");
    case EOpenPocketBaseStringComparison::AnyEquals: return TEXT("?=");
    case EOpenPocketBaseStringComparison::AnyNotEquals: return TEXT("?!=");
    case EOpenPocketBaseStringComparison::AnyContains: return TEXT("?~");
    case EOpenPocketBaseStringComparison::AnyDoesNotContain: return TEXT("?!~");
    default: return TEXT("=");
    }
}

const TCHAR* NumberOperator(const EOpenPocketBaseNumberComparison Comparison)
{
    switch (Comparison)
    {
    case EOpenPocketBaseNumberComparison::NotEquals: return TEXT("!=");
    case EOpenPocketBaseNumberComparison::GreaterThan: return TEXT(">");
    case EOpenPocketBaseNumberComparison::GreaterThanOrEqual: return TEXT(">=");
    case EOpenPocketBaseNumberComparison::LessThan: return TEXT("<");
    case EOpenPocketBaseNumberComparison::LessThanOrEqual: return TEXT("<=");
    case EOpenPocketBaseNumberComparison::AnyEquals: return TEXT("?=");
    case EOpenPocketBaseNumberComparison::AnyNotEquals: return TEXT("?!=");
    case EOpenPocketBaseNumberComparison::AnyGreaterThan: return TEXT("?>");
    case EOpenPocketBaseNumberComparison::AnyGreaterThanOrEqual: return TEXT("?>=");
    case EOpenPocketBaseNumberComparison::AnyLessThan: return TEXT("?<");
    case EOpenPocketBaseNumberComparison::AnyLessThanOrEqual: return TEXT("?<=");
    default: return TEXT("=");
    }
}

const TCHAR* BooleanOperator(const EOpenPocketBaseBooleanComparison Comparison)
{
    switch (Comparison)
    {
    case EOpenPocketBaseBooleanComparison::NotEquals: return TEXT("!=");
    case EOpenPocketBaseBooleanComparison::AnyEquals: return TEXT("?=");
    case EOpenPocketBaseBooleanComparison::AnyNotEquals: return TEXT("?!=");
    default: return TEXT("=");
    }
}

const TCHAR* DateOperator(const EOpenPocketBaseDateComparison Comparison)
{
    switch (Comparison)
    {
    case EOpenPocketBaseDateComparison::NotEquals: return TEXT("!=");
    case EOpenPocketBaseDateComparison::After: return TEXT(">");
    case EOpenPocketBaseDateComparison::OnOrAfter: return TEXT(">=");
    case EOpenPocketBaseDateComparison::Before: return TEXT("<");
    case EOpenPocketBaseDateComparison::OnOrBefore: return TEXT("<=");
    case EOpenPocketBaseDateComparison::AnyEquals: return TEXT("?=");
    case EOpenPocketBaseDateComparison::AnyNotEquals: return TEXT("?!=");
    case EOpenPocketBaseDateComparison::AnyAfter: return TEXT("?>");
    case EOpenPocketBaseDateComparison::AnyOnOrAfter: return TEXT("?>=");
    case EOpenPocketBaseDateComparison::AnyBefore: return TEXT("?<");
    case EOpenPocketBaseDateComparison::AnyOnOrBefore: return TEXT("?<=");
    default: return TEXT("=");
    }
}

const TCHAR* NullOperator(const EOpenPocketBaseNullComparison Comparison)
{
    switch (Comparison)
    {
    case EOpenPocketBaseNullComparison::IsNotNull: return TEXT("!=");
    case EOpenPocketBaseNullComparison::AnyIsNull: return TEXT("?=");
    case EOpenPocketBaseNullComparison::AnyIsNotNull: return TEXT("?!=");
    default: return TEXT("=");
    }
}

FOpenPocketBaseFilter CombineFilters(
    const FOpenPocketBaseFilter& A,
    const FOpenPocketBaseFilter& B,
    const TCHAR* Operator)
{
    if (!A.IsValid())
    {
        return A;
    }
    if (!B.IsValid())
    {
        return B;
    }
    if (A.IsEmpty())
    {
        return B;
    }
    if (B.IsEmpty())
    {
        return A;
    }

    FOpenPocketBaseFilter Filter;
    Filter.Expression = FString::Printf(
        TEXT("(%s) %s (%s)"),
        *A.Expression,
        Operator,
        *B.Expression);
    if (Filter.Expression.Len() > 64 * 1024)
    {
        return MakeInvalidFilter(TEXT("The combined filter exceeds the supported length."));
    }
    return Filter;
}
}

bool FOpenPocketBaseFilterParams::AddEncoded(const FString& Name, FString EncodedValue)
{
    if (!IsValidParameterName(Name))
    {
        return false;
    }
    EncodedValues.Add(Name, MoveTemp(EncodedValue));
    return true;
}

bool FOpenPocketBaseFilterParams::AddString(const FString& Name, const FString& Value)
{
    return AddEncoded(Name, EncodeString(Value));
}

bool FOpenPocketBaseFilterParams::AddNumber(const FString& Name, const double Value)
{
    return FMath::IsFinite(Value) && AddEncoded(Name, EncodeNumber(Value));
}

bool FOpenPocketBaseFilterParams::AddBoolean(const FString& Name, const bool bValue)
{
    return AddEncoded(Name, bValue ? TEXT("true") : TEXT("false"));
}

bool FOpenPocketBaseFilterParams::AddDate(const FString& Name, const FDateTime& Value)
{
    return AddEncoded(Name, EncodeString(OpenPocketBase::Date::Format(Value)));
}

bool FOpenPocketBaseFilterParams::AddNull(const FString& Name)
{
    return AddEncoded(Name, TEXT("null"));
}

bool FOpenPocketBaseFilterParams::AddStringArray(
    const FString& Name,
    const TArray<FString>& Value)
{
    return AddEncoded(
        Name,
        EncodeArray(
            [&Value](const TSharedRef<FCondensedJsonWriter>& Writer)
            {
                for (const FString& Item : Value)
                {
                    Writer->WriteValue(Item);
                }
            }));
}

bool FOpenPocketBaseFilterParams::AddNumberArray(
    const FString& Name,
    const TArray<double>& Value)
{
    for (const double Item : Value)
    {
        if (!FMath::IsFinite(Item))
        {
            return false;
        }
    }
    return AddEncoded(
        Name,
        EncodeArray(
            [&Value](const TSharedRef<FCondensedJsonWriter>& Writer)
            {
                for (const double Item : Value)
                {
                    Writer->WriteValue(Item);
                }
            }));
}

bool FOpenPocketBaseFilterParams::AddBooleanArray(
    const FString& Name,
    const TArray<bool>& Value)
{
    return AddEncoded(
        Name,
        EncodeArray(
            [&Value](const TSharedRef<FCondensedJsonWriter>& Writer)
            {
                for (const bool bItem : Value)
                {
                    Writer->WriteValue(bItem);
                }
            }));
}

int32 FOpenPocketBaseFilterParams::Num() const
{
    return EncodedValues.Num();
}

void FOpenPocketBaseFilterParams::Reset()
{
    EncodedValues.Reset();
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::String(
    FString Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return MakeComparison(MoveTemp(Field), StringOperator(Comparison), EncodeString(Value));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::Number(
    FString Field,
    const EOpenPocketBaseNumberComparison Comparison,
    const double Value)
{
    if (!FMath::IsFinite(Value))
    {
        return MakeInvalidFilter(TEXT("Filter numbers must be finite."));
    }
    return MakeComparison(MoveTemp(Field), NumberOperator(Comparison), EncodeNumber(Value));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::Boolean(
    FString Field,
    const EOpenPocketBaseBooleanComparison Comparison,
    const bool bValue)
{
    return MakeComparison(
        MoveTemp(Field),
        BooleanOperator(Comparison),
        bValue ? TEXT("true") : TEXT("false"));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::Date(
    FString Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime& Value)
{
    return MakeComparison(
        MoveTemp(Field),
        DateOperator(Comparison),
        EncodeString(OpenPocketBase::Date::Format(Value)));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::Null(
    FString Field,
    const EOpenPocketBaseNullComparison Comparison)
{
    return MakeComparison(MoveTemp(Field), NullOperator(Comparison), TEXT("null"));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::Raw(FString InExpression)
{
    InExpression.TrimStartAndEndInline();
    if (InExpression.Len() > 64 * 1024)
    {
        return MakeInvalidFilter(TEXT("The filter expression exceeds the supported length."));
    }

    FOpenPocketBaseFilter Filter;
    Filter.Expression = MoveTemp(InExpression);
    return Filter;
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::And(const FOpenPocketBaseFilter& Other) const
{
    return CombineFilters(*this, Other, TEXT("&&"));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::Or(const FOpenPocketBaseFilter& Other) const
{
    return CombineFilters(*this, Other, TEXT("||"));
}

bool FOpenPocketBaseFilter::IsEmpty() const
{
    return Expression.IsEmpty();
}

bool FOpenPocketBaseFilter::IsValid() const
{
    return bValid;
}

const FString& FOpenPocketBaseFilter::ToString() const
{
    return Expression;
}

bool FOpenPocketBaseFilter::TryBind(
    const FString& InExpression,
    const FOpenPocketBaseFilterParams& Params,
    FOpenPocketBaseFilter& OutFilter,
    FOpenPocketBaseError& OutError)
{
    OutFilter = {};
    OutError = {};
    if (InExpression.Len() > 64 * 1024)
    {
        OutError = MakeBindingError(TEXT("The filter expression exceeds the supported length."));
        return false;
    }

    FString Bound;
    TSet<FString> UsedParameters;
    int32 Position = 0;
    while (Position < InExpression.Len())
    {
        const int32 PlaceholderStart = InExpression.Find(
            TEXT("{:"),
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            Position);
        if (PlaceholderStart == INDEX_NONE)
        {
            Bound += InExpression.Mid(Position);
            break;
        }

        Bound += InExpression.Mid(Position, PlaceholderStart - Position);
        const int32 PlaceholderEnd = InExpression.Find(
            TEXT("}"),
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            PlaceholderStart + 2);
        if (PlaceholderEnd == INDEX_NONE)
        {
            OutError = MakeBindingError(TEXT("The filter contains an unclosed parameter placeholder."));
            return false;
        }

        const FString Name = InExpression.Mid(
            PlaceholderStart + 2,
            PlaceholderEnd - PlaceholderStart - 2);
        const FString* EncodedValue = Params.EncodedValues.Find(Name);
        if (!IsValidParameterName(Name) || EncodedValue == nullptr)
        {
            OutError = MakeBindingError(TEXT("The filter contains an unknown parameter placeholder."));
            return false;
        }

        Bound += *EncodedValue;
        UsedParameters.Add(Name);
        Position = PlaceholderEnd + 1;
    }

    if (UsedParameters.Num() != Params.EncodedValues.Num())
    {
        OutError = MakeBindingError(TEXT("The filter contains unused parameters."));
        return false;
    }
    if (Bound.Len() > 64 * 1024)
    {
        OutError = MakeBindingError(TEXT("The bound filter exceeds the supported length."));
        return false;
    }

    OutFilter = Raw(MoveTemp(Bound));
    return true;
}
