// ============================================================
//  ReflectedProperty.cpp
//  FProperty 계층의 비템플릿 메서드 구현.
// ============================================================

#include "ReflectedProperty.h"
#include "ReflectionDatabase.h" // ReflectionDatabase::GetClass
#include "Core/ResourceManager.h"   // FResourceManager::Get()
#include "Object/Object.h"     // UObjectManager::Get()

// Cast 헬퍼 (엔진 전방선언 포함)
#include "Asset/StaticMesh.h"
#include "Render/Resource/Texture.h"
#include "Render/Resource/Material.h"

// ================================================================
//  FProperty
// ================================================================

const char* FProperty::GetLabel() const
{
    if (!DisplayName.empty())
        return DisplayName.c_str();
    CachedNameString = Name.ToString();
    return CachedNameString.c_str();
}

void* FProperty::ContainerPtrToValuePtr(void* Container, int32 ArrayIndex) const
{
    return static_cast<uint8*>(Container) + Offset + ElementSize * ArrayIndex;
}

const void* FProperty::ContainerPtrToValuePtr(const void* Container, int32 ArrayIndex) const
{
    return static_cast<const uint8*>(Container) + Offset + ElementSize * ArrayIndex;
}

bool FProperty::IsEditorVisible() const
{
    return !HasAnyFlags(EPropertyFlags::PF_HideInEditor) &&
           HasAnyFlags(EPropertyFlags::PF_EditAnywhere | EPropertyFlags::PF_VisibleAnywhere);
}

bool FProperty::IsEditorEditable() const
{
    return !HasAnyFlags(EPropertyFlags::PF_HideInEditor) &&
           HasAnyFlags(EPropertyFlags::PF_EditAnywhere);
}

bool FProperty::ShouldSerialize() const
{
    return !HasAnyFlags(EPropertyFlags::PF_Transient) &&
           HasAnyFlags(EPropertyFlags::PF_SaveGame | EPropertyFlags::PF_EditAnywhere);
}

void FProperty::CopyValue(void* Dest, const void* Src) const
{
    if (ElementSize > 0)
        std::memcpy(Dest, Src, ElementSize);
}

void FProperty::SerializeInContainer(FArchive& Ar, void* Container, int32 ArrayIndex) const
{
    if (!ShouldSerialize())
        return;

    const FString Key = Name.ToString();

    if (Ar.IsLoading() && !Ar.HasKey(Key))
        return;

    Ar.SetCurrentKey(Key);
    SerializeItem(Ar, ContainerPtrToValuePtr(Container, ArrayIndex));

    if (Ar.IsSaving() && Ar.GetCurrentKey() == Key)
        Ar.SetCurrentKey("");
}

void FProperty::DrawEditorInContainer(FEditorPropertyDrawContext& Context, void* Container, int32 ArrayIndex) const
{
    if (IsEditorVisible())
        DrawEditor(Context, ContainerPtrToValuePtr(Container, ArrayIndex));
}

void FProperty::CollectReferencesInContainer(FReferenceCollector& Collector, void* Container, int32 ArrayIndex) const
{
    CollectReferences(Collector, ContainerPtrToValuePtr(Container, ArrayIndex));
}

// ================================================================
//  FBoolProperty
// ================================================================

void FBoolProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    Ar << *static_cast<bool*>(Value);
}

void FBoolProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    Context.DrawBool(GetLabel(), static_cast<bool*>(Value), IsEditorEditable());
}

// ================================================================
//  FNameProperty
// ================================================================

void FNameProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    Ar << *static_cast<FName*>(Value);
}

void FNameProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    Context.DrawName(GetLabel(), static_cast<FName*>(Value), IsEditorEditable());
}

// ================================================================
//  FStrProperty
// ================================================================

void FStrProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    Ar << *static_cast<FString*>(Value);
}

void FStrProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    Context.DrawString(GetLabel(), static_cast<FString*>(Value), IsEditorEditable());
}

// ================================================================
//  FObjectPropertyBase
// ================================================================

UObject* FObjectPropertyBase::GetObjectPropertyValue(void* Value) const
{
    return *static_cast<UObject**>(Value);
}

void FObjectPropertyBase::SetObjectPropertyValue(void* Value, UObject* Object) const
{
    *static_cast<UObject**>(Value) = Object;
}

void FObjectPropertyBase::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    Context.DrawObject(GetLabel(), static_cast<UObject**>(Value), PropertyClass, IsEditorEditable());
}

// ================================================================
//  FObjectProperty
// ================================================================

FString FObjectProperty::GetObjectAssetPath(UObject* Object)
{
    if (!Object)
        return "";

    if (UStaticMesh* Mesh = Cast<UStaticMesh>(Object))
        return Mesh->GetAssetPathFileName();

    if (UTexture* Texture = Cast<UTexture>(Object))
        return Texture->GetFilePath();

    if (UMaterialInterface* Material = Cast<UMaterialInterface>(Object))
        return Material->GetFilePath();

    return "";
}

