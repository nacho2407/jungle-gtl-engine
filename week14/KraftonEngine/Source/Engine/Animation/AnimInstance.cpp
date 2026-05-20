#include "AnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Animation/Montage/AnimMontageInstance.h"
#include "Animation/Notify/AnimNotify.h"
#include "Animation/Notify/AnimNotifyState.h"
#include "Animation/Sequence/AnimSequenceBase.h"
#include "Animation/AnimationRuntime.h"
#include "Animation/Nodes/AnimNode_Root.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "GameFramework/Pawn/Pawn.h"

#include <algorithm>

// Static 멤버 정의 — slot 이름 미지정 시 fallback. 가독성 위해 cpp 상단에 둠.
const FName UAnimInstance::DefaultMontageSlot = FName("DefaultSlot");

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	// Stale guard: 이전 frame 의 PendingRootMotion 이 남아있으면 누구도 consume 안 한 것 — drop.
	// ACharacter 외 actor 에 root motion 켠 anim 을 붙이면 CMC 가 없어 영원히 누적될 위험
	// (AccumulateRootMotion 이 매트릭스 곱 → 큰 transform → NaN). 매 frame reset 으로 차단.
	// ACharacter 케이스에선 CMC::TickComponent (TG_DuringPhysics) 가 직전 frame 끝에 이미
	// consume 했으므로 시점에 이미 identity — no-op.
	// PIE pause / frame drop 등 비정상 케이스도 자동 안전.
	PendingRootMotion = FTransform();

	// 활성 NotifyState seen 플래그 reset — AddAnimNotifies 가 매칭 시 true 표시,
	// frame 끝에 false 인 항목은 NotifyEnd 후 제거 (시퀀스 전환 / weight drop / 자연 종료 통합 경로).
	for (FActiveAnimNotifyState& A : ActiveNotifyStates)
	{
		A.bSeenThisFrame = false;
	}

	// NativeUpdateAnimation 은 RootNode 유무와 무관하게 항상 호출 — 사용자 변수 update hook.
	// UE 본가 동일: AnimGraph 평가는 별개 단계. 자식이 graph build 하더라도 graph 평가에
	// 입력으로 들어갈 변수 (예: Speed, Direction) 를 매 frame 갱신할 곳이 NativeUpdate.
	// Legacy 자식 (RootNode 없음) 의 NativeUpdate 가 직접 FSM->Tick 호출 — 그쪽도 그대로 동작.
	NativeUpdateAnimation(DeltaSeconds);

	// AnimGraph 트리 평가 — set 되어 있으면 root 부터 자식 Update 재귀 호출. 시간 진행 /
	// transition / notify 적재 / 자식 노드의 LastRM 계산까지 노드들이 책임.
	// RootMotion 누적은 FAnimNode_Root 안에서 mode 분기와 함께 처리 — AnimInstance 는 더 이상
	// 직접 분기하지 않는다 (정책 단일 진입점).
	if (RootNode)
	{
		FAnimationUpdateContext Ctx;
		Ctx.AnimInstance     = this;
		Ctx.DeltaSeconds     = DeltaSeconds;
		Ctx.FinalBlendWeight = 1.0f;
		RootNode->Update(Ctx);
	}

	// Montage Tick (legacy fallback) — RootNode 없을 때만 일괄 tick.
	// RootNode 경로에선 FAnimNode_Slot::Update 가 자기 slot 의 montage tick 책임 (Step 3.1).
	if (!RootNode)
	{
		for (FMontageSlotEntry& Entry : MontageSlots)
		{
			if (Entry.Instance && Entry.Instance->IsActive())
			{
				Entry.Instance->Tick(DeltaSeconds, this);
			}
		}
	}

	DispatchQueuedAnimEvents();

	// frame 끝 — unseen 활성 NotifyState 들 NotifyEnd 후 erase. 시퀀스 전환 / weight 0 drop /
	// 자연 종료 모두 같은 경로로 끝남.
	for (int32 i = static_cast<int32>(ActiveNotifyStates.size()) - 1; i >= 0; --i)
	{
		FActiveAnimNotifyState& A = ActiveNotifyStates[i];
		if (!A.bSeenThisFrame)
		{
			if (A.State)
			{
				A.State->NotifyEnd(OwningComponent, const_cast<UAnimSequenceBase*>(A.Sequence));
			}
			ActiveNotifyStates.erase(ActiveNotifyStates.begin() + i);
		}
	}
}

