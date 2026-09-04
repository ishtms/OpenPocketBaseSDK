// Copyright 2026 Ishtmeet Singh.

#include "OpenPocketBaseFilter.h"

#include "OpenPocketBaseDate.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

#include <charconv>

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
    if (Value == 0.0)
    {
        return TEXT("0");
    }

    ANSICHAR Buffer[64];
    const std::to_chars_result Converted = std::to_chars(
        Buffer,
        Buffer + UE_ARRAY_COUNT(Buffer),
        Value);
    FString Encoded;
    if (Converted.ec == std::errc())
    {
        const FUTF8ToTCHAR Utf8(Buffer, static_cast<int32>(Converted.ptr - Buffer));
        Encoded = FString(Utf8.Length(), Utf8.Get());
    }
    else
    {
        FString Json;
        const TSharedRef<FCondensedJsonWriter> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
        Writer->WriteArrayStart();
        Writer->WriteValue(Value);
        Writer->WriteArrayEnd();
        Writer->Close();
        Encoded = Json.Mid(1, Json.Len() - 2);
    }

    int32 ExponentIndex = INDEX_NONE;
    if (!Encoded.FindChar(TEXT('e'), ExponentIndex) &&
        !Encoded.FindChar(TEXT('E'), ExponentIndex))
    {
        return Encoded;
    }

    int32 Exponent = 0;
    if (!LexTryParseString(Exponent, *Encoded.Mid(ExponentIndex + 1)))
    {
        return Encoded;
    }

    FString Mantissa = Encoded.Left(ExponentIndex);
    const bool bNegative = Mantissa.RemoveFromStart(TEXT("-"));
    int32 DecimalIndex = INDEX_NONE;
    const int32 DigitsBeforeDecimal = Mantissa.FindChar(TEXT('.'), DecimalIndex)
        ? DecimalIndex
        : Mantissa.Len();
    Mantissa.ReplaceInline(TEXT("."), TEXT(""));

    const int32 NewDecimalIndex = DigitsBeforeDecimal + Exponent;
    FString Fixed;
    if (NewDecimalIndex <= 0)
    {
        Fixed = TEXT("0.") + FString::ChrN(-NewDecimalIndex, TEXT('0')) + Mantissa;
    }
    else if (NewDecimalIndex >= Mantissa.Len())
    {
        Fixed = Mantissa + FString::ChrN(NewDecimalIndex - Mantissa.Len(), TEXT('0'));
    }
    else
    {
        Fixed = Mantissa.Left(NewDecimalIndex) + TEXT(".") + Mantissa.Mid(NewDecimalIndex);
    }
    return bNegative ? TEXT("-") + Fixed : Fixed;
}

