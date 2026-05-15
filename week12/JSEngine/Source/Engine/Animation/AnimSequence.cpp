#include "AnimSequence.h"

#include <algorithm>

DEFINE_CLASS(UAnimSequenceBase, UObject)

void UAnimSequenceBase::AddNotify(float InTriggerTime, const FName& InNotifyName)
{
	FAnimNotifyEvent NewNotify;

	NewNotify.TriggerTime = std::clamp(InTriggerTime, 0.0f, PlayLength);
	NewNotify.NotifyName = InNotifyName;

	Notifies.push_back(NewNotify);

	std::ranges::sort(Notifies,
                      [](const FAnimNotifyEvent& A, const FAnimNotifyEvent& B) { return A.TriggerTime < B.TriggerTime; });
}

DEFINE_CLASS(UAnimSequence, UAnimSequenceBase)
DEFINE_CLASS(UDebugAnimSequence, UAnimSequenceBase)

bool UAnimSequence::GetAnimationPose(float Time, FPoseContext& OutPose) const
{
	for (int32 i = 0; i < OutPose.LocalPose.size(); ++i)
	{
		OutPose.LocalPose[i] = FMatrix::Identity;
	}
	return true;
}