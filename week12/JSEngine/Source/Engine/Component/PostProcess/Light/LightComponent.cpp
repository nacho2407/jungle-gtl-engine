#include "LightComponent.h"
#include "Core/DebugDetails.h"
#include "Render/Resource/ShadowAtlasManager.h"

#include <cmath>


namespace
{
	void BuildFrustumSplitCorners(
		const FMatrix& CamView,
		const FMatrix& CamProj,
		float SplitNearRatio,
		float SplitFarRatio,
		FVector OutWorldCorners[8],
		FVector OutViewCorners[8])
	{
		// 뷰 공간의 코너 값을 유지하여 프레임마다 빛 공간의 직교 투영 범위가 변하는 현상을 방지합니다.
		const FMatrix InvProj = CamProj.GetInverse();
		const FMatrix InvView = CamView.GetInverse();

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
			const FVector NearView = InvProj.TransformPosition(NdcNear[i]);
			const FVector FarView = InvProj.TransformPosition(NdcFar[i]);

			OutViewCorners[i] = FVector::Lerp(NearView, FarView, SplitNearRatio);
			OutViewCorners[i + 4] = FVector::Lerp(NearView, FarView, SplitFarRatio);
			OutWorldCorners[i] = InvView.TransformPosition(OutViewCorners[i]);
			OutWorldCorners[i + 4] = InvView.TransformPosition(OutViewCorners[i + 4]);
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
		return ComputeCascadeShadowMatrix(CamView, CamProj, 0.0f, 0.001f, 0.0f);
	case EShadowMap::PSM:
		return ComputePerspectiveShadowMatrix(CamView, CamProj, VisibleObjectsBounds);
	default:
		return FMatrix::Identity;
	}
}

FMatrix ULightComponent::GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj, float SplitNearT, float SplitFarT, const TArray<FBoundingBox>* VisibleObjectsBounds, float ShadowMapResolution) const
{
	switch (ShadowMapType)
	{
	case EShadowMap::CSM:
		return ComputeCascadeShadowMatrix(CamView, CamProj, SplitNearT, SplitFarT, ShadowMapResolution);
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

void ULightComponent::BuildDebugDetails(FDebugDetailsBuilder& Builder)
{
	ULightComponentBase::BuildDebugDetails(Builder);

	if (bHasDebugShadowAtlasTile)
	{
		const int32 PreviewCount = std::max(DebugShadowCascadeCount, 1);
		for (int32 CascadeIndex = 0; CascadeIndex < PreviewCount && CascadeIndex < 4; ++CascadeIndex)
		{
			const FVector4 ScaleOffset = DebugShadowCascadeCount > 0
				? DebugShadowCascadeScaleOffsets[CascadeIndex]
				: DebugShadowAtlasScaleOffset;

			FDebugSRVPreviewData Preview;
			Preview.SRV = FShadowAtlasManager::Get().GetSRV();
			Preview.DisplayInfo.ImageWidth = 160.0f;
			Preview.DisplayInfo.ImageHeight = 160.0f;
			Preview.DisplayInfo.UV0X = ScaleOffset.Z;
			Preview.DisplayInfo.UV0Y = ScaleOffset.W;
			Preview.DisplayInfo.UV1X = ScaleOffset.Z + ScaleOffset.X;
			Preview.DisplayInfo.UV1Y = ScaleOffset.W + ScaleOffset.Y;

			FString Label = PreviewCount > 1
				? FString("CSM Cascade ") + std::to_string(CascadeIndex)
				: "Shadow Atlas Tile";
			Builder.AddSRVPreview(Label.c_str(), Preview);
		}
	}

	if (bHasDebugShadowCubeTile)
	{
		FDebugCubeSRVPreviewData Preview;
		Preview.DisplayInfo.ImageWidth = 72.0f;
		Preview.DisplayInfo.ImageHeight = 72.0f;
		for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			Preview.FaceSRVs[FaceIndex] =
				FShadowAtlasManager::Get().GetCubeDebugSRV(DebugShadowCubeIndex, FaceIndex);
		}
		Builder.AddCubeSRVPreview("Shadow Cube Faces", Preview);
	}
}

FMatrix ULightComponent::ComputeCascadeShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	float SplitNearT, float SplitFarT, float ShadowMapResolution) const
{
	constexpr float XYPad = 2.0f;
	constexpr float DepthPad = 10.0f;

	FVector SplitCorners[8];
	FVector ViewSplitCorners[8];
	BuildFrustumSplitCorners(CamView, CamProj, SplitNearT, SplitFarT, SplitCorners, ViewSplitCorners);

	const FVector LightDir = GetForwardVector().GetSafeNormal();
	const FVector SplitCenter = GetCornersCenter(SplitCorners);
	const FVector ViewSplitCenter = GetCornersCenter(ViewSplitCorners);

	const float CascadeRadius = GetCornersRadius(ViewSplitCorners, ViewSplitCenter);
	const float RawHalfExtent = CascadeRadius + XYPad;

	// 부동소수점 오차로 인해 그림자가 흔들리지 않도록 반올림해 값을 고정합니다.
	const float ExtentMagnitude = RawHalfExtent > 0.0f ? std::pow(2.0f, std::floor(std::log2(RawHalfExtent))) : 1.0f;
	const float ExtentQuantum = std::max(1.0f / 16.0f, ExtentMagnitude / 1024.0f);
	const float HalfExtent = std::ceil(RawHalfExtent / ExtentQuantum) * ExtentQuantum;

	const float Resolution = std::max(1.0f, ShadowMapResolution > 0.0f ? ShadowMapResolution : static_cast<float>(ShadowResolutionScale));
	const float TexelSize = (HalfExtent * 2.0f) / Resolution;

	FVector LightRight;
	FVector LightUp;
	BuildLightBasis(LightDir, LightRight, LightUp);

	const FVector ShadowCenter = IsShadowTexelSnapped() ? SnapCenterToShadowTexel(SplitCenter, LightDir, LightRight, LightUp, TexelSize) : SplitCenter;
	const FMatrix LightView = BuildLightView(ShadowCenter, HalfExtent, LightDir, LightUp);
	const FMatrix LightProj = FMatrix::MakeOrthographicLH(HalfExtent * 2.0f, HalfExtent * 2.0f, 0.0f, HalfExtent * 2.0f + DepthPad);

	return LightView * LightProj;
}

