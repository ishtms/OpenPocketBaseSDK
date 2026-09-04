// Copyright 2026 Ishtmeet Singh.

#pragma once

#include "Misc/Optional.h"
#include "OpenPocketBaseError.h"
#include "Templates/UnrealTemplate.h"

template <typename ValueType>
class TOpenPocketBaseResult
{
public:
    static TOpenPocketBaseResult Success(ValueType Value)
    {
        TOpenPocketBaseResult Result;
        Result.Value.Emplace(MoveTemp(Value));
        return Result;
    }

    static TOpenPocketBaseResult Failure(FOpenPocketBaseError Error)
    {
        TOpenPocketBaseResult Result;
        Result.Error = MoveTemp(Error);
        return Result;
    }

    bool IsSuccess() const
    {
        return Value.IsSet();
    }

    const ValueType& GetValue() const
    {
        check(Value.IsSet());
        return Value.GetValue();
    }

    ValueType& GetValue()
    {
        check(Value.IsSet());
        return Value.GetValue();
    }

    ValueType TakeValue()
    {
        check(Value.IsSet());
        return MoveTemp(Value.GetValue());
    }

    const FOpenPocketBaseError& GetError() const
    {
        check(!Value.IsSet());
        return Error;
    }

private:
    TOptional<ValueType> Value;
    FOpenPocketBaseError Error;
};
