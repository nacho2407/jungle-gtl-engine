#pragma once

// ============================================================
//  ReflectedProperty.h
//  FProperty 및 모든 파생 Property 클래스 정의.
//
//  include 순서 (단방향, 순환 없음):
//    ReflectionFwd.h          (전방선언)
//    ReflectionTypeInfo.h     (FClassInfo / FStructInfo / FEnumInfo 완전 정의)
//    ReflectedProperty.h      ← 여기서 FProperty 계층 정의
// ============================================================

#include "CoreMinimal.h"
#include "Object/FName.h"
#include "Serialization/Archive.h"
#include "ReflectionTypeInfo.h"  // FClassInfo, FStructInfo, FEnumInfo, EPropertyFlags

#include <cstring>
#include <type_traits>
#include <algorithm>   // std::remove

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
    FString  AssetPath;
    UObject* CachedObject = nullptr;

    UObject* Get() const   { return CachedObject; }
    void ResetCache()      { CachedObject = nullptr; }
};

struct FSoftClassPtr
{
    FString      ClassPath;
    FClassInfo*  CachedClass = nullptr;

    FClassInfo* Get() const { return CachedClass; }
    void ResetCache()       { CachedClass = nullptr; }
};

struct FWeakObjectPtr
{
    uint32   ObjectId      = 0;
    uint32   SerialNumber  = 0;
    UObject* CachedObject  = nullptr;

    UObject* Get() const  { return CachedObject; }
    bool IsValid() const  { return CachedObject != nullptr; }
};

struct FLazyObjectPtr
{
    FGuid    ObjectGuid;
    UObject* CachedObject = nullptr;

    UObject* Get() const  { return CachedObject; }
    bool IsValid() const  { return CachedObject != nullptr; }
};

struct FScriptInterface
{
    UObject* Object       = nullptr;
    void*    InterfacePtr = nullptr;
};

// ------------------------------------------------------------------
// FProperty  (기반 클래스)
// ------------------------------------------------------------------
class FProperty
{
public:
    FName    Name;
    FString  CPPType;
    size_t   Offset      = 0;
    size_t   ElementSize = 0;
    int32    ArrayDim    = 1;
    uint32   Flags       = PF_None;
    FString  Category    = "Default";
    FString  DisplayName;
    mutable FString CachedNameString;

    virtual ~FProperty() = default;

    virtual EReflectedPropertyKind GetKind() const = 0;

    const char* GetLabel() const
    {
        if (!DisplayName.empty())
            return DisplayName.c_str();
        CachedNameString = Name.ToString();
        return CachedNameString.c_str();
    }

    void* ContainerPtrToValuePtr(void* Container, int32 ArrayIndex = 0) const
    {
        return static_cast<uint8*>(Container) + Offset + ElementSize * ArrayIndex;
    }

    const void* ContainerPtrToValuePtr(const void* Container, int32 ArrayIndex = 0) const
    {
        return static_cast<const uint8*>(Container) + Offset + ElementSize * ArrayIndex;
    }

    bool HasAnyFlags(uint32 InFlags) const { return (Flags & InFlags) != 0; }

    bool IsEditorVisible() const
    {
        return !HasAnyFlags(EPropertyFlags::PF_HideInEditor) &&
                HasAnyFlags(EPropertyFlags::PF_EditAnywhere | EPropertyFlags::PF_VisibleAnywhere);
    }

    bool IsEditorEditable() const
    {
        return !HasAnyFlags(EPropertyFlags::PF_HideInEditor) &&
                HasAnyFlags(EPropertyFlags::PF_EditAnywhere);
    }

    bool ShouldSerialize() const
    {
        return !HasAnyFlags(EPropertyFlags::PF_Transient) &&
                HasAnyFlags(EPropertyFlags::PF_SaveGame | EPropertyFlags::PF_EditAnywhere);
    }

    virtual void SerializeItem(FArchive& Ar, void* Value) const = 0;
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const = 0;
    virtual void CollectReferences(FReferenceCollector& Collector, void* Value) const {}
    virtual void InitializeValue(void* Value) const {}
    virtual void DestroyValue(void* Value) const {}
    virtual void CopyValue(void* Dest, const void* Src) const
    {
        if (ElementSize > 0)
            std::memcpy(Dest, Src, ElementSize);
    }

