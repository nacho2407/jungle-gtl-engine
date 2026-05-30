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

private:
	UPhysicsAssetDebugComponent* GetPhysicsAssetDebugComponent() const;
	USkeletalMeshComponent* GetTargetSkeletalMeshComponent() const;
	bool GetConstraintWorldFrames(
		const FConstraintInstanceInitDesc& ConstraintDesc,
		FTransform& OutParentFrame,
		FTransform& OutChildFrame) const;
};
