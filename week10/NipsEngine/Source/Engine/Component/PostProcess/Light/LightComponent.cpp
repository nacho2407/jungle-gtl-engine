#include "LightComponent.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Render/Resource/ShadowAtlasManager.h"

DEFINE_CLASS(ULightComponent, ULightComponentBase)
REGISTER_FACTORY(ULightComponent)

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

	FMatrix BuildLightViewFromCorners(const FVector SplitCorners[8], const FVector& LightDir)
	{
		FVector Center = FVector::ZeroVector;
		for (int i = 0; i < 8; ++i)
		{
			Center += SplitCorners[i];
		}
		Center /= 8.0f;

		FVector Ref = FVector::UpVector;
		if (std::abs(FVector::DotProduct(LightDir, FVector::UpVector)) >= 0.9f)
		{
			Ref = FVector::RightVector;
		}
		
		FVector Right = FVector::CrossProduct(Ref, LightDir).GetSafeNormal();
		FVector Up = FVector::CrossProduct(LightDir, Right).GetSafeNormal();

		float Radius = 1.0f;
		for (int i = 0; i < 8; ++i)
		{
			const float Dist = (SplitCorners[i] - Center).Size();
			Radius = std::max(Radius, Dist);
		}

		const float ViewBackoff = Radius + 10.0f;
		const FVector Eye = Center - LightDir * ViewBackoff;

		return FMatrix::MakeViewLookAtLH(Eye, Center, Up);
	}
}

FMatrix ULightComponent::GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	switch (eShadowMapType)
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
	switch (eShadowMapType)
	{
	case EShadowMap::CSM:
		return ComputeCascadeShadowMatrix(CamView, CamProj, SplitNearT, SplitFarT);
	case EShadowMap::PSM:
		return ComputePerspectiveShadowMatrix(CamView, CamProj, VisibleObjectsBounds);
	default:
		return FMatrix::Identity;
	}
}

void ULightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponentBase::GetEditableProperties(OutProps);

	static const char* ShadowMapTypeNames[] = { "CSM", "PSM" };
	OutProps.push_back({ "ShadowMapType", EPropertyType::Enum, &eShadowMapType, 0.f, 0.f, 0.f, ShadowMapTypeNames, 2 });

	FShadowAtlasManager& AtlasManager = FShadowAtlasManager::Get();
	const float AtlasW = static_cast<float>(AtlasManager.GetAtlasWidth());
	const float AtlasH = static_cast<float>(AtlasManager.GetAtlasHeight());
	const float TileSize = static_cast<float>(AtlasManager.GetTileSize());

	static FSRVDisplayInfo ShadowMapDisplay;
	ShadowMapDisplay = {
		256.f,
		256.f,
		0.f,
		0.f,
		AtlasW > 0.0f ? TileSize / AtlasW : 1.0f,
		AtlasH > 0.0f ? TileSize / AtlasH : 1.0f
	};

	OutProps.push_back({ "ShadowMap", EPropertyType::SRV, AtlasManager.GetSRV(), 0.f, 0.f, 0.f, nullptr, 0, &ShadowMapDisplay });
}

FMatrix ULightComponent::ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	FVector LightDir = GetForwardVector().GetSafeNormal();
	FMatrix CamViewProj = CamView * CamProj;

	auto ToPost = [&](const FVector& P)
		{
			FVector4 Clip = FVector4(P, 1.0f) * CamViewProj;

			if (MathUtil::Abs(Clip.W) < MathUtil::Epsilon)
			{
				return FVector::ZeroVector;
			}

			return FVector(
				Clip.X / Clip.W,
				Clip.Y / Clip.W,
				Clip.Z / Clip.W
			);
		};

	TArray<FVector> RelevantPoints;
	if (VisibleObjectsBounds != nullptr)
	{
		for (const FBoundingBox& Box : *VisibleObjectsBounds)
		{
			FVector Corners[8];
			Box.GetVertices(Corners);

			for (int i = 0; i < 8; ++i)
			{
				RelevantPoints.push_back(ToPost(Corners[i]));
			}
		}
	}

	FVector PostCorners[8] =
	{
		FVector(-1, -1, 0),
		FVector(1, -1, 0),
		FVector(-1, 1, 0),
		FVector(1, 1, 0),
		FVector(-1, -1, 1),
		FVector(1, -1, 1),
		FVector(-1, 1, 1),
		FVector(1, 1, 1)
	};

	if (RelevantPoints.empty())
	{
		for (int i = 0; i < 8; ++i)
		{
			RelevantPoints.push_back(PostCorners[i]);
		}
	}

	FVector Center = FVector::ZeroVector;
	for (const FVector& C : PostCorners)
	{
		Center += C;
	}
	Center /= 8.0f;

	const float Delta = 0.1f;

	const FMatrix CamWorld = CamView.GetInverse();
	const FVector CameraWorldPos = CamWorld.GetOrigin();
	const FVector SampleWorldPos = CameraWorldPos + CamWorld.GetForwardVector() * 3.0f;

	FVector Post0 = ToPost(SampleWorldPos);
	FVector Post1 = ToPost(SampleWorldPos + LightDir * Delta);

	FVector LightDirPost = (Post1 - Post0).GetSafeNormal();
	if (LightDirPost.IsNearlyZero())
	{
		LightDirPost = FVector(1, 0, 0);
	}

	FVector Eye = Center - LightDirPost * 1.5f;
	FVector RefA = FVector(0, 1, 0);
	FVector RefB = FVector(0, 0, 1);
	FVector Ref = (std::abs(FVector::DotProduct(LightDirPost, RefA)) < 0.9f) ? RefA : RefB;
	FVector Right = FVector::CrossProduct(Ref, LightDirPost).GetSafeNormal();
	FVector Up = FVector::CrossProduct(LightDirPost, Right).GetSafeNormal();
	FMatrix LightNDCView = FMatrix::MakeViewLookAtLH(Eye, Center, Up);

	FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const FVector& C : RelevantPoints)
	{
		FVector4 V = FVector4(C, 1.0f) * LightNDCView;
		FVector P(V.X, V.Y, V.Z);

		Min = FVector::Min(Min, P);
		Max = FVector::Max(Max, P);
	}

	FMatrix LightNDCProj = FMatrix::MakeOrthographicOffCenterLH(Min.Y, Max.Y, Min.Z, Max.Z, Min.X - 1.0f, Max.X + 1.0f);

	FMatrix Result = LightNDCView * LightNDCProj;

	return Result;
}

FMatrix ULightComponent::ComputeCascadeShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	float SplitNearT, float SplitFarT) const
{
	constexpr float XYPad = 2.0f;
	constexpr float DepthPad = 10.0f;

	FVector SplitCorners[8];
	BuildFrustumSplitCorners(CamView, CamProj, SplitNearT, SplitFarT, SplitCorners);

	const FVector LightDir = GetForwardVector().GetSafeNormal();
	const FMatrix LightView = BuildLightViewFromCorners(SplitCorners, LightDir);

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
	Min.Y -= XYPad;
	Max.Y += XYPad;
	Min.Z -= XYPad;
	Max.Z += XYPad;

	const FMatrix LightProj = FMatrix::MakeOrthographicOffCenterLH(
		Min.Y, Max.Y,
		Min.Z, Max.Z,
		Min.X, Max.X);

	return LightView * LightProj;
}
