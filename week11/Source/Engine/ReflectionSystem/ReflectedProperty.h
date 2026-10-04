#pragma once

// ============================================================
//  ReflectedProperty.h
//  FProperty 및 모든 파생 Property 클래스 선언.
//
//  구현은 ReflectedProperty.cpp 에 있습니다.
//  템플릿 클래스(TNumericProperty, TScriptArrayOps)는
//  헤더에만 존재할 수 있으므로 그대로 유지합니다.
// ============================================================

#include "Core/CoreMinimal.h"
#include "Object/FName.h"
#include "Serialization/Archive.h"
#include "ReflectionTypeInfo.h" // FClassInfo, FStructInfo, FEnumInfo, EPropertyFlags

#include <cstring>
#include <type_traits>
#include <algorithm>

class UObject;

// ------------------------------------------------------------------
// EReflectedPropertyKind
// ------------------------------------------------------------------
enum class EReflectedPropertyKind : uint8
{
    Numeric,
    Bool,
    Name,
    String,
    Text,
    Object,
    Class,
    SoftObject,
    SoftClass,
    WeakObject,
    LazyObject,
    Interface,
    Array,
    Map,
    Set,
    Struct,
    Enum,
    Delegate,
    MulticastDelegate,
    MulticastInlineDelegate,
};

// ------------------------------------------------------------------
// GC 참조 수집 인터페이스
// ------------------------------------------------------------------
struct FReferenceCollector
{
    virtual ~FReferenceCollector() = default;
    virtual void AddReferencedObject(UObject*& Object) = 0;
};

// ------------------------------------------------------------------
// 에디터 UI 드로우 컨텍스트
// ------------------------------------------------------------------
struct FEditorPropertyDrawContext
{
    virtual ~FEditorPropertyDrawContext() = default;

    virtual void DrawBool(const char* Label, bool* Value, bool bEditable) {}
    virtual void DrawInteger(const char* Label, int64* Value, bool bEditable) {}
    virtual void DrawUnsignedInteger(const char* Label, uint64* Value, bool bEditable) {}
    virtual void DrawFloat(const char* Label, double* Value, bool bEditable) {}
    virtual void DrawName(const char* Label, FName* Value, bool bEditable) {}
    virtual void DrawString(const char* Label, FString* Value, bool bEditable) {}
    virtual void DrawObject(const char* Label, UObject** Value, FClassInfo* RequiredClass, bool bEditable) {}
    virtual void DrawClass(const char* Label, FClassInfo** Value, FClassInfo* RequiredBaseClass, bool bEditable) {}
    virtual void DrawSoftObjectPath(const char* Label, FString* AssetPath, FClassInfo* RequiredClass, bool bEditable) {}
    virtual void BeginArray(const char* Label, int32 Num, bool bEditable) {}
    virtual void EndArray() {}
    virtual void BeginArrayElement(int32 Index) {}
    virtual void EndArrayElement() {}
    virtual void BeginStruct(const char* Label, FStructInfo* StructInfo, bool bEditable) {}
    virtual void EndStruct() {}
    virtual void DrawEnum(const char* Label, int64* Value, FEnumInfo* EnumInfo, bool bEditable) {}
    virtual void DrawUnsupported(const char* Label, const char* TypeName) {}
};

// ------------------------------------------------------------------
// Soft / Weak / Lazy 포인터 헬퍼 구조체
// ------------------------------------------------------------------
struct FSoftObjectPtr
{
    FString AssetPath;
    UObject* CachedObject = nullptr;

    UObject* Get() const { return CachedObject; }
    void ResetCache() { CachedObject = nullptr; }
};

struct FSoftClassPtr
{
    FString ClassPath;
    FClassInfo* CachedClass = nullptr;

    FClassInfo* Get() const { return CachedClass; }
    void ResetCache() { CachedClass = nullptr; }
};

struct FWeakObjectPtr
{
    uint32 ObjectId = 0;
    uint32 SerialNumber = 0;
    UObject* CachedObject = nullptr;

    UObject* Get() const { return CachedObject; }
    bool IsValid() const { return CachedObject != nullptr; }
};

struct FLazyObjectPtr
{
    FGuid ObjectGuid;
    UObject* CachedObject = nullptr;

    UObject* Get() const { return CachedObject; }
    bool IsValid() const { return CachedObject != nullptr; }
};

