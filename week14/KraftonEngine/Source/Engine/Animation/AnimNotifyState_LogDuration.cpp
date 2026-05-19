#include "AnimNotifyState_LogDuration.h"

#include "Core/Log.h"

void UAnimNotifyState_LogDuration::NotifyBegin(USkeletalMeshComponent* /*MeshComp*/, UAnimSequenceBase* /*Anim*/, float TotalDuration)
{
	UE_LOG("[AnimNotifyState_LogDuration] BEGIN %s (dur=%.3fs)", Tag.c_str(), TotalDuration);
}

void UAnimNotifyState_LogDuration::NotifyTick(USkeletalMeshComponent* /*MeshComp*/, UAnimSequenceBase* /*Anim*/, float FrameDeltaTime)
{
	UE_LOG("[AnimNotifyState_LogDuration] TICK  %s (dt=%.4fs)", Tag.c_str(), FrameDeltaTime);
}

void UAnimNotifyState_LogDuration::NotifyEnd(USkeletalMeshComponent* /*MeshComp*/, UAnimSequenceBase* /*Anim*/)
{
	UE_LOG("[AnimNotifyState_LogDuration] END   %s", Tag.c_str());
}
