#pragma once

#include <algorithm>

#include "Animation/AnimTypes.h"

class FAnimationRuntime
{
public:
	static bool BlendTwoPosesTogether(const FPoseContext& PoseA, const FPoseContext& PoseB, float Alpha, FPoseContext& OutPose)
	{
		const int32 BoneCount = static_cast<int32>(PoseA.LocalPose.size());
		
		if (BoneCount == 0 || PoseB.LocalPose.size() != PoseA.LocalPose.size())
		{
			return false;
		}

		Alpha = std::clamp(Alpha, 0.0f, 1.0f);

		OutPose.LocalPose.resize(BoneCount);

		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			FVector TranslationA;
			FVector TranslationB;
			FVector ScaleA;
			FVector ScaleB;
			FMatrix RotationMatrixA;
			FMatrix RotationMatrixB;

			// Bone의 Parent기준 Transform, rotaiton, scale로 분해
			const bool bDecomposedA = PoseA.LocalPose[BoneIndex].Decompose(TranslationA, RotationMatrixA, ScaleA);
			const bool bDecomposedB = PoseB.LocalPose[BoneIndex].Decompose(TranslationB, RotationMatrixB, ScaleB);

			// 분해 실패
			if (!bDecomposedA || !bDecomposedB)
			{
				OutPose.LocalPose[BoneIndex] = Alpha < 0.5f ? PoseA.LocalPose[BoneIndex] : PoseB.LocalPose[BoneIndex];
				continue;
			}

			const FVector BlendedTranslation = FVector::Lerp(TranslationA, TranslationB, Alpha);
			const FVector BlendScale = FVector::Lerp(ScaleA, ScaleB, Alpha);

			// 회전은 쿼터니언 -> 구면 선형 보간(Slerp)
			const FQuat RotationA(RotationMatrixA);
			const FQuat RotationB(RotationMatrixB);
			const FQuat BlendedRotation = FQuat::Slerp(RotationA, RotationB, Alpha);

			OutPose.LocalPose[BoneIndex] = FMatrix::MakeTRS(BlendedTranslation, BlendedRotation.ToMatrix(), BlendScale);
		}

		return true;
	}
};