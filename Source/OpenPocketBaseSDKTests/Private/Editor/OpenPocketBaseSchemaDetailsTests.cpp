#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "OpenPocketBaseSchema.h"
#include "PropertyEditorModule.h"
#include "PropertyPath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpenPocketBaseSchemaDetailsLayoutTest,
    "OpenPocketBase.Editor.SchemaDetailsShowsPropertiesOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenPocketBaseSchemaDetailsLayoutTest::RunTest(const FString& Parameters)
{
    FPropertyEditorModule& PropertyEditor =
        FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    const TSharedRef<IDetailsView> DetailsView = PropertyEditor.CreateDetailView(FDetailsViewArgs());
    DetailsView->SetObject(NewObject<UOpenPocketBaseSchema>());

    const TArray<FPropertyPath> DisplayedProperties = DetailsView->GetPropertiesInOrderDisplayed();
    for (const FString PropertyName : {
             FString(TEXT("SchemaId")),
             FString(TEXT("PocketBaseVersion")),
             FString(TEXT("Source")),
             FString(TEXT("Fingerprint")),
             FString(TEXT("Collections"))})
    {
        int32 MatchCount = 0;
        for (const FPropertyPath& PropertyPath : DisplayedProperties)
        {
            MatchCount += PropertyPath.ToString().Equals(PropertyName) ? 1 : 0;
        }

        TestEqual(*FString::Printf(TEXT("%s appears once"), *PropertyName), MatchCount, 1);
    }

    return true;
}

#endif
