#pragma once
#include "SceneComponent.h"
#include "PrimitiveComponent.h"
#include "Math/Frustum.h"
#include <memory>
#include <algorithm>
#include <cmath>

class ULevel;

struct FRenderMesh;
class FArchive;
class FMaterial;
class Archive;
struct FBoxSphereBounds;

struct FBoxSphereBounds
{
	FVector Center;
	float Radius = 0.f;
	FVector BoxExtent;
};

class ENGINE_API UPrimitiveComponent : public USceneComponent
{
public:
	DECLARE_RTTI(UPrimitiveComponent, USceneComponent)

	// virtual FBoxSphereBounds GetWorldBounds() const { return Bounds; };
	virtual FBoxSphereBounds GetWorldBounds() const { return CalcBounds(GetWorldTransform()); }
	virtual void UpdateBounds();
	virtual FBoxSphereBounds GetLocalBounds() const;
	virtual FBoxSphereBounds CalcBounds(const FMatrix& LocalToWorld) const;

	bool ShouldDrawDebugBounds() const { return bDrawDebugBounds; }
	void SetDrawDebugBounds(bool bEnable) { bDrawDebugBounds = bEnable; }

	virtual FRenderMesh* GetRenderMesh() const { return nullptr; }

	virtual bool IsPickable() const { return true; }
	virtual bool UseSpherePicking() const { return false; }
	virtual bool HasMeshIntersection() const { return false; }
	virtual bool IntersectLocalRay(const FVector& LocalOrigin, const FVector& LocalDir, float& InOutDist) const { return false; }

protected:
	virtual void MarkTransformDirty() override;

	FBoxSphereBounds Bounds;
	bool bDrawDebugBounds = true;
};
