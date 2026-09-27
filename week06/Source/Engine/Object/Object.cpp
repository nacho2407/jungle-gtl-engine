#include "Object.h"
#include "UUIDGenerator.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryArchive.h"
#include "Object/ObjectFactory.h"

TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	UUID = UUIDGenerator::GenUUID();
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

UObject* UObject::Duplicate(UObject* NewOuter) const
{
	// FObjectFactory 기반 같은 타입 인스턴스 생성 → Serialize 왕복 → PostDuplicate.
	// UUID/Name은 생성자에서 새로 발급되며, Serialize에서 덮어쓰지 않는 것이 규칙이다.
	// NewOuter가 nullptr이면 원본의 Outer를 그대로 승계.
	UObject* EffectiveOuter = NewOuter ? NewOuter : Outer;
	UObject* Dup = FObjectFactory::Get().Create(GetTypeInfo()->name, EffectiveOuter);
	if (!Dup)
	{
		return nullptr;
	}

	FMemoryArchive Writer(/*bIsSaving=*/true);
	const_cast<UObject*>(this)->Serialize(Writer);

	FMemoryArchive Reader(Writer.GetBuffer(), /*bIsSaving=*/false);
	Dup->Serialize(Reader);

	Dup->PostDuplicate();
	return Dup;
}

void UObject::Serialize(FArchive& /*Ar*/)
{
	// 기본 UObject는 직렬화할 상태 없음.
	// UUID/InternalIndex/Name은 직렬화 금지 (복제 시 새로 발급).
}

const FTypeInfo UObject::s_TypeInfo = { "UObject", nullptr, sizeof(UObject) };
