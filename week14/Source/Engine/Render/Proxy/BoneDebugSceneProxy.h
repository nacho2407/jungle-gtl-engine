#pragma once

#include "ShapeSceneProxy.h"

class UBoneDebugComponent;

class FBoneDebugSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FBoneDebugSceneProxy(UBoneDebugComponent* InComponent);
	~FBoneDebugSceneProxy() override;

	void UpdateTransform() override;

	const TArray<FWireLine>& GetCachedLines() const { return CachedLines; }
	const TArray<FWireLine>& GetCachedParentBoneLines() const { return CachedParentBoneLines; }

	const FVector4& GetBoneColor() const { return BoneColor; }
	const FVector4& GetParentBoneColor() const { return ParentBoneColor; }

private:
	void RebuildLines();

private:
	TArray<FWireLine> CachedLines;
	TArray<FWireLine> CachedParentBoneLines;

	FVector4 BoneColor;
	FVector4 ParentBoneColor;
};