void UAnimInstance::EvaluatePose(FPoseContext& Output)
{
	if (RootNode)
	{
		// RootNode 경로 — 트리 안의 FAnimNode_Slot 이 montage 처리. EvaluatePose 가
		// special-case 합성을 또 하면 이중 적용 위험. Slot 노드에 위임.
		RootNode->Evaluate(Output);
		PostEvaluatePose(Output);
		return;
	}

	// Legacy 경로 — RootNode 없으면 EvaluateAnimation 가상 호출 후 DefaultSlot 의 montage
	// 만 special-case 합성. 새 코드는 RootNode 경로 사용해 이 path 안 탐.
	EvaluateAnimation(Output);

	if (UAnimMontageInstance* DefaultMI = GetMontageInstanceForSlot(DefaultMontageSlot))
	{
		if (DefaultMI->IsActive())
		{
			const float Weight = DefaultMI->GetBlendWeight();
			if (Weight > 0.0f)
			{
				FPoseContext MontagePose;
				MontagePose.SkeletalMesh = Output.SkeletalMesh;
				MontagePose.ResetToRefPose();
				DefaultMI->EvaluateMontagePose(MontagePose);

				if (Weight >= 1.0f)
				{
					Output = MontagePose;
				}
				else
				{
					FPoseContext Blended;
					Blended.SkeletalMesh = Output.SkeletalMesh;
					Blended.ResetToRefPose();
					FAnimationRuntime::BlendTwoPosesTogether(Output, MontagePose, Weight, Blended);
					Output = Blended;
				}
			}
		}
	}

	PostEvaluatePose(Output);
}

void UAnimInstance::SetRootNode(FAnimNode_Base* InRoot)
{
	// InRoot 가 이미 FAnimNode_Root 면 그대로, 아니면 자동으로 Root 노드로 감싼다.
	// 호출자 (lua / C++) 가 임의 노드를 root 로 박아도 정책 (RootMotion 누적 등) 의 단일
	// 진입점을 보장. Auto-wrap 으로 만든 Root 도 OwnedNodes 가 lifetime 관리.
	// IsRoot() 가상함수로 판별 — RTTI (dynamic_cast) 의존 없음.
	if (InRoot && !InRoot->IsRoot())
	{
		FAnimNode_Root* Wrapper = MakeNode<FAnimNode_Root>();
		Wrapper->ChildPose = InRoot;
		RootNode = Wrapper;
	}
	else
	{
		RootNode = InRoot;
	}

	if (RootNode)
	{
		FAnimationInitializeContext InitCtx;
		InitCtx.AnimInstance = this;
		InitCtx.SkeletalMesh = GetSkeletalMesh();
		RootNode->Initialize(InitCtx);
	}
}

USkeletalMesh* UAnimInstance::GetSkeletalMesh() const
{
	return OwningComponent ? OwningComponent->GetSkeletalMesh() : nullptr;
}

APawn* UAnimInstance::TryGetPawnOwner() const
{
	// OwningComponent 가 set 보장 안 되는 경로 (생성 직후 등) 에서 호출 시 NPE 방지.
	USkeletalMeshComponent* OwnerComponent = GetOwningComponent();
	if (!OwnerComponent) return nullptr;
	if (AActor* OwnerActor = OwnerComponent->GetOwner())
	{
		return Cast<APawn>(OwnerActor);
	}
	return nullptr;
}