struct FScriptInterface
{
    UObject* Object = nullptr;
    void* InterfacePtr = nullptr;
};

// ------------------------------------------------------------------
// FProperty  (기반 클래스)
// ------------------------------------------------------------------
class FProperty
{
public:
    FName Name;
    FString CPPType;
    size_t Offset = 0;
    size_t ElementSize = 0;
    int32 ArrayDim = 1;
    uint32 Flags = PF_None;
    FString Category = "Default";
    FString DisplayName;
    mutable FString CachedNameString;

    virtual ~FProperty() = default;

    virtual EReflectedPropertyKind GetKind() const = 0;

    // 구현: ReflectedProperty.cpp
    const char* GetLabel() const;

    void* ContainerPtrToValuePtr(void* Container, int32 ArrayIndex = 0) const;
    const void* ContainerPtrToValuePtr(const void* Container, int32 ArrayIndex = 0) const;

    bool HasAnyFlags(uint32 InFlags) const { return (Flags & InFlags) != 0; }
    bool IsEditorVisible() const;
    bool IsEditorEditable() const;
    bool ShouldSerialize() const;

    virtual void SerializeItem(FArchive& Ar, void* Value) const = 0;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const = 0;
    virtual void CollectReferences(FReferenceCollector& Collector, void* Value) const {}
    virtual void InitializeValue(void* Value) const {}
    virtual void DestroyValue(void* Value) const {}
    virtual void CopyValue(void* Dest, const void* Src) const;

    // 구현: ReflectedProperty.cpp
    void SerializeInContainer(FArchive& Ar, void* Container, int32 ArrayIndex = 0) const;
    void DrawEditorInContainer(FEditorPropertyDrawContext& Context, void* Container, int32 ArrayIndex = 0) const;
    void CollectReferencesInContainer(FReferenceCollector& Collector, void* Container, int32 ArrayIndex = 0) const;
};

// ------------------------------------------------------------------
// FNumericProperty  +  TNumericProperty<T>
// ------------------------------------------------------------------
class FNumericProperty : public FProperty
{
public:
    virtual bool IsInteger() const = 0;
    virtual bool IsFloatingPoint() const = 0;
    virtual bool IsSigned() const = 0;

    virtual int64 GetSignedInt(const void* Value) const = 0;
    virtual uint64 GetUnsignedInt(const void* Value) const = 0;
    virtual double GetFloatingPoint(const void* Value) const = 0;

    virtual void SetSignedInt(void* Value, int64 NewValue) const = 0;
    virtual void SetUnsignedInt(void* Value, uint64 NewValue) const = 0;
    virtual void SetFloatingPoint(void* Value, double NewValue) const = 0;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Numeric; }
};

// 템플릿이므로 헤더에 구현 유지
template <typename T>
class TNumericProperty : public FNumericProperty
{
public:
    TNumericProperty() { ElementSize = sizeof(T); }

    virtual bool IsInteger() const override { return std::is_integral_v<T>; }
    virtual bool IsFloatingPoint() const override { return std::is_floating_point_v<T>; }
    virtual bool IsSigned() const override { return std::is_signed_v<T>; }

    virtual int64 GetSignedInt(const void* Value) const override { return static_cast<int64>(*static_cast<const T*>(Value)); }
    virtual uint64 GetUnsignedInt(const void* Value) const override { return static_cast<uint64>(*static_cast<const T*>(Value)); }
    virtual double GetFloatingPoint(const void* Value) const override { return static_cast<double>(*static_cast<const T*>(Value)); }

    virtual void SetSignedInt(void* Value, int64 NewValue) const override { *static_cast<T*>(Value) = static_cast<T>(NewValue); }
    virtual void SetUnsignedInt(void* Value, uint64 NewValue) const override { *static_cast<T*>(Value) = static_cast<T>(NewValue); }
    virtual void SetFloatingPoint(void* Value, double NewValue) const override { *static_cast<T*>(Value) = static_cast<T>(NewValue); }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        SerializeNumericValue(Ar, *static_cast<T*>(Value));
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            double Temp = GetFloatingPoint(Value);
            Context.DrawFloat(GetLabel(), &Temp, IsEditorEditable());
            if (IsEditorEditable())
                SetFloatingPoint(Value, Temp);
        }
        else if constexpr (std::is_signed_v<T>)
        {
            int64 Temp = GetSignedInt(Value);
            Context.DrawInteger(GetLabel(), &Temp, IsEditorEditable());
            if (IsEditorEditable())
                SetSignedInt(Value, Temp);
        }
        else
        {
            uint64 Temp = GetUnsignedInt(Value);
            Context.DrawUnsignedInteger(GetLabel(), &Temp, IsEditorEditable());
            if (IsEditorEditable())
                SetUnsignedInt(Value, Temp);
        }
    }

