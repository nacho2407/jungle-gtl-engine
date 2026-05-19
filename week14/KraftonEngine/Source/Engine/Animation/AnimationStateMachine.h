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

// 진행중 from-state 한 항목. 빠른 연쇄 transition (A→B blend 중 B→C) 시
// 단일 FromState 로는 A 가 폐기되어 시각 끊김 → 모든 from 을 stack 으로 보존.
// Alpha 는 "이 from 이 *그 다음 단계의 state* (= 더 최근 BlendingFrom 또는 CurrentState)
// 로 fade-out 된 진행도" 의미. 매 Tick 에서 dt/Duration 만큼 증가, 1.0 도달 시 OnExit + 제거.
struct FBlendingFrom
{
	UAnimState* State    = nullptr;
	float       Alpha    = 0.0f;
	float       Duration = 0.0f;
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

#include "Source/Engine/Animation/AnimationStateMachine.generated.h"

UCLASS()
class UAnimationStateMachine : public UObject
{
public:
	GENERATED_BODY()
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
	bool  IsBlending() const          { return !BlendingFroms.empty(); }

	// Read-only inspection — Editor debug widget 및 향후 도구 (Lua inspect 등) 용.
	const TArray<UAnimState*>&      GetStates()        const { return States; }
	const TArray<FStateTransition>& GetTransitions()   const { return Transitions; }
	UAnimState* GetCurrentState()  const { return CurrentState; }

	// 호환용 single-from getter — "가장 최근 from" 반환. multi-from 정보가 필요하면 GetBlendingFroms.
	UAnimState* GetFromState()     const { return BlendingFroms.empty() ? nullptr : BlendingFroms.back().State; }
	float       GetBlendAlpha()    const { return BlendingFroms.empty() ? 1.0f   : BlendingFroms.back().Alpha; }
	float       GetBlendDuration() const { return BlendingFroms.empty() ? 0.0f   : BlendingFroms.back().Duration; }

	// Multi-blend inspection — 진행중 모든 from 항목 (oldest=[0], latest=back).
	const TArray<FBlendingFrom>& GetBlendingFroms() const { return BlendingFroms; }

private:
	UAnimState* FindState(FName Name) const;
	void        EnterState(UAnimInstance* Owner, FName NewState);
	void        BeginBlend(UAnimInstance* Owner, FName NewState, float BlendDuration);

	TArray<UAnimState*>     States;       // FName 키 → 선형 탐색 (보통 <20 개)
	TArray<FStateTransition> Transitions;

	FName       CurrentStateName = FName::None;
	UAnimState* CurrentState     = nullptr;

	// Multi-blend 진행중 from 스택 — A→B→C 같은 빠른 연쇄에서 모든 from 을 보존.
	// oldest 가 [0], latest 가 back. Tick 에서 dt/Duration 만큼 alpha 증가, 1.0 도달분 OnExit + erase.
	// 빠른 연쇄로 무한 grow 막기 위해 한도 도달 시 oldest 강제 정리.
	TArray<FBlendingFrom>     BlendingFroms;
	static constexpr int32    MaxBlendingFroms = 4;
};
