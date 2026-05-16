#include "Object.h"
#include "EngineStatics.h"
#include "Object/ObjectSerialize.h"
#include "Object/FName.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"
#include "Object/Property.h"
#include "Math/Vector.h"

#include <cstring>

class UMaterialInterface;

TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	UUID = EngineStatics::GenUUID();
	InternalIndex = static_cast<uint32>(GUObjectArray.size());
	GUObjectArray.push_back(this);
}

UObject::~UObject()
{
	uint32 LastIndex = static_cast<uint32>(GUObjectArray.size() - 1);

	if (InternalIndex != LastIndex)
	{
		UObject* LastObject = GUObjectArray[LastIndex];
		GUObjectArray[InternalIndex] = LastObject;
		LastObject->InternalIndex = InternalIndex;
	}

	GUObjectArray.pop_back();
}

const FTypeInfo UObject::s_TypeInfo = { "UObject", nullptr, sizeof(UObject) };

UClass* UObject::StaticClass()
{
	static UClass Class(
		"UObject",
		nullptr,
		sizeof(UObject),
		UObject::s_TypeInfo.ClassFlags,
		nullptr);

	static bool bRegistered = false;
	if (!bRegistered)
	{
		bRegistered = true;
		FReflectionRegistry::Get().RegisterUClass(&Class);
	}
	return &Class;
}

// FObjectFactory 로 같은 타입의 인스턴스를 생성한 뒤 프로퍼티 복사 → PostDuplicate 훅을 실행합니다.
// 팩토리에 등록되지 않은 추상 클래스(PrimitiveComponent 등)는 Create() 가 nullptr 를 반환하므로
// 그대로 nullptr 를 반환합니다.
UObject* UObject::Duplicate()
{
	UObject* Dup = FObjectFactory::Get().Create(GetTypeInfo()->name);
	Dup->CopyPropertiesFrom(this);
	Dup->PostDuplicate(this);
	return Dup;
}

// GetEditableProperties 에 노출된 프로퍼티를 이름 기반으로 매칭하여 복사합니다.
// ・ Bool / Int / Float / Vec3 / Vec4  → memcpy
// ・ String                            → FString 대입
// ・ Name                              → FName 대입 후 PostEditChangeProperty 호출
// ・ Guid / Quat                       → memcpy
// ・ Vec3Array / StringArray / Material→ container 대입
// ・ SceneComponentRef                 → 포인터 복원은 호출 측(Duplicate) 에서 담당
void UObject::CopyPropertiesFrom(UObject* Src)
{
	if (!Src)
	{
		return;
	}

	if (UClass* SrcClass = Src->GetClass())
	{
		if (UClass* DstClass = GetClass())
		{
			TArray<const FProperty*> SrcProperties;
			SrcClass->GetAllProperties(SrcProperties);

			if (!SrcProperties.empty())
			{
				for (const FProperty* SrcProperty : SrcProperties)
				{
					if (!SrcProperty || !SrcProperty->Name)
					{
						continue;
					}

					const FProperty* DstProperty = DstClass->FindProperty(SrcProperty->Name);
					if (!DstProperty || DstProperty->Type != SrcProperty->Type)
					{
						continue;
					}

					if (CopyPropertyValue(this, Src, *DstProperty))
					{
						PostEditChangeProperty({ DstProperty->Name, EPropertyChangeType::ValueSet });
					}
				}
				return;
			}
		}
	}

	// 과도기 fallback: 아직 UClass/FProperty 등록으로 넘어오지 않은 수동/legacy descriptor만 복사합니다.
	TArray<FPropertyDescriptor> SrcProps;
	Src->GetEditableProperties(SrcProps);

	TArray<FPropertyDescriptor> DstProps;
	this->GetEditableProperties(DstProps);

	for (const FPropertyDescriptor& SrcProp : SrcProps)
	{
		FPropertyDescriptor* DstProp = nullptr;
		for (FPropertyDescriptor& D : DstProps)
		{
			if (D.Name && SrcProp.Name && strcmp(D.Name, SrcProp.Name) == 0)
			{
				DstProp = &D;
				break;
			}
		}

		if (!DstProp || !DstProp->ValuePtr || !SrcProp.ValuePtr)
		{
			continue;
		}

		switch (SrcProp.Type)
		{
		case EPropertyType::Bool:
		case EPropertyType::Int:
		case EPropertyType::Float:
		case EPropertyType::Vec3:
		case EPropertyType::Vec4:
		case EPropertyType::Color:
		case EPropertyType::Guid:
		case EPropertyType::Quat:
		{
			const size_t Size = GetPropertySize(SrcProp.Type);
			if (Size > 0)
			{
				memcpy(DstProp->ValuePtr, SrcProp.ValuePtr, Size);
				this->PostEditChangeProperty({ SrcProp.Name, EPropertyChangeType::ValueSet });
			}
			break;
		}
		case EPropertyType::Enum:
		{
			if (SrcProp.EnumMeta && DstProp->EnumMeta && SrcProp.EnumMeta->Size == DstProp->EnumMeta->Size)
			{
				memcpy(DstProp->ValuePtr, SrcProp.ValuePtr, SrcProp.EnumMeta->Size);
				this->PostEditChangeProperty({ SrcProp.Name, EPropertyChangeType::ValueSet });
			}
			break;
		}
		case EPropertyType::String:
			*static_cast<FString*>(DstProp->ValuePtr) = *static_cast<const FString*>(SrcProp.ValuePtr);
			this->PostEditChangeProperty({ SrcProp.Name, EPropertyChangeType::ValueSet });
			break;

		case EPropertyType::Name:
			*static_cast<FName*>(DstProp->ValuePtr) = *static_cast<const FName*>(SrcProp.ValuePtr);
			this->PostEditChangeProperty({ SrcProp.Name, EPropertyChangeType::ValueSet });
			break;

		case EPropertyType::SceneComponentRef:
			break;

		case EPropertyType::Vec3Array:
		{
			auto* Dst = static_cast<TArray<FVector>*>(DstProp->ValuePtr);
			const auto* SrcArray = static_cast<const TArray<FVector>*>(SrcProp.ValuePtr);
			*Dst = *SrcArray;
			this->PostEditChangeProperty({ SrcProp.Name, EPropertyChangeType::ValueSet });
			break;
		}
		case EPropertyType::StringArray:
		{
			auto* Dst = static_cast<TArray<FString>*>(DstProp->ValuePtr);
			const auto* SrcArray = static_cast<const TArray<FString>*>(SrcProp.ValuePtr);
			*Dst = *SrcArray;
			this->PostEditChangeProperty({ SrcProp.Name, EPropertyChangeType::ValueSet });
			break;
		}
		case EPropertyType::Material:
		{
			auto* Dst = static_cast<TArray<UMaterialInterface*>*>(DstProp->ValuePtr);
			const auto* SrcArray = static_cast<const TArray<UMaterialInterface*>*>(SrcProp.ValuePtr);
			if (Dst && SrcArray)
			{
				*Dst = *SrcArray;
				this->PostEditChangeProperty({ SrcProp.Name, EPropertyChangeType::ValueSet });
			}
			break;
		}
		case EPropertyType::SRV:
		case EPropertyType::CubeSRV:
			break;
		default:
			break;
		}
	}
}

