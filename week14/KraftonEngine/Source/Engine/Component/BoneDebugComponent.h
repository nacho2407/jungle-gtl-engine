#pragma once

#include "Component/PrimitiveComponent.h"

class USkeletalMeshComponent;
class FScene;

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

private:
	USkeletalMeshComponent* TargetMeshComponent = nullptr;
	int32 SelectedBoneIndex = -1;
};