private:
    static void SerializeNumericValue(FArchive& Ar, int32& Value) { Ar << Value; }
    static void SerializeNumericValue(FArchive& Ar, uint32& Value) { Ar << Value; }
    static void SerializeNumericValue(FArchive& Ar, float& Value) { Ar << Value; }

    template <typename U>
    static void SerializeNumericValue(FArchive& Ar, U& Value)
    {
        Ar.Serialize(&Value, static_cast<uint32>(sizeof(U)));
    }
};

// --- Numeric 구체 타입 ---
class FByteProperty : public TNumericProperty<uint8>
{
};
class FInt8Property : public TNumericProperty<int8>
{
};
class FInt16Property : public TNumericProperty<int16>
{
};
class FIntProperty : public TNumericProperty<int32>
{
};
class FInt64Property : public TNumericProperty<int64>
{
};
class FUInt16Property : public TNumericProperty<uint16>
{
};
class FUInt32Property : public TNumericProperty<uint32>
{
};
class FUInt64Property : public TNumericProperty<uint64>
{
};
class FFloatProperty : public TNumericProperty<float>
{
};
class FDoubleProperty : public TNumericProperty<double>
{
};

// ------------------------------------------------------------------
// FBoolProperty
// ------------------------------------------------------------------
class FBoolProperty : public FProperty
{
public:
    FBoolProperty() { ElementSize = sizeof(bool); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Bool; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

// ------------------------------------------------------------------
// FNameProperty
// ------------------------------------------------------------------
class FNameProperty : public FProperty
{
public:
    FNameProperty() { ElementSize = sizeof(FName); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Name; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

// ------------------------------------------------------------------
// FStrProperty / FTextProperty
// ------------------------------------------------------------------
class FStrProperty : public FProperty
{
public:
    FStrProperty() { ElementSize = sizeof(FString); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::String; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

class FTextProperty : public FStrProperty
{
public:
    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Text; }
};

// ------------------------------------------------------------------
// FObjectPropertyBase / FObjectProperty
// ------------------------------------------------------------------
enum class EObjectReferenceKind
{
    ObjectId,
    AssetPath
};

class FObjectPropertyBase : public FProperty
{
public:
    FClassInfo* PropertyClass = nullptr;
    EObjectReferenceKind ReferenceKind = EObjectReferenceKind::ObjectId;

    FObjectPropertyBase() { ElementSize = sizeof(UObject*); }

    UObject* GetObjectPropertyValue(void* Value) const;
    void SetObjectPropertyValue(void* Value, UObject* Object) const;

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

class FObjectProperty : public FObjectPropertyBase
{
public:
    mutable uint32 PendingObjectId = 0;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Object; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void CollectReferences(FReferenceCollector& Collector, void* Value) const override;

protected:
    FString GetObjectAssetPath(UObject* Object);
    UObject* LoadObjectFromAssetPath(const FString& Path);

    virtual uint32 GetObjectId(UObject* Object) const
    {
        (void)Object;
        return 0;
    }
    virtual UObject* ResolveObject(uint32 ObjectId) const
    {
        (void)ObjectId;
        return nullptr;
    }
};

// ------------------------------------------------------------------
// FClassProperty
// ------------------------------------------------------------------
class FClassProperty : public FProperty
{
public:
    FClassInfo* MetaClass = nullptr;

    FClassProperty() { ElementSize = sizeof(FClassInfo*); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Class; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;

protected:
    virtual FClassInfo* ResolveClass(const FString& ClassName) const
    {
        (void)ClassName;
        return nullptr;
    }
};

// ------------------------------------------------------------------
// FSoftObjectProperty / FSoftClassProperty
// ------------------------------------------------------------------
class FSoftObjectProperty : public FProperty
{
public:
    FClassInfo* PropertyClass = nullptr;

    FSoftObjectProperty() { ElementSize = sizeof(FSoftObjectPtr); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::SoftObject; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

class FSoftClassProperty : public FProperty
{
public:
    FClassInfo* MetaClass = nullptr;

    FSoftClassProperty() { ElementSize = sizeof(FSoftClassPtr); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::SoftClass; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

// ------------------------------------------------------------------
// FWeakObjectProperty / FLazyObjectProperty
// ------------------------------------------------------------------
class FWeakObjectProperty : public FProperty
{
public:
    FClassInfo* PropertyClass = nullptr;

    FWeakObjectProperty() { ElementSize = sizeof(FWeakObjectPtr); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::WeakObject; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

class FLazyObjectProperty : public FProperty
{
public:
    FClassInfo* PropertyClass = nullptr;

    FLazyObjectProperty() { ElementSize = sizeof(FLazyObjectPtr); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::LazyObject; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

// ------------------------------------------------------------------
// FInterfaceProperty
// ------------------------------------------------------------------
class FInterfaceProperty : public FProperty
{
public:
    FClassInfo* InterfaceClass = nullptr;

    FInterfaceProperty() { ElementSize = sizeof(FScriptInterface); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Interface; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
    virtual void CollectReferences(FReferenceCollector& Collector, void* Value) const override;
};

// ------------------------------------------------------------------
// FScriptArrayOps  +  TScriptArrayOps<T>
// ------------------------------------------------------------------
struct FScriptArrayOps
{
    int32 (*GetNum)(void* Array) = nullptr;
    void (*Resize)(void* Array, int32 NewNum) = nullptr;
    void* (*GetElementPtr)(void* Array, int32 Index) = nullptr;
};

// 템플릿이므로 헤더에 구현 유지
template <typename T>
struct TScriptArrayOps
{
    static int32 GetNum(void* Array)
    {
        return static_cast<int32>(static_cast<TArray<T>*>(Array)->size());
    }

    static void Resize(void* Array, int32 NewNum)
    {
        static_cast<TArray<T>*>(Array)->resize(NewNum);
    }

    static void* GetElementPtr(void* Array, int32 Index)
    {
        return &(*static_cast<TArray<T>*>(Array))[Index];
    }

    static FScriptArrayOps Make()
    {
        return { &GetNum, &Resize, &GetElementPtr };
    }
};

// ------------------------------------------------------------------
// FArrayProperty
// ------------------------------------------------------------------
class FArrayProperty : public FProperty
{
public:
    FProperty* Inner = nullptr;
    FScriptArrayOps ArrayOps;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Array; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
    virtual void CollectReferences(FReferenceCollector& Collector, void* Value) const override;
};

// ------------------------------------------------------------------
// FMapProperty / FSetProperty
// ------------------------------------------------------------------
class FMapProperty : public FProperty
{
public:
    FProperty* KeyProp = nullptr;
    FProperty* ValueProp = nullptr;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Map; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

class FSetProperty : public FProperty
{
public:
    FProperty* ElementProp = nullptr;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Set; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

// ------------------------------------------------------------------
// FStructProperty
// ------------------------------------------------------------------
class FStructProperty : public FProperty
{
public:
    FStructInfo* StructInfo = nullptr;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Struct; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
    virtual void CollectReferences(FReferenceCollector& Collector, void* Value) const override;

private:
    static void SerializeLegacyStructField(FArchive& Ar, const FString& TypeName, void* Value);
};

// ------------------------------------------------------------------
// FEnumProperty
// ------------------------------------------------------------------
class FEnumProperty : public FProperty
{
public:
    FEnumInfo* EnumInfo = nullptr;
    FNumericProperty* UnderlyingProp = nullptr;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Enum; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

// ------------------------------------------------------------------
// FDelegateProperty / FMulticastDelegateProperty / Inline variant
// ------------------------------------------------------------------
class FDelegateProperty : public FProperty
{
public:
    FFunctionInfo* SignatureFunction = nullptr;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Delegate; }
    virtual void SerializeItem(FArchive& Ar, void* Value) const override;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override;
};

class FMulticastDelegateProperty : public FDelegateProperty
{
public:
    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::MulticastDelegate; }
};

class FMulticastInlineDelegateProperty : public FMulticastDelegateProperty
{
public:
    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::MulticastInlineDelegate; }
};