#pragma once

#include "Object/Object.h"
#include "Object/FName.h"
#include "Math/Transform.h"
#include "Animation/Nodes/AnimNode_SequencePlayer.h"

class UAnimSequenceBase;
class UAnimInstance;
struct FPoseContext;

// FAnimNode_StateMachine 의 한 상태. 상태별 시퀀스/속도/루프 등을 들고, 진입 시 LocalTime
// 을 리셋, Tick 에서 시간 진행, Evaluate 에서 포즈 샘플링.
//
// 두 가지 모드:
//   A) Sequence 직접 — 외부 public 필드 (Sequence/PlayRate/bLooping) 를 내부 FAnimNode_
//      SequencePlayer 한 개에 sync 후 위임. 단순 leaf state.
//   B) SubGraphOverride — 임의 FAnimNode_Base (예: sub-SM) 를 박아 trees 안의 sub-tree 로.
//      OnEnter 시 SubGraph.OnBecomeRelevant, OnExit 시 SubGraph.OnDormant 후크 호출 — sub-SM
//      의 BlendingFroms 등 transient 정리.

#include "Source/Engine/Animation/StateMachine/AnimState.generated.h"

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
	virtual void OnEnter(UAnimInstance* Instance);
	virtual void OnExit (UAnimInstance* Instance);
	// Weight 인자 — 부모 SM 이 자기 visible 비율을 전달. 자식 SequencePlayer 의
	// AddAnimNotifies 가 가시성 임계 가드에 사용. default 1.0 (단일 진입).
	virtual void Tick   (UAnimInstance* Instance, float DeltaSeconds, float Weight = 1.0f);
	virtual void Evaluate(UAnimInstance* Instance, FPoseContext& Output);

	float GetLocalTime() const { return LocalTime; }
	void  SetLocalTime(float T) { LocalTime = T; }

	// Tick 동안 계산된 root motion delta (Sequence 가 bEnableRootMotion 일 때만 의미).
	// FSM 이 blend 중 두 상태의 delta 를 weight lerp 후 AnimInstance 에 누적.
	const FTransform& GetLastRootMotionDelta() const { return LastRootMotionDelta; }

	// Phase 1.5b+: 임의 FAnimNode_Base 를 state 의 sub-graph 로 사용. set 되면 internal Player
	// 무시하고 SubGraphOverride 에 시간 진행 / pose 평가 / RM 모두 위임. sub-state-machine
	// (Locomotion 안에 Idle/Walk sub-SM) 같은 트리 구성에 사용.
	FAnimNode_Base* SubGraphOverride = nullptr;

protected:
	float      LocalTime = 0.0f;
	FTransform LastRootMotionDelta;   // 매 Tick 후 갱신; FSM 이 Evaluate 후 읽음

	// 실제 시간 진행 / pose 평가 / RM 계산 로직은 이 노드가 담당. 외부 멤버와 양방향 sync.
	FAnimNode_SequencePlayer Player;
};
