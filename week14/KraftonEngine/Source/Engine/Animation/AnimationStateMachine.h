#pragma once

#include "Object/Object.h"
#include "Object/FName.h"
#include "Core/CoreTypes.h"
#include "Nodes/AnimNode_StateMachine.h"

class UAnimInstance;
class UAnimState;
struct FPoseContext;

// 데이터 기반 확장 가능 FSM. Phase 1.3 이후 본체는 FAnimNode_StateMachine — 이 wrapper 는
// 외부 API 호환 (RegisterState/Transition / Tick / Evaluate / Inspection getter) 를 유지하면서
// 모든 호출을 내부 노드에 delegate. Phase 1.5+ 에서 RootNode 트리에 박을 때 wrapper 가 노드를
// 직접 노출 (GetNode) 해 트리 구성 가능.
//
// 사용 예 (Phase 1.3 — wrapper API):
//     auto* FSM = UObjectManager::Get().CreateObject<UAnimationStateMachine>();
//     FSM->RegisterState(IdleState);
//     FSM->RegisterTransition({ "Idle", "Walk",
//         [](UAnimInstance* I){ return GetSpeed(I) > 0.1f; }, 0.15f });
//     FSM->SetInitialState("Idle");
//
// 매 프레임:
//     FSM->Tick(Owner, dt);          // 전이 평가 + 활성 상태 시간 진행
//     FSM->Evaluate(Owner, OutPose); // 단일 상태면 그대로, 블렌딩 중이면 N-pose 합성

#include "Source/Engine/Animation/AnimationStateMachine.generated.h"

UCLASS()
class UAnimationStateMachine : public UObject
{
public:
	GENERATED_BODY()
	UAnimationStateMachine() = default;
	~UAnimationStateMachine() override = default;

	void RegisterState(UAnimState* State)                  { Impl.RegisterState(State); }
	void RegisterTransition(const FStateTransition& T)     { Impl.RegisterTransition(T); }
	void SetInitialState(FName StateName)                  { Impl.SetInitialState(StateName); }
	void RequestTransition(FName To, float BlendDuration)  { Impl.RequestTransition(To, BlendDuration); }

	// 매 프레임 호출 — Context 로 wrapping 후 노드 위임.
	void Tick(UAnimInstance* Owner, float DeltaSeconds);
	void Evaluate(UAnimInstance* Owner, FPoseContext& Output);

	FName GetCurrentStateName() const { return Impl.GetCurrentStateName(); }
	bool  IsBlending()          const { return Impl.IsBlending(); }

	// Read-only inspection — Editor debug widget 및 향후 도구 용.
	const TArray<UAnimState*>&      GetStates()        const { return Impl.GetStates(); }
	const TArray<FStateTransition>& GetTransitions()   const { return Impl.GetTransitions(); }
	UAnimState* GetCurrentState()  const { return Impl.GetCurrentState(); }
	UAnimState* GetFromState()     const { return Impl.GetFromState(); }
	float       GetBlendAlpha()    const { return Impl.GetBlendAlpha(); }
	float       GetBlendDuration() const { return Impl.GetBlendDuration(); }
	const TArray<FBlendingFrom>& GetBlendingFroms() const { return Impl.GetBlendingFroms(); }

	// 내부 노드 직접 접근 — Phase 1.5 이후 RootNode 트리 build 시 wrapper 없이 노드를
	// 다른 노드의 자식으로 박을 때 사용. wrapper 가 노드 소유권을 유지하므로 caller 가 lifetime 걱정 X.
	FAnimNode_StateMachine&       GetNode()       { return Impl; }
	const FAnimNode_StateMachine& GetNode() const { return Impl; }

private:
	FAnimNode_StateMachine Impl;
};