void UAnimInstance::AddAnimNotifies(float PreviousTime, float CurrentTime, const UAnimSequenceBase* Sequence)
{
	if (!Sequence) return;

	const TArray<FAnimNotifyEvent>& Notifies = Sequence->GetNotifies();
	const float Length = Sequence->GetPlayLength();
	const bool  bWrapped = (CurrentTime < PreviousTime); // 루프로 시간 wrap

	// Instant trigger 가 [Prev, Cur) 구간 안에 들어왔는지.
	auto InRange = [&](float Trigger) -> bool
	{
		if (!bWrapped)
		{
			return Trigger >= PreviousTime && Trigger < CurrentTime;
		}
		// wrap: [Prev, Length) ∪ [0, Current)
		return (Trigger >= PreviousTime && Trigger < Length) ||
		       (Trigger >= 0.0f         && Trigger < CurrentTime);
	};

	// [a, b) 와 [c, d) 의 교집합 폭. 음수면 0.
	auto OverlapWidth = [](float a, float b, float c, float d) -> float
	{
		const float Lo = std::max(a, c);
		const float Hi = std::min(b, d);
		return std::max(0.0f, Hi - Lo);
	};

	// 활성 set 안에서 (state, sequence, name) 매칭 entry 조회.
	auto FindActive = [&](UAnimNotifyState* S, const UAnimSequenceBase* Seq, FName Name) -> FActiveAnimNotifyState*
	{
		for (FActiveAnimNotifyState& A : ActiveNotifyStates)
		{
			if (A.State == S && A.Sequence == Seq && A.NotifyName == Name) return &A;
		}
		return nullptr;
	};

	for (const FAnimNotifyEvent& Notify : Notifies)
	{
		// ── State notify (Duration > 0 + NotifyState 객체) ──
		if (Notify.NotifyState && Notify.Duration > 0.0f)
		{
			const float EvStart = Notify.TriggerTime;
			const float EvEnd   = Notify.TriggerTime + Notify.Duration;

			float Overlap = 0.0f;
			if (!bWrapped)
			{
				Overlap = OverlapWidth(PreviousTime, CurrentTime, EvStart, EvEnd);
			}
			else
			{
				// wrap 경계는 두 sub-range 합. (이벤트가 wrap 경계를 넘어가는 케이스는 v1 에선
				// 클리핑된 상태로 처리 — 보통 Duration < PlayLength.)
				Overlap += OverlapWidth(PreviousTime, Length,       EvStart, EvEnd);
				Overlap += OverlapWidth(0.0f,         CurrentTime,  EvStart, EvEnd);
			}

			if (Overlap > 0.0f)
			{
				FActiveAnimNotifyState* A = FindActive(Notify.NotifyState, Sequence, Notify.NotifyName);
				if (!A)
				{
					FActiveAnimNotifyState NewEntry;
					NewEntry.State          = Notify.NotifyState;
					NewEntry.Sequence       = Sequence;
					NewEntry.NotifyName     = Notify.NotifyName;
					NewEntry.TotalDuration  = Notify.Duration;
					NewEntry.bSeenThisFrame = true;
					ActiveNotifyStates.push_back(NewEntry);
					// push_back 후 reference 무효화 가능 — 인덱스 재조회.
					A = &ActiveNotifyStates.back();
					A->State->NotifyBegin(OwningComponent, const_cast<UAnimSequenceBase*>(Sequence), Notify.Duration);
				}
				else
				{
					A->bSeenThisFrame = true;
				}
				A->State->NotifyTick(OwningComponent, const_cast<UAnimSequenceBase*>(Sequence), Overlap);
			}
			continue;
		}

		// ── Instant notify (기존 경로) ──
		if (InRange(Notify.TriggerTime))
		{
			NotifyQueue.push_back({ Notify, Sequence });
		}
	}
}

UAnimMontageInstance* UAnimInstance::GetMontageInstanceForSlot(FName SlotName) const
{
	const FName Key = (SlotName == FName::None) ? DefaultMontageSlot : SlotName;
	for (const FMontageSlotEntry& Entry : MontageSlots)
	{
		if (Entry.SlotName == Key) return Entry.Instance;
	}
	return nullptr;
}

UAnimMontageInstance* UAnimInstance::GetMontageInstance() const
{
	return GetMontageInstanceForSlot(DefaultMontageSlot);
}