UObject* FObjectProperty::LoadObjectFromAssetPath(const FString& Path)
{
    if (Path.empty())
        return nullptr;

    if (PropertyClass == ReflectionDatabase::GetClass("UStaticMesh"))
        return FResourceManager::Get().LoadStaticMesh(Path);

    if (PropertyClass == ReflectionDatabase::GetClass("USkeletalMesh"))
        return FResourceManager::Get().LoadSkeletalMesh(Path);

    if (PropertyClass == ReflectionDatabase::GetClass("UTexture"))
        return FResourceManager::Get().LoadTexture(Path);

    if (PropertyClass == ReflectionDatabase::GetClass("UMaterial") ||
        PropertyClass == ReflectionDatabase::GetClass("UMaterialInterface") ||
        PropertyClass == ReflectionDatabase::GetClass("UMaterialInstance"))
    {
        return FResourceManager::Get().GetMaterialInterface(Path);
    }

    return nullptr;
}

void FObjectProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    UObject*& Object = *static_cast<UObject**>(Value);

    if (ReferenceKind == EObjectReferenceKind::AssetPath)
    {
        FString AssetPath;
        if (Ar.IsSaving())
            AssetPath = Object ? const_cast<FObjectProperty*>(this)->GetObjectAssetPath(Object) : "";

        Ar << AssetPath;

        if (Ar.IsLoading())
            Object = const_cast<FObjectProperty*>(this)->LoadObjectFromAssetPath(AssetPath);

        return;
    }

    if (ReferenceKind == EObjectReferenceKind::ObjectId)
    {
        uint32 ObjectId = (Ar.IsSaving() && Object) ? Object->GetUUID() : 0;
        Ar << ObjectId;

        if (Ar.IsLoading())
        {
            PendingObjectId = ObjectId;
            Object = UObjectManager::Get().FindByUUID(ObjectId);
        }

        return;
    }

    // 폴백: raw ObjectId 직렬화
    uint32 ObjectId = 0;
    if (Ar.IsSaving())
        ObjectId = Object ? Object->GetUUID() : 0;

    Ar << ObjectId;

    if (Ar.IsLoading())
        Object = UObjectManager::Get().FindByUUID(ObjectId);
}

void FObjectProperty::CollectReferences(FReferenceCollector& Collector, void* Value) const
{
    UObject*& Object = *static_cast<UObject**>(Value);
    if (Object)
        Collector.AddReferencedObject(Object);
}

// ================================================================
//  FClassProperty
// ================================================================

void FClassProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    FClassInfo*& ClassValue = *static_cast<FClassInfo**>(Value);
    FString ClassName = ClassValue ? ClassValue->ClassName.ToString() : "";
    Ar << ClassName;
    if (Ar.IsLoading())
        ClassValue = ResolveClass(ClassName);
}

void FClassProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    Context.DrawClass(GetLabel(), static_cast<FClassInfo**>(Value), MetaClass, IsEditorEditable());
}

// ================================================================
//  FSoftObjectProperty
// ================================================================

void FSoftObjectProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    FSoftObjectPtr& SoftObject = *static_cast<FSoftObjectPtr*>(Value);
    Ar << SoftObject.AssetPath;
    if (Ar.IsLoading())
        SoftObject.ResetCache();
}

void FSoftObjectProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    FSoftObjectPtr& SoftObject = *static_cast<FSoftObjectPtr*>(Value);
    Context.DrawSoftObjectPath(GetLabel(), &SoftObject.AssetPath, PropertyClass, IsEditorEditable());
}

// ================================================================
//  FSoftClassProperty
// ================================================================

void FSoftClassProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    FSoftClassPtr& SoftClass = *static_cast<FSoftClassPtr*>(Value);
    Ar << SoftClass.ClassPath;
    if (Ar.IsLoading())
        SoftClass.ResetCache();
}

void FSoftClassProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    FClassInfo* TempClass = static_cast<FSoftClassPtr*>(Value)->Get();
    Context.DrawClass(GetLabel(), &TempClass, MetaClass, IsEditorEditable());
}

// ================================================================
//  FWeakObjectProperty
// ================================================================

void FWeakObjectProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    FWeakObjectPtr& W = *static_cast<FWeakObjectPtr*>(Value);
    Ar << W.ObjectId;
    Ar << W.SerialNumber;
    if (Ar.IsLoading())
        W.CachedObject = nullptr;
}

void FWeakObjectProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    UObject* Object = static_cast<FWeakObjectPtr*>(Value)->Get();
    Context.DrawObject(GetLabel(), &Object, PropertyClass, false);
}

