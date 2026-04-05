#pragma once
#include "SceneComponent.h"
#include "Math/Frustum.h"
#include <memory>
#include <algorithm>
#include <cmath>

struct FRenderMesh;
class FArchive;
class FMaterial;
class Archive;
class FPrimitiveSceneProxy;
struct FBoxSphereBounds;

struct FBoxSphereBounds
{
	FVector Center;
	float Radius = 0.f;
	FVector BoxExtent;
};

enum class EPrimitiveRenderCategory : uint8
{
	Primitive,
	Text,
	SubUV,
	UUIDBillboard,
};

class ENGINE_API UPrimitiveComponent : public USceneComponent
{
public:
	DECLARE_RTTI(UPrimitiveComponent, USceneComponent)

	virtual FBoxSphereBounds GetWorldBounds() const;
	virtual void UpdateBounds() override;
	virtual FBoxSphereBounds GetLocalBounds() const;
	virtual FBoxSphereBounds CalcBounds(const FMatrix& LocalToWorld) const;

	bool ShouldDrawDebugBounds() const { return bDrawDebugBounds; }
	void SetDrawDebugBounds(bool bEnable) { bDrawDebugBounds = bEnable; }

	virtual FRenderMesh* GetRenderMesh() const;
	virtual FRenderMesh* GetRenderMesh(const float& Distance) const;
	virtual EPrimitiveRenderCategory GetRenderCategory() const { return EPrimitiveRenderCategory::Primitive; }
	virtual std::shared_ptr<FPrimitiveSceneProxy> CreateSceneProxy() const;
	FPrimitiveSceneProxy* GetSceneProxy() const;
	void MarkRenderStateDirty();
	static void FlushPendingRenderStateUpdates();

	void OnRegister() override;
	void OnUnregister() override;

	virtual bool IsPickable() const { return true; }
	virtual bool UseSpherePicking() const { return false; }
	virtual bool HasMeshIntersection() const { return false; }
	virtual bool IntersectLocalRay(const FVector& LocalOrigin, const FVector& LocalDir, float& InOutDist) const { return false; }

protected:
	static void EnqueueRenderStateUpdate(UPrimitiveComponent* InPrimitiveComponent);
	void RecreateSceneProxy();

	static TArray<UPrimitiveComponent*> PendingRenderStateUpdates;

	mutable FBoxSphereBounds Bounds;
	mutable bool bBoundsDirty = true;
	mutable std::shared_ptr<FPrimitiveSceneProxy> SceneProxy;
	mutable bool bRenderStateDirty = true;
	mutable bool bRenderStateUpdateQueued = false;
	bool bDrawDebugBounds = true;
};
