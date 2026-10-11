#pragma once

#include "AnimNode_Base.h"
#include "Math/Transform.h"
#include "Core/Types/CoreTypes.h"

// Enum 값으로 N 개의 입력 pose 중 하나를 선택하는 노드.
// StateMachine 의 단순화 버전 — transitions 없음, multi-blend stack 없음.
//   InputPoses[i] = enum value i 에 해당하는 입력 노드.
//   ActiveChildIndex = 외부 (lua 또는 C++) 가 매 update 전에 설정.
//   값이 바뀌면 BlendTime 동안 이전 child 와 현재 child 의 pose 를 lerp.
//
// 사용 예: 무장/비무장, 자세 (Stand/Crouch/Prone), 무기 종류 (Sword/Bow/Gun) 같이
// 명확한 enum 분기. State 마다 transition 평가 / multi-blend stack 가 필요 없을 때 가성비.
//
// Multi-blend 미지원: 빠른 enum 토글 시 latest pair 만 추적 (Previous → Current). 그 사이에
// 또 바뀌면 진행중 blend 는 즉시 종료, 새 Previous=직전 Current 로 reset. StateMachine 의
// BlendingFroms 스택 대비 단순/저비용. UE 동일 동작.
class FAnimNode_BlendListByEnum : public FAnimNode_Base
{
public:
	// Build-side — lua/C++ 가 채움.
	TArray<FAnimNode_Base*> InputPoses;
	int32                   ActiveChildIndex = 0;   // 외부에서 매 update 갱신 (SelectorFn 미설정 시).
	float                   BlendTime        = 0.2f;

	// 선택사항 — 설정되어 있으면 매 Update 시작 시 호출해 ActiveChildIndex 자동 갱신.
	// AnimGraph 컴파일러가 Selector input 의 source (VariableGet 노드) 를 reflection 람다로
	// inline. 비어있으면 기존 동작 (외부가 ActiveChildIndex 박음) 유지 — UCharacterAnimInstance 호환.
	TFunction<int32(UAnimInstance*)> SelectorFn;

	void Initialize(const FAnimationInitializeContext& Context) override;
	void OnBecomeRelevant(const FAnimationInitializeContext& Context) override;
	void OnDormant() override;
	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;
	const FTransform& GetLastRootMotionDelta() const override { return LastRootMotionDelta; }
	const char* GetDebugName() const override { return "BlendListByEnum"; }

	// Inspection — debug widget.
	int32 GetCurrentChildIndex()  const { return CurrentChildIndex; }
	int32 GetPreviousChildIndex() const { return PreviousChildIndex; }
	float GetBlendAlpha()         const { return BlendAlpha; }

private:
	// Initialize 시점에 ActiveChildIndex 와 동기화. Update 에서 ActiveChild != Current 면 전이 시작.
	int32 CurrentChildIndex  = -1;
	int32 PreviousChildIndex = -1;    // blend out 중인 child 인덱스 (-1 = 진행중 전이 없음)
	float BlendAlpha         = 1.0f;  // 0 = previous full, 1 = current full

	// Update 가 lerp(Previous.LastRM, Current.LastRM, alpha) 로 채움. AccumulateRootMotion 직접 X.
	FTransform LastRootMotionDelta;

	bool IsValidIndex(int32 Idx) const
	{
		return Idx >= 0 && Idx < static_cast<int32>(InputPoses.size()) && InputPoses[Idx] != nullptr;
	}
};
