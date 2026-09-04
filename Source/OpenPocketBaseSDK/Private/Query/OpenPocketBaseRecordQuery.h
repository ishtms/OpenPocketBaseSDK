// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "CoreMinimal.h"
#include "OpenPocketBaseQuery.h"

namespace OpenPocketBase::Internal
{
inline FString MakeRecordFieldsQuery(const TArray<FString>& SelectedFields)
{
    if (SelectedFields.IsEmpty())
    {
        return {};
    }

    TArray<FString> Fields = {
        TEXT("id"),
        TEXT("collectionId"),
        TEXT("collectionName"),
        TEXT("created"),
        TEXT("updated")};
    Fields.Reserve(Fields.Num() + SelectedFields.Num());
    for (const FString& Field : SelectedFields)
    {
        if (!Field.IsEmpty() && !Fields.Contains(Field))
        {
            Fields.Add(Field);
        }
    }
    return FString::Join(Fields, TEXT(","));
}

inline FString MakeRecordFieldsQuery(const TArray<FOpenPocketBaseFieldSelection>& SelectedFields)
{
    TArray<FString> QueryValues;
    QueryValues.Reserve(SelectedFields.Num());
    for (const FOpenPocketBaseFieldSelection& Field : SelectedFields)
    {
        QueryValues.Add(Field.ToQueryValue());
    }
    return MakeRecordFieldsQuery(QueryValues);
}
}
