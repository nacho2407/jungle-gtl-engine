#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Core/Types/CoreTypes.h"

// 사운드 1회 재생 instant notify. 발자국 / 무기 swing / impact 같은 단발성 트리거에 사용.
//   - SoundPath 는 Content/Audio 하위 상대 경로 (예: "footstep_grass.wav").
//   - Volume / Pitch 는 UPROPERTY(Edit, Save) — Notify Properties 패널에서 편집 + .uasset round-trip.
//   - 첫 트리거 시 AudioManager 에 자동 LoadAudio (캐시 키 = "AnimNotify:" + path), 이후 PlayAudio.
//   - AudioManager 미초기화 (예: 에디터 비-PIE) 시 LoadAudio/PlayAudio 가 early-out → 무영향.

#include "Source/Engine/Animation/Notify/AnimNotify_PlaySound.generated.h"

UCLASS()
class UAnimNotify_PlaySound : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_PlaySound() = default;
	~UAnimNotify_PlaySound() override = default;

	UPROPERTY(Edit, Save, Category="PlaySound", DisplayName="Sound Path")
	FString SoundPath;

	UPROPERTY(Edit, Save, Category="PlaySound", DisplayName="Volume")
	float Volume = 1.0f;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
