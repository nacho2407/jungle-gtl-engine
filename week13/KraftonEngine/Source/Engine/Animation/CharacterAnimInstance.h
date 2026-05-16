#pragma once

#include "Animation/AnimInstance.h"
#include "Object/FName.h"

class UAnimationStateMachine;

// 확장 FSM 데모용 AnimInstance.
//   - Idle / Walk 두 상태를 mock 시퀀스로 등록하고 Speed 값으로 전이.
//   - Speed 는 외부(캐릭터 액터/입력) 가 push 하는 게 정석이지만
//     Phase 5 데모는 입력 의존성을 피하기 위해 bAutoDriveSpeed=true 면
//     NativeUpdateAnimation 안에서 시간 기반 sin 으로 자동 변동시킨다.
//   - FObjectFactory 에 의해 USkeletalMeshComponent 의 AnimationCustom 경로에서
//     이름으로 인스턴스화되므로 IMPLEMENT_CLASS 필수.
class UCharacterAnimInstance : public UAnimInstance
{
public:
	DECLARE_CLASS(UCharacterAnimInstance, UAnimInstance)

	UCharacterAnimInstance() = default;
	~UCharacterAnimInstance() override = default;

	// 외부 push 변수 — transition Condition 람다가 이 값을 읽음.
	float Speed = 0.0f;

	// 자동 구동(데모). false 로 두면 외부가 Speed 를 직접 갱신해야 함.
	bool  bAutoDriveSpeed = true;
	float SpeedThreshold  = 10.0f;   // Idle ↔ Walk 임계값
	float AutoPeriodSec   = 6.0f;    // Speed sin 한 사이클 길이
	float AutoSpeedAmp    = 15.0f;   // Speed 진폭 (Speed ∈ [0, 2*Amp])

	// UAnimInstance:
	void NativeInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;
	void EvaluateAnimation(FPoseContext& Output) override;

	// Editor 노출 — Speed 등 데모 파라미터. PostEdit 처리 불필요 (직접 멤버 mutation).
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	UAnimationStateMachine* GetFSM() const { return FSM; }

private:
	UAnimationStateMachine* FSM = nullptr;
	float ElapsedTime = 0.0f;
};
