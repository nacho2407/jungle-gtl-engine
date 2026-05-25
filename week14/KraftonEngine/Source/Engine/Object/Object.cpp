#include "Object.h"
#include "UUIDGenerator.h"
#include "Serialization/Archive.h"
#include "Serialization/DuplicateArchive.h"
#include "Serialization/GCArchive.h"
#include "Object/Reflection/ObjectFactory.h"

TArray<UObject*> GUObjectArray;
TSet<UObject*> GUObjectSet;

UObject::UObject()
{
	UUID = UUIDGenerator::GenUUID();
	InternalIndex = static_cast<uint32>(GUObjectArray.size());
	GUObjectArray.push_back(this);
	GUObjectSet.insert(this);
}

UObject::~UObject()
{
	GUObjectSet.erase(this);

	uint32 LastIndex = static_cast<uint32>(GUObjectArray.size() - 1);

	if (InternalIndex != LastIndex)
	{
		UObject* LastObject = GUObjectArray[LastIndex];
		GUObjectArray[InternalIndex] = LastObject;
		LastObject->InternalIndex = InternalIndex;
	}

	GUObjectArray.pop_back();
}

UObject* UObject::Duplicate(UObject* NewOuter) const
{
	FDuplicateArchiveContext DuplicateContext;
	UObject* Dup = DuplicateWithArchiveContext(NewOuter, DuplicateContext);
	DuplicateContext.ResolveObjectReferenceFixups();
	if (Dup)
	{
		Dup->PostDuplicate();
	}
	return Dup;
}

UObject* UObject::DuplicateWithArchiveContext(UObject* NewOuter, FDuplicateArchiveContext& DuplicateContext) const
{
	// FObjectFactory 기반 같은 타입 인스턴스 생성 → Serialize 왕복.
	// UUID/Name은 생성자에서 새로 발급되며, Serialize에서 덮어쓰지 않는 것이 규칙이다.
	// NewOuter가 nullptr이면 원본의 Outer를 그대로 승계.
	UObject* EffectiveOuter = NewOuter ? NewOuter : Outer;
	UObject* Dup = FObjectFactory::Get().Create(GetClass()->GetName(), EffectiveOuter);
	if (!Dup)
	{
		return nullptr;
	}
	DuplicateContext.AddObjectMapping(GetUUID(), Dup);

	FDuplicateDataWriter Writer;
	const_cast<UObject*>(this)->Serialize(Writer);

	FDuplicateDataReader Reader(Writer.GetBuffer(), DuplicateContext);
	Dup->Serialize(Reader);
	return Dup;
}

void UObject::Serialize(FArchive& Ar)
{
	// 기본 UObject는 직렬화할 상태 없음.
	// UUID/InternalIndex/Name은 직렬화 금지 (복제 시 새로 발급).
	Ar << ObjectName;
}

void UObject::AddReferencedObjects(FReferenceCollector& Collector)
{
	FGCArchive Ar(Collector);
	SerializeProperties(Ar, PF_None);
}

void UObject::SerializeProperties(FArchive& Ar, uint32 RequiredFlags)
{
	Ar.BeginObject();

	TArray<const FProperty*> Properties;
	GetClass()->GetPropertyRefs(Properties);

	for (const FProperty* Property : Properties)
	{
		if (!Property || (Property->Flags & RequiredFlags) != RequiredFlags)
		{
			continue;
		}

		if (!Property->GetValuePtrFor(this))
		{
			continue;
		}

		if (!Ar.HasProperty(Property->Name))
		{
			continue;
		}

		Ar.BeginProperty(Property->Name);
		Property->Serialize(this, Ar);
		Ar.EndProperty();
	}

	Ar.EndObject();
}

void UObject::GetEditableProperties(TArray<FPropertyValue>& OutProps)
{
	PreGetEditableProperties();

	TArray<const FProperty*> Properties;
	GetClass()->GetPropertyRefs(Properties);

	for (const FProperty* Property : Properties)
	{
		if (!Property || (Property->Flags & PF_Edit) == 0)
		{
			continue;
		}
		if (!ShouldExposeProperty(*Property))
		{
			continue;
		}

		if(Property->GetValuePtrFor(this))
		{
			OutProps.push_back(Property->ToValue(this, this));
		}
	}
}

bool UObject::ShouldExposeProperty(const FProperty& /*Property*/) const
{
	return true;
}

void UObject::PostEditProperty(const char* /*PropertyName*/)
{
	// 기본 UObject는 편집 후 추가 작업 없음.
}

void UObject::PostEditChangeProperty(const FPropertyChangedEvent& Event)
{
	PostEditProperty(Event.PropertyName);
}

void UObject::RegisterProperties(UStruct* Class)
{
	(void)Class;
}

UClass UObject::StaticClassInstance("UObject", nullptr, sizeof(UObject), CF_None);
