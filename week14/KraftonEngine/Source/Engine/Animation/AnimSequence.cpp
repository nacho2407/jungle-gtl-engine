#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "PoseContext.h"
#include "AnimExtractContext.h"

DEFINE_CLASS(UAnimSequence, UAnimSequenceBase)

void UAnimSequence::SetDataModel(UAnimDataModel* InModel)
{
	DataModel = InModel;
	if (DataModel)
	{
		PlayLength = DataModel->PlayLength;
		FrameRate  = DataModel->FrameRate;
		Notifies   = DataModel->Notifies;
		BoneTracks = DataModel->BoneAnimationTracks;
	}
}

void UAnimSequence::GetBonePose(FPoseContext& Output, const FAnimExtractContext& Ctx) const
{
	// A 담당: BoneTracks 의 키프레임을 Ctx.CurrentTime 으로 샘플링해 Output.Pose 채우기.
	// 현재는 stub — Output 은 호출자(B)가 ResetToRefPose() 후 넘긴 상태 그대로 둔다.
	(void)Output;
	(void)Ctx;
}