    void SerializeInContainer(FArchive& Ar, void* Container, int32 ArrayIndex = 0) const
    {
        if (!ShouldSerialize())
            return;

        const FString Key = Name.ToString();

        if (Ar.IsLoading() && !Ar.HasKey(Key))
            return;

        Ar.SetCurrentKey(Key);
        SerializeItem(Ar, ContainerPtrToValuePtr(Container, ArrayIndex));

        if (Ar.IsSaving() && Ar.GetCurrentKey() == Key)
        {
            Ar.SetCurrentKey("");
        }
    }

    void DrawEditorInContainer(FEditorPropertyDrawContext& Context, void* Container, int32 ArrayIndex = 0) const
    {
        if (IsEditorVisible())
            DrawEditor(Context, ContainerPtrToValuePtr(Container, ArrayIndex));
    }

    void CollectReferencesInContainer(FReferenceCollector& Collector, void* Container, int32 ArrayIndex = 0) const
    {
        CollectReferences(Collector, ContainerPtrToValuePtr(Container, ArrayIndex));
    }
};

// ------------------------------------------------------------------
// FNumericProperty  +  TNumericProperty<T>
// ------------------------------------------------------------------
class FNumericProperty : public FProperty
{
public:
    virtual bool IsInteger()       const = 0;
    virtual bool IsFloatingPoint() const = 0;
    virtual bool IsSigned()        const = 0;

    virtual int64  GetSignedInt(const void* Value)      const = 0;
    virtual uint64 GetUnsignedInt(const void* Value)    const = 0;
    virtual double GetFloatingPoint(const void* Value)  const = 0;

    virtual void SetSignedInt(void* Value, int64 NewValue)      const = 0;
    virtual void SetUnsignedInt(void* Value, uint64 NewValue)   const = 0;
    virtual void SetFloatingPoint(void* Value, double NewValue) const = 0;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Numeric; }
};

template <typename T>
class TNumericProperty : public FNumericProperty
{
public:
    TNumericProperty() { ElementSize = sizeof(T); }

    virtual bool IsInteger()       const override { return std::is_integral_v<T>; }
    virtual bool IsFloatingPoint() const override { return std::is_floating_point_v<T>; }
    virtual bool IsSigned()        const override { return std::is_signed_v<T>; }

    virtual int64  GetSignedInt(const void* Value)     const override { return static_cast<int64>(*static_cast<const T*>(Value)); }
    virtual uint64 GetUnsignedInt(const void* Value)   const override { return static_cast<uint64>(*static_cast<const T*>(Value)); }
    virtual double GetFloatingPoint(const void* Value) const override { return static_cast<double>(*static_cast<const T*>(Value)); }

    virtual void SetSignedInt(void* Value, int64 NewValue)      const override { *static_cast<T*>(Value) = static_cast<T>(NewValue); }
    virtual void SetUnsignedInt(void* Value, uint64 NewValue)   const override { *static_cast<T*>(Value) = static_cast<T>(NewValue); }
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
            if (IsEditorEditable()) SetFloatingPoint(Value, Temp);
        }
        else if constexpr (std::is_signed_v<T>)
        {
            int64 Temp = GetSignedInt(Value);
            Context.DrawInteger(GetLabel(), &Temp, IsEditorEditable());
            if (IsEditorEditable()) SetSignedInt(Value, Temp);
        }
        else
        {
            uint64 Temp = GetUnsignedInt(Value);
            Context.DrawUnsignedInteger(GetLabel(), &Temp, IsEditorEditable());
            if (IsEditorEditable()) SetUnsignedInt(Value, Temp);
        }
    }

private:
    static void SerializeNumericValue(FArchive& Ar, int32& Value)  { Ar << Value; }
    static void SerializeNumericValue(FArchive& Ar, uint32& Value) { Ar << Value; }
    static void SerializeNumericValue(FArchive& Ar, float& Value)  { Ar << Value; }

    template <typename U>
    static void SerializeNumericValue(FArchive& Ar, U& Value)
    {
        Ar.Serialize(&Value, static_cast<uint32>(sizeof(U)));
    }
};

