#pragma once

#include "Object/Object.h"
#include "Animation/PoseContext.h"
#include "Animation/Notify/AnimNotifyEvent.h"
#include "Math/Transform.h"
#include "Object/FName.h"
#include "Animation/AnimationMode.h"
#include "Animation/Nodes/AnimNode_Base.h"

#include <memory>
#include <utility>

class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequenceBase;
class UAnimMontage;
class UAnimMontageInstance;
class UAnimNotifyState;
class APawn;

// 큐에 적재된 한 프레임 분의 notify — dispatch 시 Sequence 컨텍스트 보존 위해 같이 들고 다님.
// (UE 의 FAnimNotifyEventReference 와 의미 대응 — 우리는 value-copy 가 더 안전.)
struct FQueuedAnimNotify
{
	FAnimNotifyEvent         Event;
	const UAnimSequenceBase* Sequence = nullptr;
};

// 활성 상태로 추적 중인 NotifyState 한 항목 — 매 프레임 [Prev, Cur) 와 이벤트 구간이 겹치면
// bSeenThisFrame 가 true. 프레임 끝에서 false 인 항목은 NotifyEnd 후 제거.
// 시퀀스 전환 / weight drop 등 자연스러운 종료 모두 같은 경로 (다음 프레임에 안 보임 → End).
struct FActiveAnimNotifyState
{
	UAnimNotifyState*        State          = nullptr;
	const UAnimSequenceBase* Sequence       = nullptr;
	FName                    NotifyName;
	float                    TotalDuration  = 0.0f;
	bool                     bSeenThisFrame = false;
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

#include "Source/Engine/Animation/AnimInstance.generated.h"

UCLASS()
class UAnimInstance : public UObject
{
public:
	GENERATED_BODY()
	UAnimInstance() = default;
	~UAnimInstance() override = default;

	// ── 후크 ──
	virtual void NativeInitializeAnimation() {}
	virtual void NativeUpdateAnimation(float DeltaSeconds) { (void)DeltaSeconds; }
	virtual void EvaluateAnimation(FPoseContext& Output) { (void)Output; }
	virtual void PostEvaluatePose(FPoseContext& Output) { (void)Output; }

	// ── 외부 진입점 ──
	// 매 프레임 호출. NativeUpdate → DispatchQueuedAnimEvents → (호출자가) EvaluatePose.
	void UpdateAnimation(float DeltaSeconds);
	void EvaluatePose(FPoseContext& Output);

	// ── 컴포넌트 접근 ──
	void SetOwningComponent(USkeletalMeshComponent* InComp) { OwningComponent = InComp; }
	USkeletalMeshComponent* GetOwningComponent() const { return OwningComponent; }
	USkeletalMesh*          GetSkeletalMesh()    const;

	APawn* TryGetPawnOwner() const;

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

	// 현재 활성 중인 NotifyState 목록 — Editor 디버그 위젯 노출용.
	const TArray<FActiveAnimNotifyState>& GetActiveNotifyStates() const { return ActiveNotifyStates; }

	// ── Root Motion ──
	// 자식 (SingleNode/FSM) 이 UpdateAnimation 안에서 AccumulateRootMotion 으로 누적.
	// SkeletalMeshComponent 가 매 프레임 Tick 끝에서 ConsumeRootMotion 로 소비 후
	// owning actor 의 transform 에 적용. 소비 시 누적 buffer 0 으로 초기화.
	// Delta 는 root 본 local 좌표계 — 호출자가 actor world frame 으로 변환해야 함.
	//
	// RootMotionMode 가 누적 소스를 결정 (현재 dormant — 후속 step 에서 분기에 반영).
	void AccumulateRootMotion(const FTransform& Delta);
	FTransform ConsumeRootMotion();

	ERootMotionMode GetRootMotionMode() const { return RootMotionMode; }
	void            SetRootMotionMode(ERootMotionMode InMode) { RootMotionMode = InMode; }

	// ── Montage (Phase 2.1+: slot 별 보유) ──
	// SlotName 은 마지막 default 인자 — 미지정 (FName::None) 시 내부에서 DefaultMontageSlot
	// 으로 resolve. 기존 호출 (PlayMontage(M) 등) 은 자동으로 DefaultSlot 사용 — backward compat.
	// Phase 2.2 에서 FAnimNode_Slot 이 GetMontageInstanceForSlot 으로 trees 안에서 조회.
	static const FName DefaultMontageSlot;

