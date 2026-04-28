#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UDirectionalLightComponent, ULightComponent)
REGISTER_FACTORY(UDirectionalLightComponent)

void UDirectionalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponent::GetEditableProperties(OutProps);
}

FMatrix UDirectionalLightComponent::ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	FVector PostCubeCenter = FVector(0.0f, 0.0f, 0.5f);
	float PostCubeRadius = FVector(1.0f, 1.0f, 0.5f).Size();

	FMatrix LightView;
	FMatrix LightProj;

	FVector WorldLightDir = -GetForwardVector().GetSafeNormal();

	FVector EyeLightDir = CamView.TransformVector(WorldLightDir);
	FVector4 PostLightDir4 = CamProj.TransformVector4(FVector4(EyeLightDir, 0.0f), CamProj);

	bool bLightBehindOfEye = PostLightDir4.W < 0.0f;
	bool bOrthogonal = MathUtil::Abs(PostLightDir4.W) <= 0.1f;

	if (bOrthogonal)
	{
		FVector PostLightDir = FVector(PostLightDir4.X, PostLightDir4.Y, PostLightDir4.Z).GetSafeNormal();
		FVector PostLightPos = PostCubeCenter + PostLightDir * 2.0f * PostCubeRadius;
		float DistToCenter = PostLightPos.Size();

		float PostNear = DistToCenter - PostCubeRadius;
		float PostFar = DistToCenter + PostCubeRadius;

		FVector UpVector = FVector(0, 1, 0);
		if (MathUtil::Abs(FVector::DotProduct((PostCubeCenter - PostLightPos).GetSafeNormal(), UpVector)) > 0.99f)
		{
			UpVector = FVector(0, 0, 1);
		}

		LightView = FMatrix::MakeViewLookAtLH(PostLightPos, PostCubeCenter, (PostLightPos + UpVector));
		LightProj = FMatrix::MakeOrthographicLH(PostCubeRadius * 2.0f, PostCubeRadius * 2.0f, PostNear, PostFar);
	}
	else
	{
		float WRecip = 1.0f / PostLightDir4.W;
		FVector PostLightPos;
		PostLightPos.X = PostLightDir4.X * WRecip;
		PostLightPos.Y = PostLightDir4.Y * WRecip;
		PostLightPos.Z = PostLightDir4.Z * WRecip;

		FVector PostLookAtVec = (PostCubeCenter - PostLightPos);
		float PostDistLookAtVec = std::max(PostLookAtVec.Size(), 0.001f);
		PostLookAtVec /= PostDistLookAtVec;

		if (bLightBehindOfEye)
		{
			FVector ToBSphereDirection = PostCubeCenter - PostLightPos;
			const float DistToBSphere = ToBSphereDirection.Size();
			ToBSphereDirection = ToBSphereDirection.GetSafeNormal();

			float PostNear = DistToBSphere - PostCubeRadius;
			float PostFov = 2.0f * atanf(PostCubeRadius / DistToBSphere);

			PostNear = std::max(0.1f, PostNear);
			float PostFar = PostNear;
			PostNear = -PostNear;

			LightProj = FMatrix::MakePerspectiveFovLH(PostFov, 1.0f, PostNear, PostFar);
		}
		else
		{
			float PostFov = 2.0f * atanf(PostCubeRadius / PostDistLookAtVec);
			float PostAspect = 1.0f;

			float PostNear = std::max(0.1f, PostDistLookAtVec - PostCubeRadius);
			float PostFar = PostDistLookAtVec + PostCubeRadius;
			LightProj = FMatrix::MakePerspectiveFovLH(PostFov, PostAspect, PostNear, PostFar);
		}

		FVector UpVector = FVector(0, 1, 0);
		if (fabsf(FVector::DotProduct(PostLookAtVec, (PostLightPos + UpVector))) > 0.99f)
		{
			UpVector = FVector(0, 0, 1);
		}

		LightView = FMatrix::MakeViewLookAtLH(PostLightPos, PostCubeCenter, (PostLightPos + UpVector));
	}

	return LightView * LightProj;
}