// --- Numeric 타입들 ---
class FByteProperty   : public TNumericProperty<uint8>  {};
class FInt8Property   : public TNumericProperty<int8>   {};
class FInt16Property  : public TNumericProperty<int16>  {};
class FIntProperty    : public TNumericProperty<int32>  {};
class FInt64Property  : public TNumericProperty<int64>  {};
class FUInt16Property : public TNumericProperty<uint16> {};
class FUInt32Property : public TNumericProperty<uint32> {};
class FUInt64Property : public TNumericProperty<uint64> {};
class FFloatProperty  : public TNumericProperty<float>  {};
class FDoubleProperty : public TNumericProperty<double> {};

// ------------------------------------------------------------------
// FBoolProperty
// ------------------------------------------------------------------
class FBoolProperty : public FProperty
{
public:
    FBoolProperty() { ElementSize = sizeof(bool); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Bool; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        Ar << *static_cast<bool*>(Value);
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        Context.DrawBool(GetLabel(), static_cast<bool*>(Value), IsEditorEditable());
    }
};

// ------------------------------------------------------------------
// FNameProperty
// ------------------------------------------------------------------
class FNameProperty : public FProperty
{
public:
    FNameProperty() { ElementSize = sizeof(FName); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Name; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        Ar << *static_cast<FName*>(Value);
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        Context.DrawName(GetLabel(), static_cast<FName*>(Value), IsEditorEditable());
    }
};

// ------------------------------------------------------------------
// FStrProperty / FTextProperty
// ------------------------------------------------------------------
class FStrProperty : public FProperty
{
public:
    FStrProperty() { ElementSize = sizeof(FString); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::String; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        Ar << *static_cast<FString*>(Value);
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        Context.DrawString(GetLabel(), static_cast<FString*>(Value), IsEditorEditable());
    }
};

class FTextProperty : public FStrProperty
{
public:
    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Text; }
};

// ------------------------------------------------------------------
// FObjectPropertyBase / FObjectProperty
// ------------------------------------------------------------------
class FObjectPropertyBase : public FProperty
{
public:
    FClassInfo* PropertyClass = nullptr;

    FObjectPropertyBase() { ElementSize = sizeof(UObject*); }

    UObject* GetObjectPropertyValue(void* Value) const
    {
        return *static_cast<UObject**>(Value);
    }

    void SetObjectPropertyValue(void* Value, UObject* Object) const
    {
        *static_cast<UObject**>(Value) = Object;
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        Context.DrawObject(GetLabel(), static_cast<UObject**>(Value), PropertyClass, IsEditorEditable());
    }
};

class FObjectProperty : public FObjectPropertyBase
{
public:
    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Object; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        UObject* Object = GetObjectPropertyValue(Value);
        uint32 ObjectId = Object ? GetObjectId(Object) : 0;
        Ar << ObjectId;
        if (Ar.IsLoading())
            SetObjectPropertyValue(Value, ResolveObject(ObjectId));
    }

    virtual void CollectReferences(FReferenceCollector& Collector, void* Value) const override
    {
        UObject*& Object = *static_cast<UObject**>(Value);
        if (Object)
            Collector.AddReferencedObject(Object);
    }

protected:
    virtual uint32   GetObjectId(UObject* Object) const    { (void)Object;   return 0;       }
    virtual UObject* ResolveObject(uint32 ObjectId) const  { (void)ObjectId; return nullptr; }
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

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        FClassInfo*& ClassValue = *static_cast<FClassInfo**>(Value);
        FString ClassName = ClassValue ? ClassValue->ClassName.ToString() : "";
        Ar << ClassName;
        if (Ar.IsLoading())
            ClassValue = ResolveClass(ClassName);
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        Context.DrawClass(GetLabel(), static_cast<FClassInfo**>(Value), MetaClass, IsEditorEditable());
    }

protected:
    virtual FClassInfo* ResolveClass(const FString& ClassName) const { (void)ClassName; return nullptr; }
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

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        FSoftObjectPtr& SoftObject = *static_cast<FSoftObjectPtr*>(Value);
        Ar << SoftObject.AssetPath;
        if (Ar.IsLoading())
            SoftObject.ResetCache();
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        FSoftObjectPtr& SoftObject = *static_cast<FSoftObjectPtr*>(Value);
        Context.DrawSoftObjectPath(GetLabel(), &SoftObject.AssetPath, PropertyClass, IsEditorEditable());
    }
};

class FSoftClassProperty : public FProperty
{
public:
    FClassInfo* MetaClass = nullptr;

