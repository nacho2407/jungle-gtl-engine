#include "Object/Property.h"

#include "Animation/AnimSequence.h"
#include "Asset/SkeletalMesh.h"
#include "Asset/StaticMesh.h"
#include "Core/Paths.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/ResourceManager.h"
#include "Object/Class.h"
#include "Object/Object.h"
#include "Render/Resource/Material.h"
#include "Serialization/Archive.h"

#include <cmath>
#include <cstring>
#include <filesystem>

namespace
{
	bool IsValueChannel(const FString& ChannelName);
	bool IsObjectAssignableTo(UObject* Object, UClass* ExpectedClass);
	bool IsClassChildOf(UClass* Class, UClass* ExpectedClass);
	FString GetAssetObjectPath(UObject* Object);
	UObject* ResolveAssetObject(const FString& Path, UClass* ExpectedClass);

	template <typename T>
	bool CopyTypedValue(void* DstValuePtr, const void* SrcValuePtr);

	void SerializeEnumValue(FArchive& Ar, void* ValuePtr, uint8 Size);
	void SerializeGuidValue(FArchive& Ar, void* ValuePtr);
	void SerializeQuatValue(FArchive& Ar, void* ValuePtr);
	void SerializeObjectPtrValue(FArchive& Ar, const FProperty& Property, void* ValuePtr);
	void SerializeSoftObjectPtrValue(FArchive& Ar, const FProperty& Property, void* ValuePtr);
	void SerializeArrayValue(FArchive& Ar, const FProperty& Property, void* ValuePtr);

	bool CopyEnumValue(const FProperty& Property, void* DstValuePtr, const void* SrcValuePtr);
	bool CopyObjectPtrValue(const FProperty& Property, void* DstValuePtr, const void* SrcValuePtr, const FDuplicateContext* Context);
	bool CopySoftObjectPtrValue(const FProperty& Property, void* DstValuePtr, const void* SrcValuePtr);
	bool CopyArrayValue(const FProperty& Property, void* DstValuePtr, const void* SrcValuePtr, const FDuplicateContext* Context);
}

void* FProperty::GetValuePtr(UObject* Container) const
{
	return Container ? reinterpret_cast<uint8*>(Container) + Offset : nullptr;
}

const void* FProperty::GetValuePtr(const UObject* Container) const
{
	return Container ? reinterpret_cast<const uint8*>(Container) + Offset : nullptr;
}

void FProperty::SerializeItem(FArchive& Ar, UObject* Container) const
{
	if (!Container || !Name || IsTransient())
	{
		return;
	}

	if (Ar.IsLoading() && !Ar.HasKey(Name))
	{
		return;
	}

	void* ValuePtr = GetValuePtr(Container);
	if (!ValuePtr)
	{
		return;
	}

	Ar << Name;
	SerializeValue(Ar, ValuePtr);
}

void FProperty::SerializeValue(FArchive& Ar, void* ValuePtr) const
{
	if (!ValuePtr)
	{
		return;
	}

	switch (Type)
	{
	case EPropertyType::Bool:
		Ar << *static_cast<bool*>(ValuePtr);
		break;
	case EPropertyType::Int:
		Ar << *static_cast<int32*>(ValuePtr);
		break;
	case EPropertyType::Float:
		Ar << *static_cast<float*>(ValuePtr);
		break;
	case EPropertyType::Vec3:
		Ar << *static_cast<FVector*>(ValuePtr);
		break;
	case EPropertyType::Vec4:
		Ar << *static_cast<FVector4*>(ValuePtr);
		break;
	case EPropertyType::String:
		Ar << *static_cast<FString*>(ValuePtr);
		break;
	case EPropertyType::Name:
		Ar << *static_cast<FName*>(ValuePtr);
		break;
	case EPropertyType::Color:
		Ar << *static_cast<FColor*>(ValuePtr);
		break;
	case EPropertyType::Guid:
		SerializeGuidValue(Ar, ValuePtr);
		break;
	case EPropertyType::Quat:
		SerializeQuatValue(Ar, ValuePtr);
		break;
	case EPropertyType::Enum:
		if (EnumMeta)
		{
			SerializeEnumValue(Ar, ValuePtr, EnumMeta->Size);
		}
		break;
	case EPropertyType::ObjectPtr:
		SerializeObjectPtrValue(Ar, *this, ValuePtr);
		break;
	case EPropertyType::SoftObjectPtr:
		SerializeSoftObjectPtrValue(Ar, *this, ValuePtr);
		break;
	case EPropertyType::Array:
		SerializeArrayValue(Ar, *this, ValuePtr);
		break;
	case EPropertyType::Struct:
	case EPropertyType::Unknown:
	default:
		break;
	}
}

