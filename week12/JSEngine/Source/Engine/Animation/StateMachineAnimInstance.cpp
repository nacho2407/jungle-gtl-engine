#include "StateMachineAnimInstance.h"
#include "AnimationStateMachine.h"

namespace
{
	constexpr const char* StateMachineDataKey = "StateMachineData";
}

void UStateMachineAnimInstance::Serialize(FArchive& Ar)
{
	UAnimInstance::Serialize(Ar);

	if (Ar.IsSaving())
	{
		if (StateMachine)
		{
			Ar.BeginObject(StateMachineDataKey);
			StateMachine->Serialize(Ar);
			Ar.EndObject();
		}
		return;
	}

	if (Ar.IsLoading() && Ar.HasKey(StateMachineDataKey))
	{
		Ar.BeginObject(StateMachineDataKey);
		UAnimationStateMachine* LoadedStateMachine = UObjectManager::Get().CreateObject<UAnimationStateMachine>();
		if (LoadedStateMachine)
		{
			LoadedStateMachine->Serialize(Ar);
			StateMachine = LoadedStateMachine;
		}
		Ar.EndObject();
	}
}

void UStateMachineAnimInstance::Initialize(USkeletalMeshComponent* InOwnerComponent)
{
	UAnimInstance::Initialize(InOwnerComponent);
	if (StateMachine)
	{
		StateMachine->Initialize(InOwnerComponent);
	}
}

void UStateMachineAnimInstance::SetStateMachine(UAnimationStateMachine* InStateMachine)
{
	StateMachine = InStateMachine;
}

void UStateMachineAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	if (StateMachine)
	{
		StateMachine->Update(DeltaTime);
	}
}

bool UStateMachineAnimInstance::EvaluatePose(FPoseContext& OutPoseContext)
{
	return StateMachine ? StateMachine->EvaluatePose(OutPoseContext) : false;
}