    FSoftClassProperty() { ElementSize = sizeof(FSoftClassPtr); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::SoftClass; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        FSoftClassPtr& SoftClass = *static_cast<FSoftClassPtr*>(Value);
        Ar << SoftClass.ClassPath;
        if (Ar.IsLoading())
            SoftClass.ResetCache();
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        FClassInfo* TempClass = static_cast<FSoftClassPtr*>(Value)->Get();
        Context.DrawClass(GetLabel(), &TempClass, MetaClass, IsEditorEditable());
    }
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

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        FWeakObjectPtr& W = *static_cast<FWeakObjectPtr*>(Value);
        Ar << W.ObjectId;
        Ar << W.SerialNumber;
        if (Ar.IsLoading())
            W.CachedObject = nullptr;
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        UObject* Object = static_cast<FWeakObjectPtr*>(Value)->Get();
        Context.DrawObject(GetLabel(), &Object, PropertyClass, false);
    }
};

class FLazyObjectProperty : public FProperty
{
public:
    FClassInfo* PropertyClass = nullptr;

    FLazyObjectProperty() { ElementSize = sizeof(FLazyObjectPtr); }

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::LazyObject; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        FLazyObjectPtr& L = *static_cast<FLazyObjectPtr*>(Value);
        Ar << L.ObjectGuid.A;
        Ar << L.ObjectGuid.B;
        Ar << L.ObjectGuid.C;
        Ar << L.ObjectGuid.D;
        if (Ar.IsLoading())
            L.CachedObject = nullptr;
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        UObject* Object = static_cast<FLazyObjectPtr*>(Value)->Get();
        Context.DrawObject(GetLabel(), &Object, PropertyClass, false);
    }
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

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        FScriptInterface& SI = *static_cast<FScriptInterface*>(Value);
        uint32 ObjectId = 0;
        Ar << ObjectId;
        if (Ar.IsLoading())
        {
            SI.Object       = nullptr;
            SI.InterfacePtr = nullptr;
        }
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        FScriptInterface& SI = *static_cast<FScriptInterface*>(Value);
        Context.DrawObject(GetLabel(), &SI.Object, InterfaceClass, IsEditorEditable());
    }

    virtual void CollectReferences(FReferenceCollector& Collector, void* Value) const override
    {
        UObject*& Object = static_cast<FScriptInterface*>(Value)->Object;
        if (Object)
            Collector.AddReferencedObject(Object);
    }
};

// ------------------------------------------------------------------
// FScriptArrayOps  +  TScriptArrayOps<T>  +  FArrayProperty
// ------------------------------------------------------------------
struct FScriptArrayOps
{
    int32  (*GetNum)(void* Array)                          = nullptr;
    void   (*Resize)(void* Array, int32 NewNum)            = nullptr;
    void*  (*GetElementPtr)(void* Array, int32 Index)      = nullptr;
};

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

class FArrayProperty : public FProperty
{
public:
    FProperty*      Inner    = nullptr;
    FScriptArrayOps ArrayOps;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Array; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        if (!Inner || !ArrayOps.GetNum || !ArrayOps.Resize || !ArrayOps.GetElementPtr)
            return;

        int32 Num = ArrayOps.GetNum(Value);

        FString ArrayKey = Ar.GetCurrentKey();
        Ar.BeginArray(ArrayKey, Num);

        if (Ar.IsLoading())
            ArrayOps.Resize(Value, Num);

        for (int32 Index = 0; Index < Num; ++Index)
        {
            Ar.BeginObject(Index);
            Inner->SerializeItem(Ar, ArrayOps.GetElementPtr(Value, Index));
            Ar.EndObject();
        }

        Ar.EndArray();
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        if (!Inner || !ArrayOps.GetNum || !ArrayOps.GetElementPtr)
        {
            Context.DrawUnsupported(GetLabel(), CPPType.c_str());
            return;
        }

        const int32 Num = ArrayOps.GetNum(Value);
        Context.BeginArray(GetLabel(), Num, IsEditorEditable());
        for (int32 i = 0; i < Num; ++i)
        {
            Context.BeginArrayElement(i);
            Inner->DrawEditor(Context, ArrayOps.GetElementPtr(Value, i));
            Context.EndArrayElement();
        }
        Context.EndArray();
    }

    virtual void CollectReferences(FReferenceCollector& Collector, void* Value) const override
    {
        if (!Inner || !ArrayOps.GetNum || !ArrayOps.GetElementPtr)
            return;

        const int32 Num = ArrayOps.GetNum(Value);
        for (int32 i = 0; i < Num; ++i)
            Inner->CollectReferences(Collector, ArrayOps.GetElementPtr(Value, i));
    }
};

