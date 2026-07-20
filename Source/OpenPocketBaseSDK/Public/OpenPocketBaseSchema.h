#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "OpenPocketBaseSchema.generated.h"

UENUM(BlueprintType)
enum class EOpenPocketBaseCollectionType : uint8
{
    Base,
    Auth,
    View,
    Unknown
};

UENUM(BlueprintType)
enum class EOpenPocketBaseFieldType : uint8
{
    Text,
    Number,
    Boolean,
    Email,
    Url,
    Editor,
    Date,
    Autodate,
    Select,
    File,
    Relation,
    Json,
    Password,
    GeoPoint,
    Unknown
};

UENUM(BlueprintType)
enum class EOpenPocketBaseFieldValueType : uint8
{
    String,
    Number,
    Boolean,
    DateTime,
    StringArray,
    Json,
    GeoPoint,
    Unknown
};

UENUM(BlueprintType)
enum class EOpenPocketBaseFieldStorage : uint8
{
    Data,
    RecordId,
    CollectionId,
    CollectionName,
    Created,
    Updated
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseSchemaField
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString Id;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString Name;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    EOpenPocketBaseFieldType Type = EOpenPocketBaseFieldType::Unknown;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bMultiple = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bRequired = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bSystem = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bHidden = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bReadOnly = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    EOpenPocketBaseFieldStorage Storage = EOpenPocketBaseFieldStorage::Data;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bHasMin = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    double Min = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bHasMax = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    double Max = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString Pattern;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    TArray<FString> Choices;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    TArray<FString> MimeTypes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    int64 MaxSizeBytes = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    int32 MinSelect = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    int32 MaxSelect = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString RelatedCollectionId;

    EOpenPocketBaseFieldValueType GetValueType() const;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseSchemaCollection
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString Id;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString Name;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    EOpenPocketBaseCollectionType Type = EOpenPocketBaseCollectionType::Unknown;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bSystem = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    TArray<FOpenPocketBaseSchemaField> Fields;

    const FOpenPocketBaseSchemaField* FindField(const FString& IdOrName) const;
};

class UOpenPocketBaseSchema;

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseCollectionRef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    TSoftObjectPtr<UOpenPocketBaseSchema> Schema;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FGuid SchemaId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString CollectionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString Name;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    EOpenPocketBaseCollectionType Type = EOpenPocketBaseCollectionType::Unknown;

    bool IsSet() const;
    bool ResolveCurrent(FOpenPocketBaseCollectionRef& OutRef) const;

    template <typename CollectionRefType>
    bool ResolveCurrentAs(CollectionRefType& OutRef) const
    {
        OutRef = {};
        FOpenPocketBaseCollectionRef Current;
        if (!ResolveCurrent(Current) || !CollectionRefType::Accepts(Current))
        {
            return false;
        }

        static_cast<FOpenPocketBaseCollectionRef&>(OutRef) = MoveTemp(Current);
        return true;
    }
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseAuthCollectionRef : public FOpenPocketBaseCollectionRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseCollectionRef& Collection);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseWritableCollectionRef : public FOpenPocketBaseCollectionRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseCollectionRef& Collection);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    TSoftObjectPtr<UOpenPocketBaseSchema> Schema;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FGuid SchemaId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString CollectionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString FieldId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString Name;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    EOpenPocketBaseFieldType Type = EOpenPocketBaseFieldType::Unknown;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bMultiple = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bReadOnly = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    EOpenPocketBaseFieldStorage Storage = EOpenPocketBaseFieldStorage::Data;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bHasMin = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    double Min = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    bool bHasMax = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    double Max = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString Pattern;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    TArray<FString> Choices;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    TArray<FString> MimeTypes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    int64 MaxSizeBytes = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    int32 MinSelect = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    int32 MaxSelect = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString RelatedCollectionId;

    bool IsSet() const;
    bool ResolveCurrent(FOpenPocketBaseFieldRef& OutRef) const;

    template <typename FieldRefType>
    bool ResolveCurrentAs(FieldRefType& OutRef) const
    {
        OutRef = {};
        FOpenPocketBaseFieldRef Current;
        if (!ResolveCurrent(Current) || !FieldRefType::Accepts(Current))
        {
            return false;
        }

        static_cast<FOpenPocketBaseFieldRef&>(OutRef) = MoveTemp(Current);
        return true;
    }

    EOpenPocketBaseFieldValueType GetValueType() const;
    bool BelongsTo(const FOpenPocketBaseCollectionRef& Collection) const;
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseAnyFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseStringFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseTextFieldRef : public FOpenPocketBaseStringFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseNumberFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseBooleanFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseDateFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseStringArrayFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseJsonFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseGeoPointFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseSingleSelectFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseMultipleSelectFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseRelationFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseSingleRelationFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseMultipleRelationFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

USTRUCT(BlueprintType)
struct OPENPOCKETBASESDK_API FOpenPocketBaseFileFieldRef : public FOpenPocketBaseFieldRef
{
    GENERATED_BODY()

    static bool Accepts(const FOpenPocketBaseFieldRef& Field);
};

UCLASS(BlueprintType)
class OPENPOCKETBASESDK_API UOpenPocketBaseSchema final : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FGuid SchemaId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString PocketBaseVersion;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString Source;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    FString Fingerprint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open PocketBase|Schema")
    TArray<FOpenPocketBaseSchemaCollection> Collections;

#if WITH_EDITORONLY_DATA
    UPROPERTY(
        EditAnywhere,
        Category = "Open PocketBase|C++",
        meta = (ToolTip = "C++ namespace used by generated schema accessors, for example MyGame::PocketBase."))
    FString GeneratedCppNamespace = TEXT("PocketBase");

    UPROPERTY(
        EditAnywhere,
        Category = "Open PocketBase|C++",
        meta = (ToolTip = "Project-relative output header. Leave empty to use the primary game module's Generated folder."))
    FString GeneratedHeaderPath;
#endif

    const FOpenPocketBaseSchemaCollection* FindCollection(const FString& IdOrName) const;
    bool MakeCollectionRef(const FString& IdOrName, FOpenPocketBaseCollectionRef& OutRef) const;

    template <typename CollectionRefType>
    bool MakeTypedCollectionRef(
        const FString& IdOrName,
        CollectionRefType& OutRef) const
    {
        OutRef = {};
        FOpenPocketBaseCollectionRef Collection;
        if (!MakeCollectionRef(IdOrName, Collection) || !CollectionRefType::Accepts(Collection))
        {
            return false;
        }

        static_cast<FOpenPocketBaseCollectionRef&>(OutRef) = MoveTemp(Collection);
        return true;
    }

    bool MakeFieldRef(
        const FOpenPocketBaseCollectionRef& Collection,
        const FString& IdOrName,
        FOpenPocketBaseFieldRef& OutRef) const;
    bool ResolveCollection(
        const FOpenPocketBaseCollectionRef& Ref,
        const FOpenPocketBaseSchemaCollection*& OutCollection) const;
    bool ResolveField(
        const FOpenPocketBaseFieldRef& Ref,
        const FOpenPocketBaseSchemaField*& OutField) const;

    template <typename FieldRefType>
    bool MakeTypedFieldRef(
        const FOpenPocketBaseCollectionRef& Collection,
        const FString& IdOrName,
        FieldRefType& OutRef) const
    {
        OutRef = {};
        FOpenPocketBaseFieldRef Field;
        if (!MakeFieldRef(Collection, IdOrName, Field) || !FieldRefType::Accepts(Field))
        {
            return false;
        }

        static_cast<FOpenPocketBaseFieldRef&>(OutRef) = MoveTemp(Field);
        return true;
    }
};
