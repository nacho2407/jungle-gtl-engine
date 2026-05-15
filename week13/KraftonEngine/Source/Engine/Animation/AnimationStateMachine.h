#pragma once

#include "Object/Object.h"
#include "Object/FName.h"

class UAnimInstance;
class UAnimState;
struct FPoseContext;

// 상태 간 전이 규칙. From == FName::None 이면 AnyState 전이 (예: 사망/피격).
//
// Condition 은 람다로 캡슐화 — 새 전이 추가 시 엔진 코드 수정 없이 등록만으로 가능.
// BlendTime 동안 두 상태 출력이 BlendTwoPosesTogether 로 섞인다.
struct FStateTransition
{
	FName From;     // FName::None == AnyState
	FName To;
	TFunction<bool(UAnimInstance*)> Condition;
	float BlendTime = 0.2f;
};

// 데이터 기반 확장 가능 FSM.
//
// 사용 예:
//     auto* FSM = UObjectManager::Get().CreateObject<UAnimationStateMachine>();
//     FSM->RegisterState(IdleState);
//     FSM->RegisterState(WalkState);
//     FSM->RegisterTransition({ "Idle", "Walk",
//         [](UAnimInstance* I){ return GetSpeed(I) > 0.1f; }, 0.15f });
//     FSM->SetInitialState("Idle");
//
// 매 프레임:
//     FSM->Tick(Owner, dt);          // 전이 평가 + 활성 상태 시간 진행
//     FSM->Evaluate(Owner, OutPose); // 단일 상태면 그대로, 블렌딩 중이면 두 포즈 섞기
class UAnimationStateMachine : public UObject
{
public:
	DECLARE_CLASS(UAnimationStateMachine, UObject)

	UAnimationStateMachine() = default;
	~UAnimationStateMachine() override = default;

	// 등록 API — 호출 순서 자유. 같은 이름 재등록 시 기존 항목 덮어쓰기.
	void RegisterState(UAnimState* State);
	void RegisterTransition(const FStateTransition& T);

	// 시작 상태 지정. 미지정 시 첫 RegisterState 가 자동으로 시작 상태.
	void SetInitialState(FName StateName);

	// 매 프레임:
	void Tick(UAnimInstance* Owner, float DeltaSeconds);
	void Evaluate(UAnimInstance* Owner, FPoseContext& Output);

	// 외부 트리거(피격/사망 등)에서 강제 전이.
	void RequestTransition(FName To, float BlendDuration);

	FName GetCurrentStateName() const { return CurrentStateName; }
	bool  IsBlending() const          { return FromState != nullptr; }

private:
	UAnimState* FindState(FName Name) const;
	void        EnterState(UAnimInstance* Owner, FName NewState);
	void        BeginBlend(UAnimInstance* Owner, FName NewState, float BlendDuration);
	void        FinishBlend(UAnimInstance* Owner);

	TArray<UAnimState*>     States;       // FName 키 → 선형 탐색 (보통 <20 개)
	TArray<FStateTransition> Transitions;

	FName       CurrentStateName = FName::None;
	UAnimState* CurrentState     = nullptr;

	// 블렌딩 상태 (FromState != nullptr 일 때 활성).
	UAnimState* FromState        = nullptr;
	float       BlendAlpha       = 1.0f;  // 0 → FromState, 1 → CurrentState
	float       BlendDuration    = 0.0f;
};
