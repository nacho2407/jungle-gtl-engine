#pragma once
#include "SceneComponent.h"
#include "Primitive/PrimitiveBase.h"
#include "PrimitiveComponent.h"
#include "Math/Frustum.h"
#include <memory>
#include <algorithm>
#include <cmath>

struct FRenderMesh;
class FArchive;
class FMaterial;
class Archive;
struct FBoxSphereBounds;

class ENGINE_API UNewPrimitiveComponent : public USceneComponent
{
public:
	DECLARE_RTTI(UNewPrimitiveComponent, USceneComponent)

	// virtual FBoxSphereBounds GetWorldBounds() const { return Bounds; };
	virtual FBoxSphereBounds GetWorldBounds() const { return CalcBounds(GetWorldTransform()); }
	virtual void UpdateBounds();
	virtual FBoxSphereBounds GetLocalBounds() const;
	virtual FBoxSphereBounds CalcBounds(const FMatrix& LocalToWorld) const;

	// virtual void Serialize(FArchive& Ar) override;

	bool ShouldDrawDebugBounds() const { return bDrawDebugBounds; }
	void SetDrawDebugBounds(bool bEnable) { bDrawDebugBounds = bEnable; }

protected:
	FBoxSphereBounds Bounds;
	bool bDrawDebugBounds = true;
};