	void  PlayMontage(UAnimMontage* Montage, FName StartSection = FName::None,
	                  float PlayRate = 1.0f, float BlendInTime = -1.0f,
	                  FName SlotName = FName::None);
	void  StopMontage(float BlendOutTime = -1.0f, FName SlotName = FName::None);
	void  Montage_JumpToSection(FName SectionName, FName SlotName = FName::None);
	void  Montage_SetNextSection(FName From, FName To, FName SlotName = FName::None);
	bool  IsMontagePlaying(UAnimMontage* Montage = nullptr, FName SlotName = FName::None) const;

	// Slot 별 montage instance 조회. 없으면 nullptr.
	UAnimMontageInstance* GetMontageInstanceForSlot(FName SlotName) const;

	// Legacy alias — DefaultSlot 의 instance. 새 코드는 GetMontageInstanceForSlot 권장.
	UAnimMontageInstance* GetMontageInstance() const;

	// ── AnimGraph (Phase 1.4+) ──
	// 자식이 NativeInitializeAnimation 에서 MakeNode 로 노드 트리 build 후 SetRootNode 호출.
	// InRoot 가 FAnimNode_Root 가 아니면 자동으로 wrap (IsRoot() 가상함수로 판별) — RootMotion
	// 누적 등 정책 hook 의 단일 진입점 보장. null 이면 legacy 경로 (NativeUpdate/Evaluate).
	void            SetRootNode(FAnimNode_Base* InRoot);
	FAnimNode_Base* GetRootNode() const { return RootNode; }

	// 노드 빌더 헬퍼 — 노드 생성 후 OwnedNodes 에 push, raw 반환. lifetime 자동 관리.
	// 트리의 부모-자식 참조는 raw pointer 로 — OwnedNodes 가 모든 노드의 단일 소유자.
	template<typename T, typename... Args>
	T* MakeNode(Args&&... InArgs)
	{
		auto NodePtr = std::make_unique<T>(std::forward<Args>(InArgs)...);
		T* Raw = NodePtr.get();
		OwnedNodes.push_back(std::move(NodePtr));
		return Raw;
	}

protected:
	USkeletalMeshComponent*       OwningComponent = nullptr;
	TArray<FQueuedAnimNotify>     NotifyQueue;

	// 디버그용 최근 발사 notify 이력. DispatchQueuedAnimEvents 에서 push.
	// 비용: dispatch 당 push 1회 + cap 초과 시 erase front 1회 — 무시 가능.
	static constexpr size_t       RecentNotifyCapacity = 10;
	TArray<FQueuedAnimNotify>     RecentNotifies;

	// 활성 NotifyState 추적 — UpdateAnimation 시작 시 bSeenThisFrame 리셋, AddAnimNotifies 가
	// 매칭 시 true 마킹 후 Begin/Tick 호출. UpdateAnimation 끝에서 unseen 항목 NotifyEnd + 제거.
	TArray<FActiveAnimNotifyState> ActiveNotifyStates;

	// Root motion 누적 (UpdateAnimation 한 프레임 분, Consume 시 reset).
	FTransform                    PendingRootMotion;

	// Root motion 누적 정책. default = RootMotionFromEverything (기존 동작 유지).
	// 후속 step 에서 AccumulateRootMotion / Montage 누적 / Consume 분기에 체크 추가됨.
	UPROPERTY(Edit, Save, Category="Animation", DisplayName="Root Motion Mode", Enum=ERootMotionMode)
	ERootMotionMode               RootMotionMode = ERootMotionMode::RootMotionFromEverything;

	// Slot 이름 → montage instance. 보통 1-2 개 (DefaultSlot, UpperBodySlot 등) 라
	// 선형 탐색 OK. FName 키 TMap 의 hash 지원이 없어 array 사용.
	struct FMontageSlotEntry
	{
		FName                 SlotName;
		UAnimMontageInstance* Instance = nullptr;
	};
	TArray<FMontageSlotEntry>     MontageSlots;

	// AnimGraph 트리의 root — null 이면 legacy 경로. 모든 노드는 OwnedNodes 가 단일 소유.
	FAnimNode_Base*                              RootNode = nullptr;
	TArray<std::unique_ptr<FAnimNode_Base>>      OwnedNodes;
};
