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
	// 자식이 시간 진행 + AddAnimNotifies 로 큐에 적재 → 베이스가 일괄 dispatch.
	NativeUpdateAnimation(DeltaSeconds);

	// Montage 도 Tick — section 진행, blend alpha, notify push.
	if (MontageInstance && MontageInstance->IsActive())
	{
		MontageInstance->Tick(DeltaSeconds, this);
	}

	DispatchQueuedAnimEvents();
}

void UAnimInstance::EvaluatePose(FPoseContext& Output)
{
	// 1) 자식이 base pose (FSM / SingleNode) 생성.
	EvaluateAnimation(Output);

	// 2) Montage 활성이면 montage pose 평가 후 base 와 BlendWeight 로 lerp.
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
