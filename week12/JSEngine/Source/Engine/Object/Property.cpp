#include "Object/Property.h"

#include "Object/Object.h"
#include "Serialization/Archive.h"

#include <cmath>
#include <cstring>

namespace
{
	bool IsValueChannel(const FString& ChannelName);
	
	// Serialize
	template <typename T> 
	void SerializePropertyValue(FArchive& Ar, const FProperty& Property, UObject* Container);
	void SerializeEnumValue(FArchive& Ar, void* ValuePtr, uint8 Size);

	// Copy
	template <typename T>
	bool CopyTypedValue(UObject* DstContainer, const UObject* SrcContainer, const FProperty& Property);
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

	switch (Type)
	{
	case EPropertyType::Bool:
		Ar << Name;
		SerializePropertyValue<bool>(Ar, *this, Container);
		break;
	case EPropertyType::Int:
		Ar << Name;
		SerializePropertyValue<int32>(Ar, *this, Container);
		break;
	case EPropertyType::Float:
		Ar << Name;
		SerializePropertyValue<float>(Ar, *this, Container);
		break;
	case EPropertyType::Vec3:
		Ar << Name;
		SerializePropertyValue<FVector>(Ar, *this, Container);
		break;
	case EPropertyType::Vec4:
		Ar << Name;
		SerializePropertyValue<FVector4>(Ar, *this, Container);
		break;
	case EPropertyType::String:
		Ar << Name;
		SerializePropertyValue<FString>(Ar, *this, Container);
		break;
	case EPropertyType::Name:
		Ar << Name;
		SerializePropertyValue<FName>(Ar, *this, Container);
		break;
	case EPropertyType::Color:
		Ar << Name;
		SerializePropertyValue<FColor>(Ar, *this, Container);
		break;
	case EPropertyType::Vec3Array:
		Ar << Name;
		SerializePropertyValue<TArray<FVector>>(Ar, *this, Container);
		break;
	case EPropertyType::StringArray:
		Ar << Name;
		SerializePropertyValue<TArray<FString>>(Ar, *this, Container);
		break;
	case EPropertyType::Enum:
		if (EnumMeta)
		{
			Ar << Name;
			SerializeEnumValue(Ar, GetValuePtr(Container), EnumMeta->Size);
		}
		break;

	case EPropertyType::Guid:
	case EPropertyType::Quat:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::Material:
	case EPropertyType::SRV:
	case EPropertyType::CubeSRV:
	case EPropertyType::Unknown:
	default:
		break;
	}
}

bool FProperty::CopyValue(UObject* DstContainer, const UObject* SrcContainer) const
{
	if (!DstContainer || !SrcContainer || !Name || IsTransient())
	{
		return false;
	}

	switch (Type)
	{
	case EPropertyType::Bool:
		return CopyTypedValue<bool>(DstContainer, SrcContainer, *this);
	case EPropertyType::Int:
		return CopyTypedValue<int32>(DstContainer, SrcContainer, *this);
	case EPropertyType::Float:
		return CopyTypedValue<float>(DstContainer, SrcContainer, *this);
	case EPropertyType::Vec3:
		return CopyTypedValue<FVector>(DstContainer, SrcContainer, *this);
	case EPropertyType::Vec4:
		return CopyTypedValue<FVector4>(DstContainer, SrcContainer, *this);
	case EPropertyType::Color:
		return CopyTypedValue<FColor>(DstContainer, SrcContainer, *this);
	case EPropertyType::Guid:
		return CopyTypedValue<FGuid>(DstContainer, SrcContainer, *this);
	case EPropertyType::Quat:
		return CopyTypedValue<FQuat>(DstContainer, SrcContainer, *this);
	case EPropertyType::String:
		return CopyTypedValue<FString>(DstContainer, SrcContainer, *this);
	case EPropertyType::Name:
		return CopyTypedValue<FName>(DstContainer, SrcContainer, *this);
	case EPropertyType::Vec3Array:
		return CopyTypedValue<TArray<FVector>>(DstContainer, SrcContainer, *this);
	case EPropertyType::StringArray:
		return CopyTypedValue<TArray<FString>>(DstContainer, SrcContainer, *this);
	case EPropertyType::Material:
		return CopyTypedValue<TArray<UMaterialInterface*>>(DstContainer, SrcContainer, *this);
	case EPropertyType::Enum:
		if (EnumMeta && GetValuePtr(DstContainer) && GetValuePtr(SrcContainer))
		{
			std::memcpy(GetValuePtr(DstContainer), GetValuePtr(SrcContainer), EnumMeta->Size);
			return true;
		}
		return false;
	case EPropertyType::SceneComponentRef:
	case EPropertyType::SRV:
	case EPropertyType::CubeSRV:
	case EPropertyType::Unknown:
	default:
		return false;
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

	template <typename T>
	void SerializePropertyValue(FArchive& Ar, const FProperty& Property, UObject* Container)
	{
		if (T* Value = Property.ContainerPtrToValuePtr<T>(Container))
		{
			Ar << *Value;
		}
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

	template <typename T>
	bool CopyTypedValue(UObject* DstContainer, const UObject* SrcContainer, const FProperty& Property)
	{
		T* Dst = Property.ContainerPtrToValuePtr<T>(DstContainer);
		const T* Src = Property.ContainerPtrToValuePtr<T>(SrcContainer);
		if (!Dst || !Src)
		{
			return false;
		}
		*Dst = *Src;
		return true;
	}
}