#include "Animation/AnimNotify.h"

#include "Core/Logging/Log.h"
#include "Runtime/Engine.h"

void UAnimNotify_LogEvent::Notify(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event)
{
	UE_LOG("[AnimNotify_LogEvent] Notify Name=%s Class=%s Time=%.3f Mesh=%p",
		Event.NotifyName.ToString().c_str(),
		Event.NotifyClassName.c_str(),
		Event.TriggerTime,
		MeshComponent);
}

void UAnimNotify_LogEvent::NotifyBegin(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event)
{
	UE_LOG("[AnimNotify_LogEvent] Begin Name=%s Class=%s Start=%.3f Duration=%.3f Mesh=%p",
		Event.NotifyName.ToString().c_str(),
		Event.NotifyClassName.c_str(),
		Event.TriggerTime,
		Event.Duration,
		MeshComponent);
}

void UAnimNotify_LogEvent::NotifyTick(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event, float DeltaTime)
{
	UE_LOG("[AnimNotify_LogEvent] Tick Name=%s Class=%s Delta=%.3f Mesh=%p",
		Event.NotifyName.ToString().c_str(),
		Event.NotifyClassName.c_str(),
		DeltaTime,
		MeshComponent);
}

void UAnimNotify_LogEvent::NotifyEnd(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event)
{
	UE_LOG("[AnimNotify_LogEvent] End Name=%s Class=%s End=%.3f Mesh=%p",
		Event.NotifyName.ToString().c_str(),
		Event.NotifyClassName.c_str(),
		Event.GetEndTime(),
		MeshComponent);
}

void UAnimNotify_FootstepSound::Notify(USkeletalMeshComponent* MeshComponent, const FAnimNotifyStateEvent& Event)
{
	if (!GEngine)
	{
		return;
	}

	constexpr const char* FootstepSoundPath = "Asset/Sound/FootStep.mp3";
	const FAudioHandle Handle = GEngine->GetAudioSystem().PlaySFX(FootstepSoundPath, 1.0f);
}
