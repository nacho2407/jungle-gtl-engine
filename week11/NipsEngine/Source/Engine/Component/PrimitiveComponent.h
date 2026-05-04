#pragma once

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Render/Common/RenderTypes.h"
#include "Engine/Geometry/Ray.h"
#include "Core/CollisionTypes.h"
#include "Engine/Geometry/AABB.h"
#include "Core/Delegates/Delegate.h"
#include "Collision/Collision.h"

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

class UPrimitiveComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UPrimitiveComponent, USceneComponent)
    ~UPrimitiveComponent() override;

    FOnComponentBeginOverlap OnComponentBeginOverlap;
    FOnComponentEndOverlap OnComponentEndOverlap;
    FOnComponentHit OnComponentHit;

    /* For Property window */
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    virtual void Serialize(FArchive& Ar) override;

    /* Visibility */
    void SetVisibility(bool bVisible);
    bool IsVisible() const { return bIsVisible; }

    void SetEnableCull(const bool bInEnableCull) { bEnableCull = bInEnableCull; }
    bool IsEnableCull() const { return bEnableCull; }

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
	bool bIsVisible = true;
	bool bEnableCull = true; // frustum, occlusion culling으로 컬링될지 여부 판정

    bool bGenerateOverlapEvents = false;
    bool bBlockComponent = false; // ComponentHit
    TMap<UPrimitiveComponent*, FCollisionResult> CurOverlaps;
    TMap<UPrimitiveComponent*, FCollisionResult> PrevOverlaps;
};

// struct FMeshData;
//
//
// class UPrimitiveComponent : public USceneComponent
// {
// protected:
// 	const FMeshData* MeshData = nullptr;
// 	FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
// 	mutable FVector WorldAABBMinLocation;
// 	mutable FVector WorldAABBMaxLocation;
// 	bool bIsVisible = true;
//
// public:
// 	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)
//
// 	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
//
// 	inline const FMeshData* GetMeshData() const { return MeshData; };
//
// 	inline void SetVisibility(bool bVisible) { bIsVisible = bVisible; }
//
// 	// 월드 공간 AABB를 FBoundingBox로 반환 (파트 B LineBatcher와의 인터페이스)
// 	FBoundingBox GetWorldBoundingBox() const
// 	{
// 		return FBoundingBox(WorldAABBMinLocation, WorldAABBMaxLocation);
// 	}
//
// 	//Collision
// 	virtual void UpdateWorldAABB() const;
// 	bool CheckAABB(const FRay& Ray);
// 	bool Raycast(const FRay& Ray, FHitResult& OutHitResult);
// 	bool IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir, const FVector& V0, const FVector& V1, const FVector& V2, float& OutT);
// 	virtual bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult);
// 	inline bool IsVisible() const { return bIsVisible; }
//
// 	void UpdateWorldMatrix() const override;
// 	void AddWorldOffset(const FVector& WorldDelta) override;
// 	virtual EPrimitiveType GetPrimitiveType() const = 0;
//
// 	// MeshBuffer 기반 아웃라인 렌더링을 지원하는지 여부.
// 	// Batcher 처리 타입(SubUV, Text)은 false를 반환합니다.
// 	virtual bool SupportsOutline() const { return true; }
// };
//
// class UCubeComponent : public UPrimitiveComponent
// {
// private:
//
// public:
// 	DECLARE_CLASS(UCubeComponent, UPrimitiveComponent)
// 	UCubeComponent();
// 	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Cube;
//
// 	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
// };
//
// class USphereComponent : public UPrimitiveComponent
// {
// private:
//
// public:
// 	DECLARE_CLASS(USphereComponent, UPrimitiveComponent)
// 	USphereComponent();
// 	void UpdateWorldAABB() const override;
// 	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Sphere;
//
// 	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
// };
//
// class UPlaneComponent : public UPrimitiveComponent
// {
// private:
//
// public:
// 	DECLARE_CLASS(UPlaneComponent, UPrimitiveComponent)
// 	UPlaneComponent();
// 	void UpdateWorldAABB() const override;
// 	void SetRelativeScale(const FVector& NewScale);
// 	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Plane;
//
// 	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
// };
