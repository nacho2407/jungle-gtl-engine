#pragma once

#include "AnimNodeContexts.h"
#include "Math/Transform.h"

struct FPoseContext;

// 모든 anim 노드의 베이스. UAnimInstance::RootNode 가 트리의 root 를 보유, 매 frame
// SkeletalMeshComponent::EvaluateAnimInstance 가 UpdateAnimation → EvaluatePose 흐름에서
// RootNode->Update / Evaluate 를 호출해 최종 pose 를 만든다.
//
// 라이프사이클:
//   1) Initialize(ctx)         — 트리 build 직후 1 회. 자식 노드의 Initialize 는 자기 책임으로
//                                재귀 호출 (부모가 자식 보유 패턴이라 트리 순회 자연스러움).
//   2) OnBecomeRelevant(ctx)   — StateMachine 노드가 새 state 진입 시 그 state 의 SubGraph 에
//                                호출. SequencePlayer 등이 override 해서 LocalTime reset 등.
//   3) Update(ctx)             — 매 frame. 시간 진행 / transition 평가 / notify 적재 / RM 누적.
//                                Pose 는 만지지 않음 (= 가벼움, weight 0 인 가지 skip 가능).
//                                자식 Update 도 자기가 재귀 호출 (FractionalWeight 로 ctx 전파).
//   4) Evaluate(out)           — 매 frame, Update 직후. 입력 pose 합성해 out 채움.
//
// 메모리 / 소유:
//   - plain class (UObject 아님) — reflection / Outer 비용 없이 매 frame 수십 번 평가 안전.
//   - AnimInstance 가 TArray<unique_ptr<FAnimNode_Base>> 로 노드 보유. 노드 간 참조는 raw pointer.
//   - 직렬화 안 함. 자식 AnimInstance 가 NativeInitializeAnimation 에서 매번 코드로 트리 build.
class FAnimNode_Base
{
public:
	virtual ~FAnimNode_Base() = default;

	virtual void Initialize(const FAnimationInitializeContext& Context)         { (void)Context; }
	virtual void OnBecomeRelevant(const FAnimationInitializeContext& Context)   { (void)Context; }
	virtual void Update(const FAnimationUpdateContext& Context)                 { (void)Context; }
	virtual void Evaluate(FPoseContext& Output) = 0;

	// 매 Update 가 마지막으로 계산한 root motion delta.
	//   SequencePlayer — 자기 sequence 의 ExtractRootMotion 결과
	//   StateMachine — 자식 노드들의 LastRM 을 multi-blend sequential lerp 결과
	// 노드 자체는 AnimInstance->AccumulateRootMotion 을 호출하지 않는다 (= 외부 누적 패턴).
	// AnimInstance::UpdateAnimation 끝에서 RootNode 의 값 한 번만 누적해 트리 깊이와
	// 무관하게 이중 누적 위험 0. mode 체크도 그 한 곳에서.
	virtual const FTransform& GetLastRootMotionDelta() const
	{
		static const FTransform Identity;
		return Identity;
	}

	virtual const char* GetDebugName() const { return "AnimNode"; }
};