// ================================================================
//  FLazyObjectProperty
// ================================================================

void FLazyObjectProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    FLazyObjectPtr& L = *static_cast<FLazyObjectPtr*>(Value);
    Ar << L.ObjectGuid.A;
    Ar << L.ObjectGuid.B;
    Ar << L.ObjectGuid.C;
    Ar << L.ObjectGuid.D;
    if (Ar.IsLoading())
        L.CachedObject = nullptr;
}

void FLazyObjectProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    UObject* Object = static_cast<FLazyObjectPtr*>(Value)->Get();
    Context.DrawObject(GetLabel(), &Object, PropertyClass, false);
}

// ================================================================
//  FInterfaceProperty
// ================================================================

void FInterfaceProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    FScriptInterface& SI = *static_cast<FScriptInterface*>(Value);
    uint32 ObjectId = 0;
    Ar << ObjectId;
    if (Ar.IsLoading())
    {
        SI.Object = nullptr;
        SI.InterfacePtr = nullptr;
    }
}

void FInterfaceProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    FScriptInterface& SI = *static_cast<FScriptInterface*>(Value);
    Context.DrawObject(GetLabel(), &SI.Object, InterfaceClass, IsEditorEditable());
}

void FInterfaceProperty::CollectReferences(FReferenceCollector& Collector, void* Value) const
{
    UObject*& Object = static_cast<FScriptInterface*>(Value)->Object;
    if (Object)
        Collector.AddReferencedObject(Object);
}

// ================================================================
//  FArrayProperty
// ================================================================

void FArrayProperty::SerializeItem(FArchive& Ar, void* Value) const
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

void FArrayProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
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

void FArrayProperty::CollectReferences(FReferenceCollector& Collector, void* Value) const
{
    if (!Inner || !ArrayOps.GetNum || !ArrayOps.GetElementPtr)
        return;

    const int32 Num = ArrayOps.GetNum(Value);
    for (int32 i = 0; i < Num; ++i)
        Inner->CollectReferences(Collector, ArrayOps.GetElementPtr(Value, i));
}

// ================================================================
//  FMapProperty
// ================================================================

void FMapProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    (void)Ar;
    (void)Value;
}

void FMapProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    (void)Value;
    Context.DrawUnsupported(GetLabel(), CPPType.c_str());
}

// ================================================================
//  FSetProperty
// ================================================================

void FSetProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    (void)Ar;
    (void)Value;
}

void FSetProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    (void)Value;
    Context.DrawUnsupported(GetLabel(), CPPType.c_str());
}

// ================================================================
//  FStructProperty
// ================================================================

void FStructProperty::SerializeItem(FArchive& Ar, void* Value) const
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

void FStructProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    Context.BeginStruct(GetLabel(), StructInfo, IsEditorEditable());
    Context.EndStruct();
    (void)Value;
}

void FStructProperty::CollectReferences(FReferenceCollector& Collector, void* Value) const
{
    if (!StructInfo)
        return;

    for (FProperty* ChildProp : StructInfo->ReflectedProperties)
    {
        if (ChildProp)
            ChildProp->CollectReferencesInContainer(Collector, Value);
    }
}

void FStructProperty::SerializeLegacyStructField(FArchive& Ar, const FString& TypeName, void* Value)
{
    if (TypeName == "bool")
        Ar << *static_cast<bool*>(Value);
    else if (TypeName == "int" ||
             TypeName == "int32")
        Ar << *static_cast<int32*>(Value);
    else if (TypeName == "uint32")
        Ar << *static_cast<uint32*>(Value);
    else if (TypeName == "float")
        Ar << *static_cast<float*>(Value);
    else if (TypeName == "FString")
        Ar << *static_cast<FString*>(Value);
    else if (TypeName == "FName")
        Ar << *static_cast<FName*>(Value);
    else if (TypeName == "FVector")
        Ar << *static_cast<FVector*>(Value);
    else if (TypeName == "FVector4")
        Ar << *static_cast<FVector4*>(Value);
    else if (TypeName == "FColor")
        Ar << *static_cast<FColor*>(Value);
}

// ================================================================
//  FEnumProperty
// ================================================================

void FEnumProperty::SerializeItem(FArchive& Ar, void* Value) const
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

void FEnumProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
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

// ================================================================
//  FDelegateProperty
// ================================================================

void FDelegateProperty::SerializeItem(FArchive& Ar, void* Value) const
{
    (void)Ar;
    (void)Value;
}

void FDelegateProperty::DrawEditor(FEditorPropertyDrawContext& Context, void* Value) const
{
    (void)Value;
    Context.DrawUnsupported(GetLabel(), CPPType.c_str());
}