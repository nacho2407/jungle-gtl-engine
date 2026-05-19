#pragma once

#include "AnimNode_Base.h"
#include "Animation/PoseContext.h"

// 단순 ref pose 출력 노드. LayeredBlend 의 BlendPose 의 InputPose 로 활용 —
// montage slot 이 비어있을 때 ref pose 가 base 와 lerp 되지만, Slot 의 effective weight 가
// 0 이라 base 100% 가 됨. 즉 montage 없을 땐 안 보임.
//
// 트리 구성 예 (yui_character 의 UpperBody 데모):
//   Root = LayeredBlend(
//     BasePose  = DefaultSlot ← Locomotion TopSM
//     BlendPose = UpperBodySlot ← RefPose
//     Mask = Spine 본 트리)
class FAnimNode_RefPose : public FAnimNode_Base
{
public:
	void Evaluate(FPoseContext& Output) override
	{
		Output.ResetToRefPose();
	}

	const char* GetDebugName() const override { return "RefPose"; }
};