bool FProperty::CopyValue(void* DstValuePtr, const void* SrcValuePtr, const FDuplicateContext* Context) const
{
	if (!DstValuePtr || !SrcValuePtr)
	{
		return false;
	}

	switch (Type)
	{
	case EPropertyType::Bool:
		return CopyTypedValue<bool>(DstValuePtr, SrcValuePtr);
	case EPropertyType::Int:
		return CopyTypedValue<int32>(DstValuePtr, SrcValuePtr);
	case EPropertyType::Float:
		return CopyTypedValue<float>(DstValuePtr, SrcValuePtr);
	case EPropertyType::Vec3:
		return CopyTypedValue<FVector>(DstValuePtr, SrcValuePtr);
	case EPropertyType::Vec4:
		return CopyTypedValue<FVector4>(DstValuePtr, SrcValuePtr);
	case EPropertyType::Color:
		return CopyTypedValue<FColor>(DstValuePtr, SrcValuePtr);
	case EPropertyType::Guid:
		return CopyTypedValue<FGuid>(DstValuePtr, SrcValuePtr);
	case EPropertyType::Quat:
		return CopyTypedValue<FQuat>(DstValuePtr, SrcValuePtr);
	case EPropertyType::String:
		return CopyTypedValue<FString>(DstValuePtr, SrcValuePtr);
	case EPropertyType::Name:
		return CopyTypedValue<FName>(DstValuePtr, SrcValuePtr);
	case EPropertyType::Enum:
		return CopyEnumValue(*this, DstValuePtr, SrcValuePtr);
	case EPropertyType::ObjectPtr:
		return CopyObjectPtrValue(*this, DstValuePtr, SrcValuePtr, Context);
	case EPropertyType::SoftObjectPtr:
		return CopySoftObjectPtrValue(*this, DstValuePtr, SrcValuePtr);
	case EPropertyType::Array:
		return CopyArrayValue(*this, DstValuePtr, SrcValuePtr, Context);
	case EPropertyType::Struct:
		if (Size > 0)
		{
			std::memcpy(DstValuePtr, SrcValuePtr, Size);
			return true;
		}
		return false;
	case EPropertyType::Unknown:
	default:
		return false;
	}
}

bool FProperty::CopyValue(UObject* DstContainer, const UObject* SrcContainer, const FDuplicateContext* Context) const
{
	if (!DstContainer || !SrcContainer || !Name || IsTransient())
	{
		return false;
	}

	return CopyValue(GetValuePtr(DstContainer), GetValuePtr(SrcContainer), Context);
}

void FProperty::VisitReferences(FReferenceCollector& Collector, void* ValuePtr) const
{
	if (!ValuePtr)
	{
		return;
	}

	switch (Type)
	{
	case EPropertyType::ObjectPtr:
		if (ReferenceKind != EObjectReferenceKind::Asset && ObjectPtrOps)
		{
			ObjectPtrOps->VisitReference(Collector, ValuePtr);
		}
		break;
	case EPropertyType::Array:
		if (ArrayOps && InnerProperty)
		{
			const int32 Count = ArrayOps->Num(ValuePtr);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				InnerProperty->VisitReferences(Collector, ArrayOps->GetElementPtr(ValuePtr, Index));
			}
		}
		break;
	default:
		break;
	}
}

