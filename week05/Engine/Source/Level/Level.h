#pragma once

#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include <d3d11.h>
#include "WorldTypes.h"
#include "Core/ShowFlags.h"
#include "BVH.h"

class AActor;
class FCamera;
class FFrustum;
class UCameraComponent;
class UPrimitiveComponent;
struct FRenderCommandQueue;

// 씬 -> 레벨로 이름 변경.
class ENGINE_API ULevel : public UObject
{
public:
	DECLARE_RTTI(ULevel, UObject)
	~ULevel();

	/** 지정한 액터 타입을 생성하고 씬에 등록한 뒤 PostSpawnInitialize까지 호출한다. */
	template <typename T>
	T* SpawnActor(const FString& InName)
	{
		static_assert(std::is_base_of_v<AActor, T>, "T must derive from AActor");

		T* NewActor = FObjectFactory::ConstructObject<T>(this, InName);
		if (!NewActor)
		{
			return nullptr;
		}
		RegisterActor(NewActor);
		NewActor->PostSpawnInitialize();

		return NewActor;
	}

	/** 씬의 액터 목록에 추가하고, 액터가 자신이 속한 씬을 알 수 있게 연결한다. */
	void RegisterActor(AActor* InActor);
	/** 액터를 즉시 목록에서 제거하지 않고 파괴 절차만 시작한다. */
	void DestroyActor(AActor* InActor);
	/** 파괴 표시된 액터를 씬 배열에서 정리한다. */
	void CleanupDestroyedActors();

	/** 현재 씬에 살아 있는 액터 배열을 반환한다. */
	const TArray<AActor*>& GetActors() const { return Actors; }
	/** 자신이 속한 월드의 타입을 그대로 노출한다. */
	EWorldType GetWorldType() const;
	/** 현재 씬이 에디터용 월드에 속하는지 검사한다. */
	bool IsEditorScene() const;
	/** 현재 씬이 게임/PIE 월드에 속하는지 검사한다. */
	bool IsGameScene() const;
	/** 씬이 사용할 카메라를 월드로부터 가져온다. */
	FCamera* GetCamera() const;

	/** 모든 액터를 파괴하고 씬을 빈 상태로 되돌린다. */
	void ClearActors();
	/** 아직 BeginPlay가 호출되지 않은 액터들에게 한 번만 BeginPlay를 전파한다. */
	void BeginPlay();
	/** 씬 안의 액터를 순회하며 Tick하고, 끝나면 파괴 대상을 정리한다. */
	void Tick(float DeltaTime);

	void MarkSpatialDirty();
	void QueryPrimitivesByFrustum(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	void QueryPrimitivesByRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxDistance, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	void VisitPrimitivesByRay(const FVector& RayOrigin, const FVector& RayDirection, float& InOutMaxDistance, const BVH::FRayHitVisitor& Visitor) const;
	void VisitBVHNodes(const FBVHNodeVisitor& Visitor) const;
	void VisitBVHNodesForPrimitive(UPrimitiveComponent* Target, const FBVHNodeVisitor& Visitor) const;

	//----------------------------------------------------------------
	//						PIE Layer
	//----------------------------------------------------------------
	void DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const override;
	void DuplicateSubObjects(UObject* DuplicatedObject, FDuplicateContext& Context) const override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;
	void PostDuplicate(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

private:
	void GatherPrimitiveComponents(TArray<UPrimitiveComponent*>& OutPrimitives) const;
	void RebuildSpatialIfNeeded() const;

	TArray<AActor*> Actors;
	bool bBegunPlay = false;
	mutable BVH SpatialBVH;
	mutable bool bSpatialDirty = true;
};