void UObject::Serialize(FArchive& Ar)
{
	FString ClassName = GetClass() ? GetClass()->GetName() : GetTypeInfo()->name;
	Ar << "Type" << ClassName;
	Ar << "ObjectName" << ObjectName;
	SerializeReflectedProperties(Ar);
}

void UObject::SerializeReflectedProperties(FArchive& Ar)
{
	UClass* Class = GetClass();
	if (!Class)
	{
		ObjectSerialize::SerializeProperties(Ar, this);
		return;
	}

	TArray<const FProperty*> Properties;
	Class->GetAllProperties(Properties);

	// 과도기 호환: 아직 GenerateReflection.py를 다시 돌리지 않았거나,
	// UClass/FProperty 등록 대상이 아닌 객체는 기존 FClassMetaData 직렬화를 사용합니다.
	if (Properties.empty())
	{
		ObjectSerialize::SerializeProperties(Ar, this);
		return;
	}

	for (const FProperty* Property : Properties)
	{
		if (!Property)
		{
			continue;
		}
		SerializeProperty(Ar, this, *Property);
	}
}

// 런타임 UClass/FProperty를 우선 사용하고, 아직 런타임 등록이 없는 경우만
// 기존 FClassMetaData 경로로 디스크립터를 생성합니다.
void UObject::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	// 과도기 fallback 전용: FPropertyDescriptor는 수동/legacy 프로퍼티만 담습니다.
	// 런타임 리플렉션 프로퍼티는 GetClass()->GetAllProperties()를 직접 사용합니다.

	const FTypeInfo* CurrentType = GetTypeInfo();
	while (CurrentType != nullptr)
	{
		if (const FClassMetaData* Meta = FReflectionRegistry::Get().GetRegisteredClass(CurrentType->name))
		{
			for (const FPropertyMetaData& PropMeta : Meta->Properties)
			{
				FPropertyDescriptor Desc;
				Desc.Name = PropMeta.Name;
				Desc.Type = PropMeta.Type;
				Desc.ValuePtr = reinterpret_cast<uint8*>(this) + PropMeta.Offset;
				Desc.UsageFlags = PropMeta.UsageFlags;
				Desc.Min = PropMeta.Min;
				Desc.Max = PropMeta.Max;
				Desc.Speed = PropMeta.Speed;
				Desc.DisplayName = PropMeta.DisplayName;
				Desc.EnumMeta = PropMeta.EnumMeta;
				OutProps.push_back(Desc);
			}
		}
		CurrentType = CurrentType->Parent;
	}

	// 하위 클래스에서 override한 기존의 수동 프로퍼티는 이어서 수집됩니다.
}
