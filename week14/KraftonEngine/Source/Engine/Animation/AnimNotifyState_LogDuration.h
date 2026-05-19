#pragma once

#include "AnimNotifyState.h"
#include "Core/CoreTypes.h"

// Phase A 데모용 구체 NotifyState — Begin/Tick/End 호출 시 콘솔에 로깅.
//   Begin/End 는 1 회씩, Tick 은 활성 구간 매 프레임 호출.
//   Tag 는 시퀀스/임포터가 채워 넣음 (UPROPERTY(Save) 라 .uasset 라운드트립 OK).

#include "Source/Engine/Animation/AnimNotifyState_LogDuration.generated.h"

UCLASS()
class UAnimNotifyState_LogDuration : public UAnimNotifyState
{
public:
	GENERATED_BODY()
	UAnimNotifyState_LogDuration() = default;
	~UAnimNotifyState_LogDuration() override = default;

	UPROPERTY(Edit, Save, Category="NotifyState", DisplayName="Tag")
	FString Tag = "LogDuration";

	void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float TotalDuration) override;
	void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float FrameDeltaTime) override;
	void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
