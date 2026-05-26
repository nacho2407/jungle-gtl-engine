#include "GarbageCollector.h"
#include "Object.h"
#include "ReferenceCollector.h"
#include "Serialization/GCArchive.h"
#include "Runtime/Engine.h"
#include "Core/Logging/Log.h"
#include "Mesh/MeshManager.h"
#include "Materials/MaterialManager.h"
#include "Texture/Texture2D.h"
#include "Animation/Skeleton/SkeletonManager.h"
#include "Particles/ParticleSystemManager.h"

void FGarbageCollector::CollectGarbage()
{
	// [Step 1] 모든 객체의 마킹을 false로 초기화
	for (UObject* Obj : GUObjectArray) {
		if (Obj)
			Obj->SetGarbageMarked(false);
	}

	// [Step 2] Root Set(절대 삭제되면 안 되는 시작점)들로부터 순회 시작
	FReferenceCollector Collector;


	// Root set
	if (GEngine)
	{
		Collector.AddReferencedObject(GEngine);
	}
	FMeshManager::AddReferencedObjects(Collector);
	FMaterialManager::Get().AddReferencedObjects(Collector);
	UTexture2D::AddReferencedObjects(Collector);
	FSkeletonManager::Get().AddReferencedObjects(Collector);
	FParticleSystemManager::Get().AddReferencedObjects(Collector);

	while (UObject* Object = Collector.Pop())
	{
		FGCArchive Ar(Collector);
		Object->Serialize(Ar);
		Object->AddReferencedObjects(Collector);
	}

	// [Step 3] 마킹되지 않은 객체들 삭제 (Sweep)
	for (int32 i = static_cast<int32>(GUObjectArray.size()) - 1; i >= 0; --i) {
		UObject* Obj = GUObjectArray[i];
		if (!Obj->IsGarbageMarked()) {
			// 참조되지 않음 -> 삭제!
			UE_LOG("[GC] Sweep candidate: %s", Obj->GetName().c_str());
			//UObjectManager::Get().DestroyObject(Obj);
		}
	}
}
