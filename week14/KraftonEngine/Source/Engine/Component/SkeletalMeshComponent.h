#pragma once

#include "SkinnedMeshComponent.h"

#include "Source/Engine/Component/SkeletalMeshComponent.generated.h"
// SkeletalMesh 전용 render proxy만 제공하는 얇은 wrapper.
// Skinning/bone/material/bounds 상태는 모두 USkinnedMeshComponent가 소유한다.
UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	GENERATED_BODY()
	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	// Render access 섹션: SceneProxy
	FPrimitiveSceneProxy* CreateSceneProxy() override;
};