FString EncodeNumberArray(const TArray<double>& Values)
{
    FString Json = TEXT("[");
    for (int32 Index = 0; Index < Values.Num(); ++Index)
    {
        if (Index > 0)
        {
            Json += TEXT(",");
        }
        Json += EncodeNumber(Values[Index]);
    }
    Json += TEXT("]");
    return EncodeString(Json);
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

FOpenPocketBaseError MakeBindingError(FString Message)
{
    FOpenPocketBaseError Error;
    Error.Kind = EOpenPocketBaseErrorKind::InvalidArgument;
    Error.Message = MoveTemp(Message);
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
    const FOpenPocketBaseFieldRef& Field,
    const TCHAR* Operator,
    FString EncodedValue)
{
    FOpenPocketBaseFieldRef Current;
    if (!Field.ResolveCurrent(Current) || !IsValidField(Current.Name))
    {
        return MakeInvalidFilter(TEXT("Choose a valid collection field for the filter."));
    }

    FOpenPocketBaseFilter Filter;
    Filter.Expression = FString::Printf(TEXT("%s %s %s"), *Current.Name, Operator, *EncodedValue);
    Filter.SchemaId = Current.SchemaId;
    Filter.CollectionId = Current.CollectionId;
    return Filter;
}

FOpenPocketBaseFilter MakeDynamicComparison(
    FString Field,
    const TCHAR* Operator,
    FString EncodedValue)
{
    Field.TrimStartAndEndInline();
    if (!IsValidField(Field))
    {
        return MakeInvalidFilter(
            TEXT("Dynamic filter Field is empty or is not a valid dot-separated PocketBase field path. Use letters, numbers, and underscores in each path segment."));
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
    if (A.SchemaId.IsValid() && B.SchemaId.IsValid() &&
        (A.SchemaId != B.SchemaId || A.CollectionId != B.CollectionId))
    {
        return MakeInvalidFilter(TEXT("These filters target different collections. Build every part of an AND or OR filter from fields in the same collection."));
    }

    FOpenPocketBaseFilter Filter;
    Filter.Expression = FString::Printf(
        TEXT("(%s) %s (%s)"),
        *A.Expression,
        Operator,
        *B.Expression);
    Filter.SchemaId = A.SchemaId.IsValid() ? A.SchemaId : B.SchemaId;
    Filter.CollectionId = !A.CollectionId.IsEmpty() ? A.CollectionId : B.CollectionId;
    if (Filter.Expression.Len() > 64 * 1024)
    {
        return MakeInvalidFilter(FString::Printf(
            TEXT("The combined filter is %d characters, but the maximum is 65536. Remove filter terms or split the request."),
            Filter.Expression.Len()));
    }
    return Filter;
}
}

bool FOpenPocketBaseDynamicFilterParams::AddEncoded(const FString& Name, FString EncodedValue)
{
    if (!IsValidParameterName(Name))
    {
        return false;
    }
    EncodedValues.Add(Name, MoveTemp(EncodedValue));
    return true;
}

bool FOpenPocketBaseDynamicFilterParams::AddString(const FString& Name, const FString& Value)
{
    return AddEncoded(Name, EncodeString(Value));
}

bool FOpenPocketBaseDynamicFilterParams::AddNumber(const FString& Name, const double Value)
{
    return FMath::IsFinite(Value) && AddEncoded(Name, EncodeNumber(Value));
}

bool FOpenPocketBaseDynamicFilterParams::AddBoolean(const FString& Name, const bool bValue)
{
    return AddEncoded(Name, bValue ? TEXT("true") : TEXT("false"));
}

bool FOpenPocketBaseDynamicFilterParams::AddDate(const FString& Name, const FDateTime& Value)
{
    return AddEncoded(Name, EncodeString(OpenPocketBase::Date::Format(Value)));
}

bool FOpenPocketBaseDynamicFilterParams::AddNull(const FString& Name)
{
    return AddEncoded(Name, TEXT("null"));
}

bool FOpenPocketBaseDynamicFilterParams::AddStringArray(
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

bool FOpenPocketBaseDynamicFilterParams::AddNumberArray(
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
    return AddEncoded(Name, EncodeNumberArray(Value));
}

bool FOpenPocketBaseDynamicFilterParams::AddBooleanArray(
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

int32 FOpenPocketBaseDynamicFilterParams::Num() const
{
    return EncodedValues.Num();
}

void FOpenPocketBaseDynamicFilterParams::Reset()
{
    EncodedValues.Reset();
}

namespace
{
FOpenPocketBaseFilter MakeRelatedFilter(
    const FOpenPocketBaseExpand& Relations,
    FOpenPocketBaseFilter Terminal)
{
    if (!Relations.IsSet() || !Terminal.IsValid() || Terminal.IsEmpty())
    {
        return MakeInvalidFilter(TEXT("Choose a valid relation path and terminal field."));
    }

    FOpenPocketBaseRelationFieldRef LastRelation;
    if (!Relations.Path.Last().ResolveCurrentAs(LastRelation) ||
        LastRelation.SchemaId != Terminal.SchemaId ||
        LastRelation.RelatedCollectionId != Terminal.CollectionId)
    {
        return MakeInvalidFilter(TEXT("The filter field must belong to the relation path's target collection."));
    }

    FOpenPocketBaseRelationFieldRef RootRelation;
    if (!Relations.Path[0].ResolveCurrentAs(RootRelation))
    {
        return MakeInvalidFilter(TEXT("The relation path is no longer valid in the schema."));
    }

    Terminal.Expression = Relations.ToQueryValue() + TEXT(".") + Terminal.Expression;
    Terminal.SchemaId = RootRelation.SchemaId;
    Terminal.CollectionId = RootRelation.CollectionId;
    return Terminal;
}
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::String(
    const FOpenPocketBaseStringFieldRef& Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    FOpenPocketBaseStringFieldRef Current;
    if (!Field.ResolveCurrentAs(Current))
    {
        return MakeInvalidFilter(TEXT("Choose a string field for this filter."));
    }
    return MakeComparison(Current, StringOperator(Comparison), EncodeString(Value));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::StringArray(
    const FOpenPocketBaseStringArrayFieldRef& Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    FOpenPocketBaseStringArrayFieldRef Current;
    if (!Field.ResolveCurrentAs(Current))
    {
        return MakeInvalidFilter(TEXT("Choose a string-array field for this filter."));
    }
    return MakeComparison(Current, StringOperator(Comparison), EncodeString(Value));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::Number(
    const FOpenPocketBaseNumberFieldRef& Field,
    const EOpenPocketBaseNumberComparison Comparison,
    const double Value)
{
    if (!FMath::IsFinite(Value))
    {
        return MakeInvalidFilter(TEXT("Filter Value must be a finite number. NaN and infinity cannot be sent to PocketBase."));
    }
    FOpenPocketBaseNumberFieldRef Current;
    if (!Field.ResolveCurrentAs(Current))
    {
        return MakeInvalidFilter(TEXT("Choose a number field for this filter."));
    }
    return MakeComparison(Current, NumberOperator(Comparison), EncodeNumber(Value));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::Boolean(
    const FOpenPocketBaseBooleanFieldRef& Field,
    const EOpenPocketBaseBooleanComparison Comparison,
    const bool bValue)
{
    FOpenPocketBaseBooleanFieldRef Current;
    if (!Field.ResolveCurrentAs(Current))
    {
        return MakeInvalidFilter(TEXT("Choose a boolean field for this filter."));
    }
    return MakeComparison(
        Current,
        BooleanOperator(Comparison),
        bValue ? TEXT("true") : TEXT("false"));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::Date(
    const FOpenPocketBaseDateFieldRef& Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime& Value)
{
    FOpenPocketBaseDateFieldRef Current;
    if (!Field.ResolveCurrentAs(Current))
    {
        return MakeInvalidFilter(TEXT("Choose a date field for this filter."));
    }
    return MakeComparison(
        Current,
        DateOperator(Comparison),
        EncodeString(OpenPocketBase::Date::Format(Value)));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::Null(
    const FOpenPocketBaseFieldRef& Field,
    const EOpenPocketBaseNullComparison Comparison)
{
    FOpenPocketBaseFieldRef Current;
    if (!Field.ResolveCurrent(Current))
    {
        return MakeInvalidFilter(TEXT("Choose a field for this filter."));
    }
    return MakeComparison(Current, NullOperator(Comparison), TEXT("null"));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::RelatedString(
    const FOpenPocketBaseExpand& Relations,
    const FOpenPocketBaseStringFieldRef& Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return MakeRelatedFilter(Relations, String(Field, Comparison, Value));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::RelatedNumber(
    const FOpenPocketBaseExpand& Relations,
    const FOpenPocketBaseNumberFieldRef& Field,
    const EOpenPocketBaseNumberComparison Comparison,
    const double Value)
{
    return MakeRelatedFilter(Relations, Number(Field, Comparison, Value));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::RelatedBoolean(
    const FOpenPocketBaseExpand& Relations,
    const FOpenPocketBaseBooleanFieldRef& Field,
    const EOpenPocketBaseBooleanComparison Comparison,
    const bool bValue)
{
    return MakeRelatedFilter(Relations, Boolean(Field, Comparison, bValue));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::RelatedDate(
    const FOpenPocketBaseExpand& Relations,
    const FOpenPocketBaseDateFieldRef& Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime& Value)
{
    return MakeRelatedFilter(Relations, Date(Field, Comparison, Value));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::RelatedNull(
    const FOpenPocketBaseExpand& Relations,
    const FOpenPocketBaseFieldRef& Field,
    const EOpenPocketBaseNullComparison Comparison)
{
    return MakeRelatedFilter(Relations, Null(Field, Comparison));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::DynamicString(
    FString Field,
    const EOpenPocketBaseStringComparison Comparison,
    const FString& Value)
{
    return MakeDynamicComparison(MoveTemp(Field), StringOperator(Comparison), EncodeString(Value));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::DynamicNumber(
    FString Field,
    const EOpenPocketBaseNumberComparison Comparison,
    const double Value)
{
    if (!FMath::IsFinite(Value))
    {
        return MakeInvalidFilter(TEXT("Dynamic filter Value must be a finite number. NaN and infinity cannot be sent to PocketBase."));
    }
    return MakeDynamicComparison(MoveTemp(Field), NumberOperator(Comparison), EncodeNumber(Value));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::DynamicBoolean(
    FString Field,
    const EOpenPocketBaseBooleanComparison Comparison,
    const bool bValue)
{
    return MakeDynamicComparison(
        MoveTemp(Field),
        BooleanOperator(Comparison),
        bValue ? TEXT("true") : TEXT("false"));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::DynamicDate(
    FString Field,
    const EOpenPocketBaseDateComparison Comparison,
    const FDateTime& Value)
{
    return MakeDynamicComparison(
        MoveTemp(Field),
        DateOperator(Comparison),
        EncodeString(OpenPocketBase::Date::Format(Value)));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::DynamicNull(
    FString Field,
    const EOpenPocketBaseNullComparison Comparison)
{
    return MakeDynamicComparison(MoveTemp(Field), NullOperator(Comparison), TEXT("null"));
}

FOpenPocketBaseFilter FOpenPocketBaseFilter::DynamicRaw(FString InExpression)
{
    InExpression.TrimStartAndEndInline();
    if (InExpression.Len() > 64 * 1024)
    {
        return MakeInvalidFilter(FString::Printf(
            TEXT("The raw filter is %d characters, but the maximum is 65536. Shorten the expression or split the request."),
            InExpression.Len()));
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

bool FOpenPocketBaseFilter::BelongsTo(const FOpenPocketBaseCollectionRef& Collection) const
{
    return !SchemaId.IsValid() ||
        (Collection.IsSet() && SchemaId == Collection.SchemaId && CollectionId == Collection.CollectionId);
}

const FString& FOpenPocketBaseFilter::ToString() const
{
    return Expression;
}

bool FOpenPocketBaseFilter::TryBindDynamic(
    const FString& InExpression,
    const FOpenPocketBaseDynamicFilterParams& Params,
    FOpenPocketBaseFilter& OutFilter,
    FOpenPocketBaseError& OutError)
{
    OutFilter = {};
    OutError = {};
    if (InExpression.Len() > 64 * 1024)
    {
        OutError = MakeBindingError(FString::Printf(
            TEXT("The filter template is %d characters, but the maximum is 65536. Shorten the expression before binding parameters."),
            InExpression.Len()));
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
            OutError = MakeBindingError(FString::Printf(
                TEXT("The filter parameter placeholder beginning at character %d has no closing brace. Close it using the form {:parameterName}."),
                PlaceholderStart));
            return false;
        }

        const FString Name = InExpression.Mid(
            PlaceholderStart + 2,
            PlaceholderEnd - PlaceholderStart - 2);
        const FString* EncodedValue = Params.EncodedValues.Find(Name);
        if (!IsValidParameterName(Name))
        {
            OutError = MakeBindingError(FString::Printf(
                TEXT("Filter parameter name '%s' is invalid. Start with a letter or underscore and use only letters, numbers, and underscores."),
                *Name));
            return false;
        }
        if (EncodedValue == nullptr)
        {
            OutError = MakeBindingError(FString::Printf(
                TEXT("Filter parameter '%s' has no bound value. Add that parameter before binding the filter."),
                *Name));
            return false;
        }

        Bound += *EncodedValue;
        UsedParameters.Add(Name);
        Position = PlaceholderEnd + 1;
    }

    if (UsedParameters.Num() != Params.EncodedValues.Num())
    {
        TArray<FString> UnusedParameters;
        Params.EncodedValues.GetKeys(UnusedParameters);
        UnusedParameters.RemoveAll(
            [&UsedParameters](const FString& Name)
            {
                return UsedParameters.Contains(Name);
            });
        UnusedParameters.Sort();
        OutError = MakeBindingError(FString::Printf(
            TEXT("The following bound filter parameters are not used by the expression: %s. Remove them or add matching placeholders."),
            *FString::Join(UnusedParameters, TEXT(", "))));
        return false;
    }
    if (Bound.Len() > 64 * 1024)
    {
        OutError = MakeBindingError(FString::Printf(
            TEXT("The bound filter is %d characters, but the maximum is 65536. Shorten the expression or its values."),
            Bound.Len()));
        return false;
    }

    OutFilter = DynamicRaw(MoveTemp(Bound));
    return true;
}
