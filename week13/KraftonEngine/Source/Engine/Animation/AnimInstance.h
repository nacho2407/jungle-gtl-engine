#pragma once

#include "Object/Object.h"
#include "PoseContext.h"
#include "AnimNotifyEvent.h"
#include "Math/Transform.h"

class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequenceBase;

// 큐에 적재된 한 프레임 분의 notify — dispatch 시 Sequence 컨텍스트 보존 위해 같이 들고 다님.
// (UE 의 FAnimNotifyEventReference 와 의미 대응 — 우리는 value-copy 가 더 안전.)
struct FQueuedAnimNotify
{
	FAnimNotifyEvent         Event;
	const UAnimSequenceBase* Sequence = nullptr;
};

// 모든 애니메이션 인스턴스의 베이스. SkeletalMeshComponent 1개에 1개 인스턴스.
//
// 라이프사이클:
//   1) UObjectManager 로 생성 후 SetOwningComponent 로 소속 컴포넌트 지정
//   2) NativeInitializeAnimation() — 1회 초기화 후크
//   3) 매 프레임 UpdateAnimation(dt) 호출
//        → NativeUpdateAnimation(dt)        — 자식이 시간 진행 + AddAnimNotifies(prev, cur, seq) 호출
//        → DispatchQueuedAnimEvents()       — 베이스가 큐 비우면서 일괄 dispatch (Notify→Notify + HandleAnimNotify)
//      그 후 호출자가 EvaluatePose(Output) — 결과를 OwningComponent 의 본 edit pose 로 푸시.
//
// Notify dispatch 통합: SingleNode 든 Custom+FSM 이든 자식은 AddAnimNotifies 만 호출 (큐에 push).
// 실제 dispatch 는 UpdateAnimation 끝에서 베이스가 한 번 수행 — UE 의 DispatchQueuedAnimEvents 패턴.
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
	// 매 프레임 호출. NativeUpdate → DispatchQueuedAnimEvents → (호출자가) EvaluatePose.
	void UpdateAnimation(float DeltaSeconds);
	void EvaluatePose(FPoseContext& Output);

	// ── 컴포넌트 접근 ──
	void SetOwningComponent(USkeletalMeshComponent* InComp) { OwningComponent = InComp; }
	USkeletalMeshComponent* GetOwningComponent() const { return OwningComponent; }
	USkeletalMesh*          GetSkeletalMesh()    const;

	// ── Notify ──
	// 자식(SingleNode, FSM 노드) 이 시퀀스 [PreviousTime, CurrentTime) 구간을 재생한 직후 호출.
	// 시간 범위 내 notify 들을 큐에 적재만 함 — 즉시 dispatch 하지 않음.
	// 루프 경계는 [Prev, Length) ∪ [0, Cur) 로 처리.
	void AddAnimNotifies(float PreviousTime, float CurrentTime, const UAnimSequenceBase* Sequence);

	// UpdateAnimation 끝에서 자동 호출 — 큐 비우면서 각 notify dispatch.
	//   1) Q.Event.Notify->Notify(comp, seq)  — UE 패턴 (시퀀스가 소유한 로직 객체)
	//   2) HandleAnimNotify(Q.Event)          — fallback 후크 (Owner-상태 의존 로직)
	void DispatchQueuedAnimEvents();

	// fallback 후크 — Notify 객체가 없거나, 추가 처리 필요할 때 자식이 오버라이드.
	virtual void HandleAnimNotify(const FAnimNotifyEvent& Notify) { (void)Notify; }

	// Read-only inspection — Editor debug widget 용. dispatch 직후의 ring buffer (capacity RecentNotifyCapacity).
	// 가장 오래된 것이 [0], 가장 최근이 [size-1].
	const TArray<FQueuedAnimNotify>& GetRecentNotifies() const { return RecentNotifies; }

	// ── Root Motion ──
	// 자식 (SingleNode/FSM) 이 UpdateAnimation 안에서 AccumulateRootMotion 으로 누적.
	// SkeletalMeshComponent 가 매 프레임 Tick 끝에서 ConsumeRootMotion 로 소비 후
	// owning actor 의 transform 에 적용. 소비 시 누적 buffer 0 으로 초기화.
	// Delta 는 root 본 local 좌표계 — 호출자가 actor world frame 으로 변환해야 함.
	void AccumulateRootMotion(const FTransform& Delta);
	FTransform ConsumeRootMotion();

protected:
	USkeletalMeshComponent*       OwningComponent = nullptr;
	TArray<FQueuedAnimNotify>     NotifyQueue;

	// 디버그용 최근 발사 notify 이력. DispatchQueuedAnimEvents 에서 push.
	// 비용: dispatch 당 push 1회 + cap 초과 시 erase front 1회 — 무시 가능.
	static constexpr size_t       RecentNotifyCapacity = 10;
	TArray<FQueuedAnimNotify>     RecentNotifies;

	// Root motion 누적 (UpdateAnimation 한 프레임 분, Consume 시 reset).
	FTransform                    PendingRootMotion;
};
