#include "AnimSequence.h"

DEFINE_CLASS(UAnimSequenceBase, UObject)
DEFINE_CLASS(UAnimSequence, UAnimSequenceBase)

bool UAnimSequence::GetAnimationPose(float Time, FPoseContext& OutPose) const override
{
	for (int32 i = 0; i < OutPose.LocalPose.size(); ++i)
	{
		OutPose.LocalPose[i] = FMatrix::Identity;
	}
	return true;
}