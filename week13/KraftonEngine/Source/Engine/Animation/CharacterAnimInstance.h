#pragma once

#include "Animation/AnimInstance.h"
#include "Object/FName.h"

// 확장 FSM 데모용 AnimInstance.
//   - Idle / Walk 두 상태를 mock 시퀀스로 등록하고 Speed 값으로 전이.
//   - Speed 는 외부(캐릭터 액터/입력) 가 push 하는 게 정석이지만
//     Phase 5 데모는 입력 의존성을 피하기 위해 bAutoDriveSpeed=true 면
//     NativeUpdateAnimation 안에서 시간 기반 sin 으로 자동 변동시킨다.
//   - FObjectFactory 에 의해 USkeletalMeshComponent 의 AnimationCustom 경로에서
//     이름으로 인스턴스화되므로 UCLASS/GENERATED_BODY 등록이 필수.

#include "Source/Engine/Animation/CharacterAnimInstance.generated.h"

UCLASS()
class UCharacterAnimInstance : public UAnimInstance
{
public:
	GENERATED_BODY()
	UCharacterAnimInstance() = default;
	~UCharacterAnimInstance() override = default;

	// 외부 push 변수 — transition Condition 람다가 이 값을 읽음.
	UPROPERTY(Edit, Category="Animation|Character", DisplayName="Speed", Min=0.0f, Max=100.0f, Speed=0.5f)
	float Speed = 0.0f;

	// 자동 구동(데모). false 로 두면 외부가 Speed 를 직접 갱신해야 함.
	UPROPERTY(Edit, Save, Category="Animation|Character", DisplayName="Auto Drive Speed")
	bool  bAutoDriveSpeed = true;
	UPROPERTY(Edit, Save, Category="Animation|Character", DisplayName="Speed Threshold", Min=0.0f, Max=50.0f, Speed=0.1f)
	float SpeedThreshold  = 10.0f;   // Idle ↔ Walk 임계값
	UPROPERTY(Edit, Save, Category="Animation|Character", DisplayName="Auto Period (s)", Min=0.1f, Max=30.0f, Speed=0.1f)
	float AutoPeriodSec   = 6.0f;    // Speed sin 한 사이클 길이
	UPROPERTY(Edit, Save, Category="Animation|Character", DisplayName="Auto Speed Amp", Min=0.0f, Max=100.0f, Speed=0.5f)
	float AutoSpeedAmp    = 15.0f;   // Speed 진폭 (Speed ∈ [0, 2*Amp])

	// UAnimInstance:
	void NativeInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;
	void EvaluateAnimation(FPoseContext& Output) override;

	// PIE Duplicate / Scene save — Editor-set 데모 파라미터 라운드트립.
	void Serialize(FArchive& Ar) override;

private:
	float ElapsedTime = 0.0f;
};
