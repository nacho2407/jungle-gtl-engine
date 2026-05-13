#pragma once

#include "Component/PrimitiveComponent.h"

class USkeletalMeshComponent;
class FScene;

enum class EBoneDebugDrawMode : uint8
{
	SelectedOnly,
	AllBones
};

class UBoneDebugComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UBoneDebugComponent, UPrimitiveComponent)

	UBoneDebugComponent();
	~UBoneDebugComponent() override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	USkeletalMeshComponent* GetTargetMeshComponent() const { return TargetMeshComponent; }
	void SetTargetMeshComponent(USkeletalMeshComponent* InMeshComponent) { TargetMeshComponent = InMeshComponent; MarkRenderStateDirty(); }

	int32 GetSelectedBoneIndex() const { return SelectedBoneIndex; }
	void SetSelectedBoneIndex(int32 InBoneIndex) { SelectedBoneIndex = InBoneIndex; MarkRenderStateDirty(); }

	EBoneDebugDrawMode GetDrawMode() const { return DrawMode; }
	void SetDrawMode(EBoneDebugDrawMode InDrawMode) { DrawMode = InDrawMode; MarkRenderStateDirty(); }

private:
	USkeletalMeshComponent* TargetMeshComponent = nullptr;
	int32 SelectedBoneIndex = -1;
	EBoneDebugDrawMode DrawMode = EBoneDebugDrawMode::SelectedOnly;
};
