#pragma once

#include "AnimNode_Base.h"
#include "Math/Transform.h"
#include "Object/FName.h"
#include "Core/Types/CoreTypes.h"

class UAnimInstance;
class UAnimState;

// 상태 간 전이 규칙. From == FName::None 이면 AnyState 전이 (예: 사망/피격).
// Condition 은 람다로 캡슐화 — 새 전이 추가 시 엔진 코드 수정 없이 등록만으로 가능.
// BlendTime 동안 두 상태 출력이 BlendTwoPosesTogether 로 섞인다.
struct FStateTransition
{
	FName From;     // FName::None == AnyState
	FName To;
	TFunction<bool(UAnimInstance*)> Condition;
	float BlendTime = 0.2f;
};

// 진행중 from-state 한 항목. 빠른 연쇄 transition 시 stack 으로 보존해 multi-blend.
// Alpha 는 "이 from 이 그 다음 단계 state (= 더 최근 BlendingFrom 또는 CurrentState)
// 로 fade-out 된 진행도" 의미. 매 Update 에서 dt/Duration 만큼 증가, 1.0 도달 시 OnExit + 제거.
struct FBlendingFrom
{
	UAnimState* State    = nullptr;
	float       Alpha    = 0.0f;
	float       Duration = 0.0f;
};

// FSM 노드 — transition 평가 / multi-blend stack / root motion sequential lerp /
// N-pose evaluate 까지 단일 노드 안에서. AnimGraph 트리에 박혀 sub-state-machine 도 자연
// 표현 (state 의 SubGraphOverride 가 또 다른 FAnimNode_StateMachine).
// Initialize/OnBecomeRelevant 가 CurrentState.OnEnter, OnDormant 가 BlendingFroms / CurrentState
// OnExit 로 transient state 정리.
class FAnimNode_StateMachine : public FAnimNode_Base
{
public:
	// Build API — 자식 AnimInstance 가 호출.
	void RegisterState(UAnimState* State);
	void RegisterTransition(const FStateTransition& T);
	void SetInitialState(FName StateName);
	void RequestTransition(FName To, float BlendDuration);

	// AnimNode interface.
	void Initialize(const FAnimationInitializeContext& Context) override;
	void OnBecomeRelevant(const FAnimationInitializeContext& Context) override;
	void OnDormant() override;
	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;
	const FTransform& GetLastRootMotionDelta() const override { return LastRootMotionDelta; }
	const char* GetDebugName() const override { return "StateMachine"; }

	// Inspection.
	FName       GetCurrentStateName() const { return CurrentStateName; }
	UAnimState* GetCurrentState()     const { return CurrentState; }
	bool        IsBlending()          const { return !BlendingFroms.empty(); }
	UAnimState* GetFromState()        const { return BlendingFroms.empty() ? nullptr : BlendingFroms.back().State; }
	float       GetBlendAlpha()       const { return BlendingFroms.empty() ? 1.0f   : BlendingFroms.back().Alpha; }
	float       GetBlendDuration()    const { return BlendingFroms.empty() ? 0.0f   : BlendingFroms.back().Duration; }

	const TArray<UAnimState*>&      GetStates()         const { return States; }
	const TArray<FStateTransition>& GetTransitions()    const { return Transitions; }
	const TArray<FBlendingFrom>&    GetBlendingFroms()  const { return BlendingFroms; }

private:
	UAnimState* FindState(FName Name) const;
	void        EnterState(UAnimInstance* Owner, FName NewState);
	void        BeginBlend(UAnimInstance* Owner, FName NewState, float Duration);

	TArray<UAnimState*>      States;
	TArray<FStateTransition> Transitions;

	FName       CurrentStateName = FName::None;
	UAnimState* CurrentState     = nullptr;

	// Multi-blend 진행중 from 스택 (oldest=[0], latest=back). 한도 도달 시 oldest 강제 정리.
	TArray<FBlendingFrom>     BlendingFroms;
	static constexpr int32    MaxBlendingFroms = 4;

	// 매 Update 가 자식들의 LastRM 을 sequential lerp 한 결과. 외부 (부모 SM 또는 AnimInstance)
	// 가 누적 책임. SM 자체는 AccumulateRootMotion 호출 안 함 — 이중 누적 방지.
	FTransform LastRootMotionDelta;
};