void FProperty::VisitSoftReferences(FSoftReferenceCollector& Collector, void* ValuePtr) const
{
	if (!ValuePtr)
	{
		return;
	}

	switch (Type)
	{
	case EPropertyType::SoftObjectPtr:
		if (SoftObjectOps)
		{
			SoftObjectOps->VisitSoftReference(Collector, ValuePtr, ObjectClass);
		}
		break;
	case EPropertyType::ObjectPtr:
		if (ReferenceKind == EObjectReferenceKind::Asset && ObjectPtrOps)
		{
			Collector.AddSoftObjectPath(GetAssetObjectPath(ObjectPtrOps->GetObject(ValuePtr)), ObjectClass);
		}
		break;
	case EPropertyType::Array:
		if (ArrayOps && InnerProperty)
		{
			const int32 Count = ArrayOps->Num(ValuePtr);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				InnerProperty->VisitSoftReferences(Collector, ArrayOps->GetElementPtr(ValuePtr, Index));
			}
		}
		break;
	default:
		break;
	}
}

bool FProperty::IsSequencerScalar() const
{
	if (!Name || !HasPropertyFlag(Flags, EPropertyFlags::Animatable))
	{
		return false;
	}

	return Type == EPropertyType::Bool
		|| Type == EPropertyType::Int
		|| Type == EPropertyType::Float
		|| Type == EPropertyType::Vec3
		|| Type == EPropertyType::Vec4
		|| Type == EPropertyType::Color;
}

bool FProperty::ReadScalarChannelValue(const UObject* Container, const FString& ChannelName, float& OutValue) const
{
	if (!Container || !Name)
	{
		return false;
	}

	if (Type == EPropertyType::Bool && IsValueChannel(ChannelName))
	{
		const bool* Value = ContainerPtrToValuePtr<bool>(Container);
		if (!Value) return false;
		OutValue = *Value ? 1.0f : 0.0f;
		return true;
	}

	if (Type == EPropertyType::Int && IsValueChannel(ChannelName))
	{
		const int32* Value = ContainerPtrToValuePtr<int32>(Container);
		if (!Value) return false;
		OutValue = static_cast<float>(*Value);
		return true;
	}

	if (Type == EPropertyType::Float && IsValueChannel(ChannelName))
	{
		const float* Value = ContainerPtrToValuePtr<float>(Container);
		if (!Value) return false;
		OutValue = *Value;
		return true;
	}

	if (Type == EPropertyType::Vec3)
	{
		const FVector* Value = ContainerPtrToValuePtr<FVector>(Container);
		if (!Value) return false;
		if (ChannelName == "X") { OutValue = Value->X; return true; }
		if (ChannelName == "Y") { OutValue = Value->Y; return true; }
		if (ChannelName == "Z") { OutValue = Value->Z; return true; }
	}

	if (Type == EPropertyType::Color)
	{
		const FColor* Value = ContainerPtrToValuePtr<FColor>(Container);
		if (!Value) return false;
		if (ChannelName == "R") { OutValue = Value->R; return true; }
		if (ChannelName == "G") { OutValue = Value->G; return true; }
		if (ChannelName == "B") { OutValue = Value->B; return true; }
		if (ChannelName == "A") { OutValue = Value->A; return true; }
	}

	if (Type == EPropertyType::Vec4)
	{
		const FVector4* Value = ContainerPtrToValuePtr<FVector4>(Container);
		if (!Value) return false;
		if (ChannelName == "X") { OutValue = Value->X; return true; }
		if (ChannelName == "Y") { OutValue = Value->Y; return true; }
		if (ChannelName == "Z") { OutValue = Value->Z; return true; }
		if (ChannelName == "W") { OutValue = Value->W; return true; }
	}

	return false;
}

