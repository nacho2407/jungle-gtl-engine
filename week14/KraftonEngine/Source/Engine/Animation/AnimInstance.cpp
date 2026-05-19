#include "AnimInstance.h"
#include "AnimMontage.h"
#include "AnimMontageInstance.h"
#include "AnimNotify.h"
#include "AnimSequenceBase.h"
#include "AnimationRuntime.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "GameFramework/Pawn.h"

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	// Stale guard: 이전 frame 의 PendingRootMotion 이 남아있으면 누구도 consume 안 한 것 — drop.
	// ACharacter 외 actor 에 root motion 켠 anim 을 붙이면 CMC 가 없어 영원히 누적될 위험
	// (AccumulateRootMotion 이 매트릭스 곱 → 큰 transform → NaN). 매 frame reset 으로 차단.
	// ACharacter 케이스에선 CMC::TickComponent (TG_DuringPhysics) 가 직전 frame 끝에 이미
	// consume 했으므로 시점에 이미 identity — no-op.
	// PIE pause / frame drop 등 비정상 케이스도 자동 안전.
	PendingRootMotion = FTransform();

	// NativeUpdateAnimation 은 RootNode 유무와 무관하게 항상 호출 — 사용자 변수 update hook.
	// UE 본가 동일: AnimGraph 평가는 별개 단계. 자식이 graph build 하더라도 graph 평가에
	// 입력으로 들어갈 변수 (예: Speed, Direction) 를 매 frame 갱신할 곳이 NativeUpdate.
	// Legacy 자식 (RootNode 없음) 의 NativeUpdate 가 직접 FSM->Tick 호출 — 그쪽도 그대로 동작.
	NativeUpdateAnimation(DeltaSeconds);

	// AnimGraph 트리 평가 — set 되어 있으면 root 부터 자식 Update 재귀 호출. 시간 진행 /
	// transition / notify 적재 / 자식 노드의 LastRM 계산까지 노드들이 책임. 단 누적 (Accumulate)
	// 은 노드가 직접 안 함 — 트리의 root 가 합성한 LastRM 을 외부에서 한 번만 push.
	if (RootNode)
	{
		FAnimationUpdateContext Ctx;
		Ctx.AnimInstance     = this;
		Ctx.DeltaSeconds     = DeltaSeconds;
		Ctx.FinalBlendWeight = 1.0f;
		RootNode->Update(Ctx);

		// 트리 평가 후 root 의 합성 LastRM 을 mode 체크 후 누적. 이중 누적 방지 위해 단일 진입점.
		// RootMotionFromMontagesOnly 면 base graph 측 RM 무시 (Montage 만 적용되도록).
		if (RootMotionMode != ERootMotionMode::RootMotionFromMontagesOnly)
		{
			AccumulateRootMotion(RootNode->GetLastRootMotionDelta());
		}
	}

	// Montage 도 Tick — section 진행, blend alpha, notify push.
	// (Phase 2.2 에서 Slot 노드로 옮겨 RootNode 트리 안에서 처리 예정 — 현 단계는 양쪽 경로 공통.)
	if (MontageInstance && MontageInstance->IsActive())
	{
		MontageInstance->Tick(DeltaSeconds, this);
	}

	DispatchQueuedAnimEvents();
}

void UAnimInstance::EvaluatePose(FPoseContext& Output)
{
	// 1) base pose 생성 — RootNode 트리 또는 legacy 가상 호출.
	if (RootNode)
	{
		RootNode->Evaluate(Output);
	}
	else
	{
		EvaluateAnimation(Output);
	}

	// 2) Montage 활성이면 montage pose 평가 후 base 와 BlendWeight 로 lerp.
	//    (Phase 2.2 에서 Slot 노드로 옮김 — RootNode 경로에선 트리 내부에서 처리되고
	//    여기 special-case 코드는 legacy path 전용으로 축소될 예정.)
	if (MontageInstance && MontageInstance->IsActive())
	{
		const float Weight = MontageInstance->GetBlendWeight();
		if (Weight > 0.0f)
		{
			FPoseContext MontagePose;
			MontagePose.SkeletalMesh = Output.SkeletalMesh;
			MontagePose.ResetToRefPose();
			MontageInstance->EvaluateMontagePose(MontagePose);

			if (Weight >= 1.0f)
			{
				// 완전 montage — base 무시.
				Output = MontagePose;
			}
			else
			{
				// base × montage blend.
				FPoseContext Blended;
				Blended.SkeletalMesh = Output.SkeletalMesh;
				Blended.ResetToRefPose();
				FAnimationRuntime::BlendTwoPosesTogether(Output, MontagePose, Weight, Blended);
				Output = Blended;
			}
		}
	}
}

void UAnimInstance::SetRootNode(FAnimNode_Base* InRoot)
{
	RootNode = InRoot;
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
	USkeletalMeshComponent* OwnerComponent = GetOwningComponent();
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

	for (const FAnimNotifyEvent& Notify : Notifies)
	{
		if (InRange(Notify.TriggerTime))
		{
			NotifyQueue.push_back({ Notify, Sequence });
		}
	}
}

void UAnimInstance::PlayMontage(UAnimMontage* Montage, FName StartSection, float PlayRate, float BlendInTime)
{
	if (!Montage) return;
	if (!MontageInstance)
	{
		MontageInstance = UObjectManager::Get().CreateObject<UAnimMontageInstance>(this);
	}
	MontageInstance->Play(Montage, StartSection, PlayRate, BlendInTime);
}

void UAnimInstance::StopMontage(float BlendOutTime)
{
	if (MontageInstance) MontageInstance->Stop(BlendOutTime);
}

void UAnimInstance::Montage_JumpToSection(FName SectionName)
{
	if (MontageInstance) MontageInstance->JumpToSection(SectionName);
}

void UAnimInstance::Montage_SetNextSection(FName From, FName To)
{
	if (MontageInstance) MontageInstance->SetNextSection(From, To);
}

bool UAnimInstance::IsMontagePlaying(UAnimMontage* Montage) const
{
	if (!MontageInstance || !MontageInstance->IsActive()) return false;
	if (!Montage) return true;
	return MontageInstance->GetCurrentMontage() == Montage;
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
