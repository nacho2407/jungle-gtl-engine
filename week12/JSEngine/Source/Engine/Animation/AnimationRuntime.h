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

			const bool bDecomposedA = DecomposeForAnimationBlend(PoseA.LocalPose[BoneIndex], TranslationA, RotationMatrixA, ScaleA);
			const bool bDecomposedB = DecomposeForAnimationBlend(PoseB.LocalPose[BoneIndex], TranslationB, RotationMatrixB, ScaleB);

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

private:
	static float GetUpper3x3Determinant(const FMatrix& Matrix)
	{
		const FVector XAxis = Matrix.GetScaledAxis(EAxis::X);
		const FVector YAxis = Matrix.GetScaledAxis(EAxis::Y);
		const FVector ZAxis = Matrix.GetScaledAxis(EAxis::Z);
		return FVector::DotProduct(FVector::CrossProduct(XAxis, YAxis), ZAxis);
	}

	static bool DecomposeForAnimationBlend(
		const FMatrix& Matrix,
		FVector& OutTranslation,
		FMatrix& OutRotation,
		FVector& OutScale)
	{
		constexpr float ScaleTolerance = 1.e-8f;

		OutTranslation = Matrix.GetOrigin();

		const FVector XAxis = Matrix.GetScaledAxis(EAxis::X);
		const FVector YAxis = Matrix.GetScaledAxis(EAxis::Y);
		const FVector ZAxis = Matrix.GetScaledAxis(EAxis::Z);

		OutScale = FVector(XAxis.Size(), YAxis.Size(), ZAxis.Size());
		if (OutScale.X <= ScaleTolerance || OutScale.Y <= ScaleTolerance || OutScale.Z <= ScaleTolerance)
		{
			OutRotation = FMatrix::Identity;
			return false;
		}

		if (GetUpper3x3Determinant(Matrix) < 0.0f)
		{
			OutScale.X *= -1.0f;
		}

		OutRotation = FMatrix::Identity;
		OutRotation.SetAxes(
			XAxis / OutScale.X,
			YAxis / OutScale.Y,
			ZAxis / OutScale.Z,
			FVector::ZeroVector);

		return true;
	}
};