bool FProperty::WriteScalarChannelValue(UObject* Container, const FString& ChannelName, float NewValue) const
{
	if (!Container || !Name || !HasPropertyFlag(Flags, EPropertyFlags::Write))
	{
		return false;
	}

	if (Type == EPropertyType::Bool && IsValueChannel(ChannelName))
	{
		bool* Value = ContainerPtrToValuePtr<bool>(Container);
		if (!Value) return false;
		*Value = NewValue >= 0.5f;
		return true;
	}

	if (Type == EPropertyType::Int && IsValueChannel(ChannelName))
	{
		int32* Value = ContainerPtrToValuePtr<int32>(Container);
		if (!Value) return false;
		*Value = static_cast<int32>(std::round(NewValue));
		return true;
	}

	if (Type == EPropertyType::Float && IsValueChannel(ChannelName))
	{
		float* Value = ContainerPtrToValuePtr<float>(Container);
		if (!Value) return false;
		*Value = NewValue;
		return true;
	}

	if (Type == EPropertyType::Vec3)
	{
		FVector* Value = ContainerPtrToValuePtr<FVector>(Container);
		if (!Value) return false;
		if (ChannelName == "X") { Value->X = NewValue; return true; }
		if (ChannelName == "Y") { Value->Y = NewValue; return true; }
		if (ChannelName == "Z") { Value->Z = NewValue; return true; }
	}

	if (Type == EPropertyType::Color)
	{
		FColor* Value = ContainerPtrToValuePtr<FColor>(Container);
		if (!Value) return false;
		if (ChannelName == "R") { Value->R = NewValue; return true; }
		if (ChannelName == "G") { Value->G = NewValue; return true; }
		if (ChannelName == "B") { Value->B = NewValue; return true; }
		if (ChannelName == "A") { Value->A = NewValue; return true; }
	}

	if (Type == EPropertyType::Vec4)
	{
		FVector4* Value = ContainerPtrToValuePtr<FVector4>(Container);
		if (!Value) return false;
		if (ChannelName == "X") { Value->X = NewValue; return true; }
		if (ChannelName == "Y") { Value->Y = NewValue; return true; }
		if (ChannelName == "Z") { Value->Z = NewValue; return true; }
		if (ChannelName == "W") { Value->W = NewValue; return true; }
	}

	return false;
}

namespace
{
	bool IsValueChannel(const FString& ChannelName)
	{
		return ChannelName.empty() || ChannelName == "Value";
	}

	bool IsObjectAssignableTo(UObject* Object, UClass* ExpectedClass)
	{
		return !Object || !ExpectedClass || Object->IsA(ExpectedClass);
	}

	bool IsClassChildOf(UClass* Class, UClass* ExpectedClass)
	{
		return Class && ExpectedClass && Class->IsChildOf(ExpectedClass);
	}

	FString GetAssetObjectPath(UObject* Object)
	{
		if (!Object)
		{
			return {};
		}

		if (UMaterialInterface* Material = Cast<UMaterialInterface>(Object))
		{
			const FString FilePath = FPaths::Normalize(Material->GetFilePath());
			return FilePath.empty() ? Material->GetName() : FilePath;
		}

		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
		{
			return FPaths::Normalize(StaticMesh->GetAssetPathFileName());
		}

		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
		{
			return FPaths::Normalize(SkeletalMesh->GetAssetPathFileName());
		}

		if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(Object))
		{
			return FPaths::Normalize(AnimSequence->GetAssetPath());
		}