// ------------------------------------------------------------------
// FMapProperty / FSetProperty
// ------------------------------------------------------------------
class FMapProperty : public FProperty
{
public:
    FProperty* KeyProp   = nullptr;
    FProperty* ValueProp = nullptr;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Map; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override { (void)Ar; (void)Value; }
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        (void)Value;
        Context.DrawUnsupported(GetLabel(), CPPType.c_str());
    }
};

class FSetProperty : public FProperty
{
public:
    FProperty* ElementProp = nullptr;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Set; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override { (void)Ar; (void)Value; }
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        (void)Value;
        Context.DrawUnsupported(GetLabel(), CPPType.c_str());
    }
};

// ------------------------------------------------------------------
// FStructProperty
// ------------------------------------------------------------------

class FStructProperty : public FProperty
{
public:
    FStructInfo* StructInfo = nullptr;
    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Struct; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        if (!StructInfo)
            return;

        FString ObjectKey = Ar.GetCurrentKey();
        Ar.BeginObject(ObjectKey);

        for (FProperty* ChildProp : StructInfo->ReflectedProperties)
        {
            if (!ChildProp || !ChildProp->ShouldSerialize())
                continue;

            ChildProp->SerializeInContainer(Ar, Value);
        }

        Ar.EndObject();
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        Context.BeginStruct(GetLabel(), StructInfo, IsEditorEditable());
        Context.EndStruct();
        (void)Value;
    }

private:
    static void SerializeLegacyStructField(FArchive& Ar, const FString& TypeName, void* Value)
    {
        if      (TypeName == "bool")               Ar << *static_cast<bool*>(Value);
        else if (TypeName == "int" ||
                 TypeName == "int32")              Ar << *static_cast<int32*>(Value);
        else if (TypeName == "uint32")             Ar << *static_cast<uint32*>(Value);
        else if (TypeName == "float")              Ar << *static_cast<float*>(Value);
        else if (TypeName == "FString")            Ar << *static_cast<FString*>(Value);
        else if (TypeName == "FName")              Ar << *static_cast<FName*>(Value);
        else if (TypeName == "FVector")            Ar << *static_cast<FVector*>(Value);
        else if (TypeName == "FVector4")           Ar << *static_cast<FVector4*>(Value);
        else if (TypeName == "FColor")             Ar << *static_cast<FColor*>(Value);
    }
};

// ------------------------------------------------------------------
// FEnumProperty
// ------------------------------------------------------------------
class FEnumProperty : public FProperty
{
public:
    FEnumInfo*       EnumInfo       = nullptr;
    FNumericProperty* UnderlyingProp = nullptr;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Enum; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override
    {
        if (UnderlyingProp)
        {
            UnderlyingProp->SerializeItem(Ar, Value);
            return;
        }
        uint32 RawValue = *static_cast<uint32*>(Value);
        Ar << RawValue;
        if (Ar.IsLoading())
            *static_cast<uint32*>(Value) = RawValue;
    }

    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        int64 Temp = UnderlyingProp
            ? UnderlyingProp->GetSignedInt(Value)
            : static_cast<int64>(*static_cast<uint32*>(Value));

        Context.DrawEnum(GetLabel(), &Temp, EnumInfo, IsEditorEditable());

        if (IsEditorEditable())
        {
            if (UnderlyingProp)
                UnderlyingProp->SetSignedInt(Value, Temp);
            else
                *static_cast<uint32*>(Value) = static_cast<uint32>(Temp);
        }
    }
};

// ------------------------------------------------------------------
// FDelegateProperty / FMulticastDelegateProperty / Inline variant
// ------------------------------------------------------------------
class FDelegateProperty : public FProperty
{
public:
    void* SignatureFunction = nullptr;

    virtual EReflectedPropertyKind GetKind() const override { return EReflectedPropertyKind::Delegate; }

    virtual void SerializeItem(FArchive& Ar, void* Value) const override { (void)Ar; (void)Value; }
    virtual void DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const override
    {
        (void)Value;
        Context.DrawUnsupported(GetLabel(), CPPType.c_str());
    }
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
