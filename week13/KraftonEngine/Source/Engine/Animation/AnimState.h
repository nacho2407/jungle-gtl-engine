#pragma once

#include "Object/Object.h"
#include "Object/FName.h"
#include "Math/Transform.h"

class UAnimSequenceBase;
class UAnimInstance;
struct FPoseContext;

// UAnimationStateMachine 의 한 노드. 상태별 시퀀스/속도/루프 등을 들고,
// 진입 시 LocalTime 을 리셋, Tick 에서 시간 진행, Evaluate 에서 포즈 샘플링.

#include "Source/Engine/Animation/AnimState.generated.h"

UCLASS()
class UAnimState : public UObject
{
public:
	GENERATED_BODY()
	UAnimState() = default;
	~UAnimState() override = default;

	// 상태 식별자 (FSM 등록 시 키로 사용).
	FName StateName = FName::None;

	// 이 상태가 재생할 시퀀스. nullptr 이면 ref pose 유지.
	UAnimSequenceBase* Sequence = nullptr;
	float              PlayRate = 1.0f;
	bool               bLooping = true;

	// 후크 — 데이터 기반 FSM 에서는 대부분 기본 구현만으로 충분하지만,
	// 자식이 진입 효과/특수 평가를 넣을 수 있도록 가상함수로 남긴다.
	virtual void OnEnter(UAnimInstance* Instance) { (void)Instance; LocalTime = 0.0f; }
	virtual void OnExit (UAnimInstance* Instance) { (void)Instance; }
	virtual void Tick   (UAnimInstance* Instance, float DeltaSeconds);
	virtual void Evaluate(UAnimInstance* Instance, FPoseContext& Output);

	float GetLocalTime() const { return LocalTime; }
	void  SetLocalTime(float T) { LocalTime = T; }

	// Tick 동안 계산된 root motion delta (Sequence 가 bEnableRootMotion 일 때만 의미).
	// FSM 이 blend 중 두 상태의 delta 를 weight lerp 후 AnimInstance 에 누적.
	const FTransform& GetLastRootMotionDelta() const { return LastRootMotionDelta; }

protected:
	float      LocalTime = 0.0f;
	FTransform LastRootMotionDelta;   // 매 Tick 후 갱신; FSM 이 Evaluate 후 읽음
};
