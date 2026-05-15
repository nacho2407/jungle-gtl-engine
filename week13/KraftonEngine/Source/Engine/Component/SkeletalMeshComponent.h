#pragma once

#include "SkinnedMeshComponent.h"
#include "Asset/AssetPackage.h"

class UAnimSequenceBase;
// SkeletalMesh 전용 render proxy만 제공하는 얇은 wrapper.
// Skinning/bone/material/bounds 상태는 모두 USkinnedMeshComponent가 소유한다.
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	// Render access 섹션: SceneProxy
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void PlayAnimation(UAnimSequenceBase* NewAnimToPlay, bool bLooping);
	void StopAnimation();

	UAnimSequenceBase* GetAnimation() const
	{
		return AnimationToPlay;
	}

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	UAnimSequenceBase* AnimationToPlay = nullptr;

	float CurrentAnimTime   = 0.0f;
	float PlayRate          = 1.0f;
	bool  bLoopAnimation    = true;
	bool  bPlayingAnimation = false;
};
