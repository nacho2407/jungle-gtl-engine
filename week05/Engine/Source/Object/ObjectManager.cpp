#include "ObjectManager.h"
#include "Object/Object.h"
#include "Object/Class.h"
#include "Debug/EngineLog.h"
#include <algorithm>
#include "Object/ObjectFactory.h"

constexpr int32 GUObjectArrayReserveSize = 32768;

ObjectManager::ObjectManager()
{
	GUObjectArray.reserve(GUObjectArrayReserveSize);
}

ObjectManager::~ObjectManager()
{
	// GUObjectArray에 남은 오브젝트 전부 해제
	for (UObject* Obj : GUObjectArray)
	{
		if (Obj)
		{
			delete Obj;
		}
	}
	GUObjectArray.clear();
}

UObject* ObjectManager::SpawnObject(
	UClass* InClass,
	UObject* InOuter,
	const FString& InName)
{
	return FObjectFactory::ConstructObject(InClass, InOuter, InName);
}

void ObjectManager::ReleaseObject(UObject* obj)
{
	if (!obj) return;

	// PendingKill 마킹 후 즉시 삭제
	// ~UObject()에서 GUObjectArray[InternalIndex] = nullptr 처리
	obj->MarkPendingKill();
	delete obj;
}

void ObjectManager::FlushKilledObjects()
{
	int32 PrevCount = static_cast<int32>(GUObjectArray.size());
	int32 KilledCount = 0;

	// Phase 1: PendingKill 오브젝트를 실제 delete (GC)
	for (int32 Idx = 0; Idx < GUObjectArray.size(); ++Idx)
	{
		UObject* Obj = GUObjectArray[Idx];
		if (Obj && Obj->IsPendingKill())
		{
			delete Obj;
			++KilledCount;
		}
	}

	// Phase 2: nullptr 슬롯을 제거하고 살아있는 오브젝트의 InternalIndex 재조정
	int32 WriteIdx = 0;
	for (int32 ReadIdx = 0; ReadIdx < GUObjectArray.size(); ++ReadIdx)
	{
		UObject* Obj = GUObjectArray[ReadIdx];
		if (Obj != nullptr)
		{
			Obj->InternalIndex = static_cast<uint32>(WriteIdx);
			GUObjectArray[WriteIdx] = Obj;
			++WriteIdx;
		}
	}
	GUObjectArray.resize(WriteIdx);
	GUObjectArray.reserve(GUObjectArrayReserveSize);

	UE_LOG("[GC] FlushKilledObjects: %d objects collected, %d -> %d alive",
		KilledCount, PrevCount, WriteIdx);
}