#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/WeakObjectPtr.h"

#include "Source/Engine/Component/Debug/BoneDebugComponent.generated.h"
class USkeletalMeshComponent;
class FScene;

enum class EBoneDebugDrawMode : uint8
{
	SelectedOnly,
	AllBones
};

UCLASS()
class UBoneDebugComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()
	UBoneDebugComponent();
	~UBoneDebugComponent() override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	USkeletalMeshComponent* GetTargetMeshComponent() const { return TargetMeshComponent.Get(); }
	void SetTargetMeshComponent(USkeletalMeshComponent* InMeshComponent) { TargetMeshComponent = InMeshComponent; MarkRenderStateDirty(); }

	int32 GetSelectedBoneIndex() const { return SelectedBoneIndex; }
	void SetSelectedBoneIndex(int32 InBoneIndex) { SelectedBoneIndex = InBoneIndex; MarkRenderStateDirty(); }

	void AddReferencedObjects(FReferenceCollector& Collector) override;

	EBoneDebugDrawMode GetDrawMode() const { return DrawMode; }
	void SetDrawMode(EBoneDebugDrawMode InDrawMode) { DrawMode = InDrawMode; MarkRenderStateDirty(); }

private:
	// Non-owning debug target. The skeletal mesh component is owned by its actor.
	TWeakObjectPtr<USkeletalMeshComponent> TargetMeshComponent;
	int32 SelectedBoneIndex = -1;
	EBoneDebugDrawMode DrawMode = EBoneDebugDrawMode::SelectedOnly;
};
