#pragma once

#include "Math/Transform.h"
#include "Object/FName.h"
#include "Render/Geometry/DebugGeometryTypes.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"

class UPhysicsAssetDebugComponent;
class USkeletalMeshComponent;
struct FConstraintInstanceInitDesc;
struct FFrameContext;

class FPhysicsAssetSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FPhysicsAssetSceneProxy(UPhysicsAssetDebugComponent* InComponent);
	~FPhysicsAssetSceneProxy() override = default;

	void BuildPhysicsAssetSolidMesh(const FFrameContext& Frame, FPhysicsDebugSolidMesh& OutMesh) const;
	void BuildPhysicsAssetConstraintAxisLines(const FFrameContext& Frame, TArray<FPhysicsDebugLine>& OutLines) const;

private:
	UPhysicsAssetDebugComponent* GetPhysicsAssetDebugComponent() const;
	USkeletalMeshComponent* GetTargetSkeletalMeshComponent() const;

	mutable FPhysicsDebugSolidMesh CachedSolidMesh;
	mutable uint64 CachedSolidDebugRevision = 0;
	mutable uint64 CachedSolidSkinnedRevision = 0;
	mutable bool bCachedSolidMeshFresh = false;
};