		return Object->GetName();
	}

	UObject* ResolveAssetObject(const FString& Path, UClass* ExpectedClass)
	{
		if (Path.empty() || !ExpectedClass)
		{
			return nullptr;
		}

		if (IsClassChildOf(ExpectedClass, UMaterialInterface::StaticClass()))
		{
			return FResourceManager::Get().GetMaterialInterface(Path);
		}

		if (IsClassChildOf(ExpectedClass, UStaticMesh::StaticClass()))
		{
			return FResourceManager::Get().LoadStaticMesh(Path);
		}

		if (IsClassChildOf(ExpectedClass, USkeletalMesh::StaticClass()))
		{
			return FResourceManager::Get().LoadSkeletalMesh(Path);
		}

		if (IsClassChildOf(ExpectedClass, UAnimationAsset::StaticClass()))
		{
			return FResourceManager::Get().LoadAnimSequence(Path);
		}

		return nullptr;
	}

	template <typename T>
	bool CopyTypedValue(void* DstValuePtr, const void* SrcValuePtr)
	{
		T* Dst = static_cast<T*>(DstValuePtr);
		const T* Src = static_cast<const T*>(SrcValuePtr);
		if (!Dst || !Src)
		{
			return false;
		}
		*Dst = *Src;
		return true;
	}

	void SerializeEnumValue(FArchive& Ar, void* ValuePtr, uint8 Size)
	{
		if (!ValuePtr)
		{
			return;
		}

		int32 Value = 0;
		if (!Ar.IsLoading())
		{
			switch (Size)
			{
			case 1: Value = static_cast<int32>(*static_cast<uint8*>(ValuePtr)); break;
			case 2: Value = static_cast<int32>(*static_cast<uint16*>(ValuePtr)); break;
			case 4: Value = static_cast<int32>(*static_cast<int32*>(ValuePtr)); break;
			case 8: Value = static_cast<int32>(*static_cast<int64*>(ValuePtr)); break;
			default: break;
			}
		}

		Ar << Value;

		if (Ar.IsLoading())
		{
			switch (Size)
			{
			case 1: *static_cast<uint8*>(ValuePtr) = static_cast<uint8>(Value); break;
			case 2: *static_cast<uint16*>(ValuePtr) = static_cast<uint16>(Value); break;
			case 4: *static_cast<int32*>(ValuePtr) = static_cast<int32>(Value); break;
			case 8: *static_cast<int64*>(ValuePtr) = static_cast<int64>(Value); break;
			default: break;
			}
		}
	}

	void SerializeGuidValue(FArchive& Ar, void* ValuePtr)
	{
		FGuid* Guid = static_cast<FGuid*>(ValuePtr);
		FString Text = Guid ? Guid->ToString() : FString();
		Ar << Text;
		if (Ar.IsLoading() && Guid)
		{
			*Guid = FGuid::FromString(Text);
		}
	}

	void SerializeQuatValue(FArchive& Ar, void* ValuePtr)
	{
		FQuat* Quat = static_cast<FQuat*>(ValuePtr);
		if (!Quat)
		{
			return;
		}

		FVector4 Components(Quat->X, Quat->Y, Quat->Z, Quat->W);
		Ar << Components;
		if (Ar.IsLoading())
		{
			*Quat = FQuat(Components.X, Components.Y, Components.Z, Components.W);
			Quat->Normalize();
		}
	}

	void SerializeObjectPtrValue(FArchive& Ar, const FProperty& Property, void* ValuePtr)
	{
		if (!Property.ObjectPtrOps)
		{
			return;
		}

		if (Property.ReferenceKind == EObjectReferenceKind::Asset)
		{
			FString Path;
			if (Ar.IsSaving())
			{
				Path = GetAssetObjectPath(Property.ObjectPtrOps->GetObject(ValuePtr));
			}
			Ar << Path;
			if (Ar.IsLoading())
			{
				Property.ObjectPtrOps->SetObject(ValuePtr, ResolveAssetObject(Path, Property.ObjectClass));
			}
			return;
		}

		uint32 ObjectId = 0;
		if (Ar.IsSaving())
		{
			UObject* Object = Property.ObjectPtrOps->GetObject(ValuePtr);
			if (IObjectReferenceResolver* Resolver = Ar.GetObjectResolver())
			{
				ObjectId = Resolver->GetObjectId(Object);
			}
			else
			{
				ObjectId = Object ? Object->GetUUID() : 0;
			}
		}

		Ar << ObjectId;

		if (Ar.IsLoading())
		{
			UObject* Object = nullptr;
			if (ObjectId != 0)
			{
				if (IObjectReferenceResolver* Resolver = Ar.GetObjectResolver())
				{
					Object = Resolver->ResolveObjectId(ObjectId, Property.ObjectClass);
				}
				else
				{
					Object = UObjectManager::Get().FindByUUID(ObjectId);
				}
			}
			Property.ObjectPtrOps->SetObject(ValuePtr, IsObjectAssignableTo(Object, Property.ObjectClass) ? Object : nullptr);
		}
	}

	void SerializeSoftObjectPtrValue(FArchive& Ar, const FProperty& Property, void* ValuePtr)
	{
		if (!Property.SoftObjectOps)
		{
			return;
		}

		FString Path;
		if (Ar.IsSaving())
		{
			Path = Property.SoftObjectOps->GetPath(ValuePtr);
		}
		Ar << Path;
		if (Ar.IsLoading())
		{
			Property.SoftObjectOps->SetPath(ValuePtr, Path);
		}
	}

	void SerializeArrayValue(FArchive& Ar, const FProperty& Property, void* ValuePtr)
	{
		if (!Property.ArrayOps || !Property.InnerProperty)
		{
			return;
		}

		int32 Count = Property.ArrayOps->Num(ValuePtr);
		const FString Key = Ar.GetCurrentKey();
		Ar.BeginArray(Key, Count);
		if (Ar.IsLoading())
		{
			Property.ArrayOps->Resize(ValuePtr, Count);
		}

		for (int32 Index = 0; Index < Count; ++Index)
		{
			void* ElementPtr = Property.ArrayOps->GetElementPtr(ValuePtr, Index);
			Property.InnerProperty->SerializeValue(Ar, ElementPtr);
		}

		Ar.EndArray();
	}

	bool CopyEnumValue(const FProperty& Property, void* DstValuePtr, const void* SrcValuePtr)
	{
		if (!Property.EnumMeta || !DstValuePtr || !SrcValuePtr)
		{
			return false;
		}
		std::memcpy(DstValuePtr, SrcValuePtr, Property.EnumMeta->Size);
		return true;
	}

	bool CopyObjectPtrValue(const FProperty& Property, void* DstValuePtr, const void* SrcValuePtr, const FDuplicateContext* Context)
	{
		if (!Property.ObjectPtrOps)
		{
			return false;
		}

		UObject* Object = Property.ObjectPtrOps->GetObject(SrcValuePtr);
		if (Context)
		{
			if (UObject* RemappedObject = Context->ResolveDuplicatedObject(Object))
			{
				Object = RemappedObject;
			}
		}

		Property.ObjectPtrOps->SetObject(DstValuePtr, IsObjectAssignableTo(Object, Property.ObjectClass) ? Object : nullptr);
		return true;
	}

	bool CopySoftObjectPtrValue(const FProperty& Property, void* DstValuePtr, const void* SrcValuePtr)
	{
		if (!Property.SoftObjectOps)
		{
			return false;
		}

		Property.SoftObjectOps->SetPath(DstValuePtr, Property.SoftObjectOps->GetPath(SrcValuePtr));
		return true;
	}

	bool CopyArrayValue(const FProperty& Property, void* DstValuePtr, const void* SrcValuePtr, const FDuplicateContext* Context)
	{
		if (!Property.ArrayOps || !Property.InnerProperty)
		{
			return false;
		}

		const int32 Count = Property.ArrayOps->Num(SrcValuePtr);
		Property.ArrayOps->Resize(DstValuePtr, Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			void* DstElement = Property.ArrayOps->GetElementPtr(DstValuePtr, Index);
			const void* SrcElement = Property.ArrayOps->GetElementPtr(SrcValuePtr, Index);
			Property.InnerProperty->CopyValue(DstElement, SrcElement, Context);
		}
		return true;
	}
}
