#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "EdGraphSchema_K2.h"
#include "OpenPocketBaseCustomRoute.h"
#include "OpenPocketBaseStringLibrary.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectIterator.h"

namespace
{
FEdGraphPinType MakeStringPinType()
{
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_String;
    return Type;
}

FEdGraphPinType MakeStructPinType(UScriptStruct* Struct)
{
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
    Type.PinSubCategoryObject = Struct;
    return Type;
}

FEdGraphPinType MakeEnumPinType(UEnum* Enum)
{
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Byte;
    Type.PinSubCategoryObject = Enum;
    return Type;
}

template <typename Type, typename PinTypeFactory>
void FindMissingAutocasts(
    const UEdGraphSchema_K2& Schema,
    const FName PackageName,
    const FString& ExpectedOwner,
    PinTypeFactory MakePinType,
    TArray<FString>& OutTypeNames,
    TArray<FString>& OutMissing)
{
    const FEdGraphPinType StringType = MakeStringPinType();
    for (TObjectIterator<Type> It; It; ++It)
    {
        Type* ReflectedType = *It;
        if (ReflectedType->GetOutermost()->GetFName() != PackageName ||
            !ReflectedType->GetName().Contains(TEXT("OpenPocketBase")))
        {
            continue;
        }

        OutTypeNames.Add(ReflectedType->GetName());
        const TOptional<UEdGraphSchema_K2::FSearchForAutocastFunctionResults> Conversion =
            Schema.SearchForAutocastFunction(MakePinType(ReflectedType), StringType);
        if (!Conversion.IsSet() ||
            Conversion->FunctionOwner == nullptr ||
            Conversion->FunctionOwner->GetName() != ExpectedOwner)
        {
            OutMissing.Add(ReflectedType->GetName());
        }
    }
}

FString InvokeHealthResultConversion(const FOpenPocketBaseHealthResult& HealthResult)
{
    UClass* Library = FindObject<UClass>(
        nullptr,
        TEXT("/Script/OpenPocketBaseSDK.OpenPocketBaseStringLibrary"));
    if (Library == nullptr)
    {
        return FString();
    }

    UFunction* Function = Library->FindFunctionByName(
        TEXT("Conv_OpenPocketBaseHealthResultToString"));
    if (Function == nullptr)
    {
        return FString();
    }

    FStructProperty* InputProperty = nullptr;
    FStrProperty* ReturnProperty = nullptr;
    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        if (It->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            ReturnProperty = CastField<FStrProperty>(*It);
        }
        else if (It->HasAnyPropertyFlags(CPF_Parm))
        {
            InputProperty = CastField<FStructProperty>(*It);
        }
    }
    if (InputProperty == nullptr || ReturnProperty == nullptr)
    {
        return FString();
    }

    FStructOnScope Parameters(Function);
    InputProperty->Struct->CopyScriptStruct(
        InputProperty->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()),
        &HealthResult);
    Library->GetDefaultObject()->ProcessEvent(Function, Parameters.GetStructMemory());
    return ReturnProperty->GetPropertyValue_InContainer(Parameters.GetStructMemory());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseStringAutocastTest,
    "OpenPocketBase.Blueprint.AllDataTypesAutocastToString",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseStringAutocastTest::RunTest(const FString& Parameters)
{
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    if (!TestNotNull(TEXT("The Blueprint schema is available"), Schema))
    {
        return false;
    }

    TArray<FString> CoreStructs;
    TArray<FString> CoreEnums;
    TArray<FString> AdminStructs;
    TArray<FString> Missing;
    FindMissingAutocasts<UScriptStruct>(
        *Schema,
        TEXT("/Script/OpenPocketBaseSDK"),
        TEXT("OpenPocketBaseStringLibrary"),
        MakeStructPinType,
        CoreStructs,
        Missing);
    FindMissingAutocasts<UEnum>(
        *Schema,
        TEXT("/Script/OpenPocketBaseSDK"),
        TEXT("OpenPocketBaseStringLibrary"),
        MakeEnumPinType,
        CoreEnums,
        Missing);
    FindMissingAutocasts<UScriptStruct>(
        *Schema,
        TEXT("/Script/OpenPocketBaseSDKAdmin"),
        TEXT("OpenPocketBaseAdminStringLibrary"),
        MakeStructPinType,
        AdminStructs,
        Missing);

    TestEqual(TEXT("Every core struct is covered"), CoreStructs.Num(), 50);
    TestEqual(TEXT("Every core enum is covered"), CoreEnums.Num(), 19);
    TestEqual(TEXT("Every admin struct is covered"), AdminStructs.Num(), 11);
    TestTrue(
        *FString::Printf(
            TEXT("Every SDK data type autocasts to String. Missing: %s"),
            *FString::Join(Missing, TEXT(", "))),
        Missing.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseDebugStringFormattingTest,
    "OpenPocketBase.Blueprint.DataTypeStringsAreReadable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseDebugStringFormattingTest::RunTest(const FString& Parameters)
{
    FOpenPocketBaseHealthResult HealthResult;
    HealthResult.bHealthy = true;
    HealthResult.HttpStatus = 200;
    HealthResult.Code = 200;
    HealthResult.Message = TEXT("API is healthy.");
    HealthResult.DurationSeconds = 0.001;

    const FString Output = InvokeHealthResultConversion(HealthResult);
    TestTrue(TEXT("The type name is included"), Output.Contains(TEXT("Health Result")));
    TestTrue(TEXT("Boolean fields are readable"), Output.Contains(TEXT("\"healthy\": true")));
    TestTrue(TEXT("Status fields are readable"), Output.Contains(TEXT("\"httpStatus\": 200")));
    TestTrue(TEXT("Messages are readable"), Output.Contains(TEXT("API is healthy.")));
    TestTrue(TEXT("The output uses multiple lines"), Output.Contains(TEXT("\n")));

    FOpenPocketBaseClientConfig Config;
    Config.DefaultHeaders.Add(TEXT("Authorization"), TEXT("secret-value"));
    const FString ConfigOutput =
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseClientConfigToString(Config);
    TestFalse(TEXT("Sensitive values are not printed"), ConfigOutput.Contains(TEXT("secret-value")));
    TestTrue(TEXT("Sensitive values are marked as redacted"), ConfigOutput.Contains(TEXT("<redacted>")));

    FOpenPocketBaseFileDownloadResult Download;
    Download.Bytes.SetNumZeroed(32);
    const FString DownloadOutput =
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseFileDownloadResultToString(Download);
    TestTrue(TEXT("Byte arrays are summarized"), DownloadOutput.Contains(TEXT("<32 bytes>")));

    TestEqual(
        TEXT("Enums use their readable names"),
        UOpenPocketBaseStringLibrary::Conv_OpenPocketBaseErrorKindToString(
            EOpenPocketBaseErrorKind::Transport),
        FString(TEXT("Transport")));
    return true;
}

#endif
