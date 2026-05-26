#pragma once
#include "SceneComponent.h"
#include "Render/Common/RenderTypes.h"
#include "Engine/Geometry/Ray.h"
#include "Core/CollisionTypes.h"
#include "Engine/Geometry/AABB.h"
#include "Core/Delegates/Delegate.h"
#include "Collision/Collision.h"

class FPrimitiveRenderProxy;

/*
	아직 미사용
*/
DECLARE_DELEGATE(FOnComponentHit, UPrimitiveComponent*, AActor*, UPrimitiveComponent*, FVector, const FHitResult&);

/*
	OverlappedComponent  // 이벤트를 받은 주체 (내 쪽)
	OtherActor           // 겹친 상대 Actor
	OtherComp            // 겹친 상대 Component
	OtherBodyIndex       // (멀티 바디용) 상대의 바디 인덱스
	bFromSweep           // Sweep으로 인해 발생했는지
	SweepResult          // Sweep 충돌 상세 정보
*/

DECLARE_DELEGATE(FOnComponentBeginOverlap, UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32, bool, const FHitResult&)
DECLARE_DELEGATE(FOnComponentEndOverlap, UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32, bool, const FHitResult&)

UCLASS(Abstract)
class UPrimitiveComponent : public USceneComponent
{
public:
	GENERATED_BODY(UPrimitiveComponent, USceneComponent)
	~UPrimitiveComponent() override;

	FOnComponentBeginOverlap OnComponentBeginOverlap;
	FOnComponentEndOverlap OnComponentEndOverlap;
	FOnComponentHit OnComponentHit;

	/* For Property window */
	void PostDuplicate(UObject* Original) override;
	void PostEditProperty(const char* PropertyName) override;

	/* Visibility */
	void SetVisibility(bool bVisible);
	bool IsVisible() const { return bIsVisible; }

	void SetEnableCull(const bool bInEnableCull) { bEnableCull = bInEnableCull; }
	bool IsEnableCull() const { return bEnableCull; }

	void SetCastDecal(bool bInCastDecal) { bCastDecal = bInCastDecal; }
	bool IsCastDecal() const { return bCastDecal; }

	/* Getter */
	virtual const FAABB& GetWorldAABB() const
	{
		UpdateWorldAABB();
		return WorldAABB;
	}

	/* For Collision(Ray-casting) */
	virtual void UpdateWorldAABB() const = 0;
	bool Raycast(const FRay& Ray, FHitResult& OutHitResult);
	bool IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir, const FVector& V0, const FVector& V1,
						   const FVector& V2, float& OutT);
	virtual bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) = 0;

	/* For Transform */
	void UpdateWorldMatrix() const override;
	void AddWorldOffset(const FVector& WorldDelta) override;
	virtual EPrimitiveType GetPrimitiveType() const = 0;
	virtual FPrimitiveRenderProxy* GetRenderProxy() { return nullptr; }

	/* For Material */
	virtual int32 GetNumMaterials() const { return 0; }
	virtual class UMaterialInterface* GetMaterial(int32 SlotIndex) const { return nullptr; }
	virtual void SetMaterial(int32 SlotIndex, class UMaterialInterface* InMaterial) {}

	virtual bool SupportsOutline() const { return true; }

	const TMap<UPrimitiveComponent*, FCollisionResult>& GetOverlapInfos() const { return CurOverlaps; }
	bool IsOverlappingActor(const AActor* OtherActor) const;
	bool ShouldGenerateOverlapEvents() const { return bGenerateOverlapEvents; }
	void SetGenerateOverlapEvents(bool NewState) { bGenerateOverlapEvents = NewState; }
	void ClearOverlaps() { CurOverlaps.clear(); }
	void AddOverlap(UPrimitiveComponent* OtherComp, const FCollisionResult& CollisionResult) { CurOverlaps[OtherComp] = CollisionResult; }
	void RemoveOverlap(UPrimitiveComponent* OtherComp);
	void SetPrevOverlaps(const TMap<UPrimitiveComponent*, FCollisionResult>& InOverlaps) { PrevOverlaps = InOverlaps; }
	// Begin, End 체크
	void ResolveOverlaps();

protected:
	void OnTransformDirty() override;
	void NotifySpatialIndexDirty() const;

protected:
	mutable FAABB WorldAABB;

	UPROPERTY(DisplayName = "Visible", LuaReadWrite, LuaName = Visible)
	bool bIsVisible = true;

	UPROPERTY(DisplayName = "Enable Cull", LuaReadWrite, LuaName = EnableCull)
	bool bEnableCull = true; // frustum, occlusion culling으로 컬링될지 여부 판정

	UPROPERTY(DisplayName = "Cast Decal")
	bool bCastDecal = true;

	UPROPERTY(DisplayName = "Generate Overlap Events", LuaReadOnly, LuaName = GenerateOverlapEvents)
	bool bGenerateOverlapEvents = false;

	bool bBlockComponent = false; // ComponentHit
	TMap<UPrimitiveComponent*, FCollisionResult> CurOverlaps;
	TMap<UPrimitiveComponent*, FCollisionResult> PrevOverlaps;
};
