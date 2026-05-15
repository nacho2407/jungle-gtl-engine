#pragma once

#include "Object/Object.h"
#include "PoseContext.h"
#include "AnimNotifyEvent.h"

class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequenceBase;

// 모든 애니메이션 인스턴스의 베이스. SkeletalMeshComponent 1개에 1개 인스턴스.
//
// 라이프사이클:
//   1) UObjectManager 로 생성 후 SetOwningComponent 로 소속 컴포넌트 지정
//   2) NativeInitializeAnimation() — 1회 초기화 후크
//   3) 매 프레임 UpdateAnimation(dt) 호출
//        → NativeUpdateAnimation(dt)  // 변수 갱신 (Pawn 속도/MovementMode 읽기 등)
//        → EvaluateAnimation(Output)  // 실제 포즈 계산
//        → 결과를 OwningComponent 의 본 edit pose 로 푸시 (호출 측 책임)
class UAnimInstance : public UObject
{
public:
	DECLARE_CLASS(UAnimInstance, UObject)

	UAnimInstance() = default;
	~UAnimInstance() override = default;

	// ── 후크 ──
	virtual void NativeInitializeAnimation() {}
	virtual void NativeUpdateAnimation(float DeltaSeconds) { (void)DeltaSeconds; }
	virtual void EvaluateAnimation(FPoseContext& Output) { (void)Output; }

	// ── 외부 진입점 ──
	// 매 프레임 호출. NativeUpdate → Evaluate → (호출자가) 결과를 컴포넌트에 푸시.
	void UpdateAnimation(float DeltaSeconds);
	void EvaluatePose(FPoseContext& Output);

	// ── 컴포넌트 접근 ──
	void SetOwningComponent(USkeletalMeshComponent* InComp) { OwningComponent = InComp; }
	USkeletalMeshComponent* GetOwningComponent() const { return OwningComponent; }
	USkeletalMesh*          GetSkeletalMesh()    const;

	// ── Notify ──
	// [PreviousTime, CurrentTime) 구간을 가로지른 Notify 들을 모아 HandleAnimNotify 호출.
	// 루프 경계는 [Prev, Length) ∪ [0, Current) 로 처리.
	void TriggerAnimNotifies(float PreviousTime, float CurrentTime, const UAnimSequenceBase* Sequence);
	virtual void HandleAnimNotify(const FAnimNotifyEvent& Notify) { (void)Notify; }

protected:
	USkeletalMeshComponent* OwningComponent = nullptr;
};
