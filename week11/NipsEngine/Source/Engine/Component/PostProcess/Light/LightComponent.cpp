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

void ULightComponent::PostDuplicate(UObject* Original)
{
	ULightComponentBase::PostDuplicate(Original);
	ULightComponent* Orig = Cast<ULightComponent>(Original);

	eShadowMapType = Orig->eShadowMapType;
}

void ULightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponentBase::GetEditableProperties(OutProps);

	static const char* ShadowMapTypeNames[] = { "CSM", "PSM" };
	OutProps.push_back({ "ShadowMapType", EPropertyType::Enum, &eShadowMapType, 0.f, 0.f, 0.f, ShadowMapTypeNames, 2 });

	PrintShadowMapDebugInfo(OutProps);
	OutProps.push_back({ "Shadow Resolution Scale", EPropertyType::Int, &ShadowResolutionScale});

	// ConstantBias = DepthBias * (1 / 2 ^ 24 or 16) + SlopeScaledBias * (MaxDepthSlope)
	// MaxDepthSlope는 이제 셰이더에서 직접 구할 예정. 레스터라이즈로 할 경우 PSM 처리가 안됨
	OutProps.push_back({ "Constant Bias ( DepthBias ^ (1 / TextureBit))", EPropertyType::Float, &ConstantBias, 0.000f, 0.01f, 0.001f });
	OutProps.push_back({ "Slope-Scaled Bias", EPropertyType::Float, &SlopeScaledBias, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Shadow Sharpen", EPropertyType::Float, &ShadowSharpen });
}

void ULightComponent::Serialize(FArchive& Ar)
{
	ULightComponentBase::Serialize(Ar);
	
	uint32 ShadowMapTypeValue = static_cast<uint32>(eShadowMapType);
	Ar << "ShadowMapType" << ShadowMapTypeValue;
	Ar << "ShadowResolutionScale" << ShadowResolutionScale;
	Ar << "ConstantBias" << ConstantBias;
	Ar << "SlopeScaledBias" << SlopeScaledBias;
	Ar << "ShadowSharpen" << ShadowSharpen;

	if (Ar.IsLoading())
	{
		eShadowMapType = static_cast<EShadowMap>(ShadowMapTypeValue);
	}
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

void ULightComponent::PrintShadowMapDebugInfo(TArray<FPropertyDescriptor>& OutProps) const
{
    FShadowAtlasManager& AtlasManager = FShadowAtlasManager::Get();

    const FVector4& SO = bHasDebugShadowAtlasTile
                             ? DebugShadowAtlasScaleOffset
                             : FVector4(1, 1, 0, 0);


    static FSRVDisplayInfo ShadowMapDisplay;
    ShadowMapDisplay = {
        256.f,
        256.f,
        SO.Z,
        SO.W,
        SO.Z + SO.X,
        SO.W + SO.Y
    };

    OutProps.push_back({ "ShadowMap", EPropertyType::SRV, AtlasManager.GetSRV(), 0.f, 0.f, 0.f, nullptr, 0, &ShadowMapDisplay });
}