void UAnimInstance::PlayMontage(UAnimMontage* Montage, FName StartSection, float PlayRate, float BlendInTime, FName SlotName)
{
	if (!Montage) return;
	const FName Key = (SlotName == FName::None) ? DefaultMontageSlot : SlotName;

	// 기존 slot entry 찾거나 새로 만들기 — 첫 PlayMontage 호출 시 lazy 생성.
	UAnimMontageInstance* Instance = nullptr;
	for (FMontageSlotEntry& Entry : MontageSlots)
	{
		if (Entry.SlotName == Key) { Instance = Entry.Instance; break; }
	}
	if (!Instance)
	{
		Instance = UObjectManager::Get().CreateObject<UAnimMontageInstance>(this);
		MontageSlots.push_back({ Key, Instance });
	}
	Instance->Play(Montage, StartSection, PlayRate, BlendInTime);
}

void UAnimInstance::StopMontage(float BlendOutTime, FName SlotName)
{
	if (UAnimMontageInstance* MI = GetMontageInstanceForSlot(SlotName))
	{
		MI->Stop(BlendOutTime);
	}
}

void UAnimInstance::Montage_JumpToSection(FName SectionName, FName SlotName)
{
	if (UAnimMontageInstance* MI = GetMontageInstanceForSlot(SlotName))
	{
		MI->JumpToSection(SectionName);
	}
}

void UAnimInstance::Montage_SetNextSection(FName From, FName To, FName SlotName)
{
	if (UAnimMontageInstance* MI = GetMontageInstanceForSlot(SlotName))
	{
		MI->SetNextSection(From, To);
	}
}

bool UAnimInstance::IsMontagePlaying(UAnimMontage* Montage, FName SlotName) const
{
	UAnimMontageInstance* MI = GetMontageInstanceForSlot(SlotName);
	if (!MI || !MI->IsActive()) return false;
	if (!Montage) return true;
	return MI->GetCurrentMontage() == Montage;
}

void UAnimInstance::AccumulateRootMotion(const FTransform& Delta)
{
	// Mode 가 Ignore 면 누적 자체 skip — PendingRootMotion 은 identity 로 유지.
	// RootMotionFromMontagesOnly 일 때 base (SingleNode/FSM) 누적 skip 은 호출자 측 책임
	// (Step 5 에서 base 누적 호출 지점에 mode 체크가 들어간다 — 여기선 둘 다 통과).
	if (RootMotionMode == ERootMotionMode::IgnoreRootMotion) return;

	// 두 delta 합성 — row-vec 매트릭스로 정확히 누적 후 다시 분해.
	// 단순한 합산은 회전 누적 시 부정확. 매트릭스 곱이 안전.
	const FMatrix M = Delta.ToMatrix() * PendingRootMotion.ToMatrix();
	PendingRootMotion.Location = FVector(M.M[3][0], M.M[3][1], M.M[3][2]);
	// 회전만 quaternion 합성 (정밀도 유지)
	PendingRootMotion.Rotation = (Delta.Rotation * PendingRootMotion.Rotation).GetNormalized();
	// Scale 은 root motion 에서 보통 1 이라 무시.
}

FTransform UAnimInstance::ConsumeRootMotion()
{
	const FTransform Out = PendingRootMotion;
	PendingRootMotion = FTransform();   // Identity 로 reset
	return Out;
}

void UAnimInstance::DispatchQueuedAnimEvents()
{
	for (const FQueuedAnimNotify& Q : NotifyQueue)
	{
		// 1) UE 패턴 — 로직 객체가 박혀 있으면 자기 Notify() 실행. 시퀀스가 자기 로직 소유.
		if (Q.Event.Notify)
		{
			// UAnimNotify::Notify 시그니처가 비-const 라 const_cast.
			Q.Event.Notify->Notify(OwningComponent, const_cast<UAnimSequenceBase*>(Q.Sequence));
		}

		// 2) AnimInstance 자식이 NotifyName 매칭으로 추가 처리할 수 있도록 fallback 후크.
		HandleAnimNotify(Q.Event);

		// 3) 디버그 ring buffer — Editor widget 가 최근 N 개 표시.
		RecentNotifies.push_back(Q);
		if (RecentNotifies.size() > RecentNotifyCapacity)
		{
			RecentNotifies.erase(RecentNotifies.begin());
		}
	}
	NotifyQueue.clear();
}
