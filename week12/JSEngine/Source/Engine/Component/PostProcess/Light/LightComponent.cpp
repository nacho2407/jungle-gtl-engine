#include "LightComponent.h"
#include <cmath>


namespace
{
	void BuildFrustumSplitCorners(
		const FMatrix& CamView,
		const FMatrix& CamProj,
		float SplitNearRatio,
		float SplitFarRatio,
		FVector OutCorners[8])
	{
		const FMatrix InvViewProj = (CamView * CamProj).GetInverse();

		const FVector NdcNear[4] =
		{
			FVector(-1, -1, 0),
			FVector(1, -1, 0),
			FVector(-1,  1, 0),
			FVector(1,  1, 0),
		};

		const FVector NdcFar[4] =
		{
			FVector(-1, -1, 1),
			FVector(1, -1, 1),
			FVector(-1,  1, 1),
			FVector(1,  1, 1),
		};

		for (int i = 0; i < 4; ++i)
		{
			const FVector Near = InvViewProj.TransformPosition(NdcNear[i]);
			const FVector Far = InvViewProj.TransformPosition(NdcFar[i]);

			OutCorners[i] = FVector::Lerp(Near, Far, SplitNearRatio);
			OutCorners[i + 4] = FVector::Lerp(Near, Far, SplitFarRatio);
		}
	}

	void BuildLightBasis(const FVector& LightDir, FVector& OutRight, FVector& OutUp)
	{
		FVector Ref = FVector::UpVector;
		if (std::abs(FVector::DotProduct(LightDir, FVector::UpVector)) >= 0.9f)
		{
			Ref = FVector::RightVector;
		}

		OutRight = FVector::CrossProduct(Ref, LightDir).GetSafeNormal();
		OutUp = FVector::CrossProduct(LightDir, OutRight).GetSafeNormal();
	}

	FMatrix BuildLightView(const FVector& Center, float Radius, const FVector& LightDir, const FVector& Up)
	{
		const float ViewBackoff = Radius + 10.0f;
		const FVector Eye = Center - LightDir * ViewBackoff;

		return FMatrix::MakeViewLookAtLH(Eye, Center, Up);
	}

	FVector GetCornersCenter(const FVector Corners[8])
	{
		FVector Center = FVector::ZeroVector;
		for (int i = 0; i < 8; ++i)
		{
			Center += Corners[i];
		}
		return Center / 8.0f;
	}

	float GetCornersRadius(const FVector Corners[8], const FVector& Center)
	{
		float Radius = 1.0f;
		for (int i = 0; i < 8; ++i)
		{
			Radius = std::max(Radius, (Corners[i] - Center).Size());
		}
		return Radius;
	}

	float SnapToTexel(float Value, float TexelSize)
	{
		if (TexelSize <= 0.0f)
		{
			return Value;
		}

		return std::floor(Value / TexelSize + 0.5f) * TexelSize;
	}

	FVector SnapCenterToShadowTexel(
		const FVector& Center,
		const FVector& LightDir,
		const FVector& Right,
		const FVector& Up,
		float TexelSize)
	{
		const float ForwardDistance = FVector::DotProduct(Center, LightDir);
		const float RightDistance = FVector::DotProduct(Center, Right);
		const float UpDistance = FVector::DotProduct(Center, Up);

		return LightDir * ForwardDistance +
			Right * SnapToTexel(RightDistance, TexelSize) +
			Up * SnapToTexel(UpDistance, TexelSize);
	}
}

FMatrix ULightComponent::GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	switch (ShadowMapType)
	{
	case EShadowMap::CSM:
		return ComputeCascadeShadowMatrix(CamView, CamProj, 0.0f, 0.001f);
	case EShadowMap::PSM:
		return ComputePerspectiveShadowMatrix(CamView, CamProj, VisibleObjectsBounds);
	default:
		return FMatrix::Identity;
	}
}

FMatrix ULightComponent::GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj,
	float SplitNearT, float SplitFarT, const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	switch (ShadowMapType)
	{
	case EShadowMap::CSM:
		return ComputeCascadeShadowMatrix(CamView, CamProj, SplitNearT, SplitFarT);
	case EShadowMap::PSM:
		return ComputePerspectiveShadowMatrix(CamView, CamProj, VisibleObjectsBounds);
	default:
		return FMatrix::Identity;
	}
}

void ULightComponent::PostDuplicate(UObject* Original)
{
	ULightComponentBase::PostDuplicate(Original);
	ULightComponent* Orig = Cast<ULightComponent>(Original);

	ShadowMapType = Orig->ShadowMapType;
}

FMatrix ULightComponent::ComputeCascadeShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	float SplitNearT, float SplitFarT) const
{
	constexpr float XYPad = 2.0f;
	constexpr float DepthPad = 10.0f;

	FVector SplitCorners[8];
	BuildFrustumSplitCorners(CamView, CamProj, SplitNearT, SplitFarT, SplitCorners);

	const FVector LightDir = GetForwardVector().GetSafeNormal();
	const FVector SplitCenter = GetCornersCenter(SplitCorners);
	const float CascadeRadius = GetCornersRadius(SplitCorners, SplitCenter);
	const float HalfExtent = CascadeRadius + XYPad;
	const float Resolution = std::max(1.0f, static_cast<float>(ShadowResolutionScale));
	const float TexelSize = (HalfExtent * 2.0f) / Resolution;

	FVector LightRight;
	FVector LightUp;
	BuildLightBasis(LightDir, LightRight, LightUp);

	const FVector SnappedCenter = SnapCenterToShadowTexel(SplitCenter, LightDir, LightRight, LightUp, TexelSize);
	const FMatrix LightView = BuildLightView(SnappedCenter, CascadeRadius, LightDir, LightUp);

	FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int i = 0; i < 8; ++i)
	{
		const FVector4 tmp = FVector4(SplitCorners[i], 1.0f) * LightView;
		const FVector LightSapceVertex(tmp.X, tmp.Y, tmp.Z);

		Min = FVector::Min(Min, LightSapceVertex);
		Max = FVector::Max(Max, LightSapceVertex);
	}

	Min.X -= DepthPad;
	Max.X += DepthPad;

	Min.Y = -HalfExtent;
	Max.Y = HalfExtent;
	Min.Z = -HalfExtent;
	Max.Z = HalfExtent;

	const FMatrix LightProj = FMatrix::MakeOrthographicOffCenterLH(
		Min.Y, Max.Y,
		Min.Z, Max.Z,
		Min.X, Max.X);

	return LightView * LightProj;
}

