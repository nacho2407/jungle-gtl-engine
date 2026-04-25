#include "LightComponent.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Render/Resource/ShadowAtlasManager.h"

DEFINE_CLASS(ULightComponent, ULightComponentBase)
REGISTER_FACTORY(ULightComponent)

FMatrix ULightComponent::GetLightViewProj(const FMatrix& CamView, const FMatrix& CamProj) const
{
	FMatrix OutMatrix = FMatrix::Identity;

	switch (eShadowMapType)
	{
	case EShadowMap::BASIC:
		OutMatrix = ComputeBasicShadowMatrix(CamView, CamProj);
		break;
	case EShadowMap::PSM:
		OutMatrix = ComputePerspectiveShadowMatrix(CamView, CamProj);
		break;
	case EShadowMap::CSM:
		break;
	default:
		break;
	}
	return FMatrix();
}

void ULightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponentBase::GetEditableProperties(OutProps);

	static const char* ShadowMapTypeNames[] = { "Basic", "PSM", "CSM" };
	OutProps.push_back({ "ShadowMapType", EPropertyType::Enum, &eShadowMapType, 0.f, 0.f, 0.f, ShadowMapTypeNames, 3 });

	const FShadowAtlas& Atlas = FShadowAtlasManager::Get().ShadowMapAtlas;
	float InvX = 1.f / static_cast<float>(Atlas.TileCountX);
	float InvY = 1.f / static_cast<float>(Atlas.TileCountY);

	static FSRVDisplayInfo ShadowMapDisplay;
	ShadowMapDisplay = { 256.f, 256.f, 0.f, 0.f, InvX, InvY };

	OutProps.push_back({ "ShadowMap", EPropertyType::SRV, Atlas.ShadowSRV.Get(), 0.f, 0.f, 0.f, nullptr, 0, &ShadowMapDisplay });
}

FMatrix ULightComponent::ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj) const
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

	FVector Eye = Center - LightDirPost * 2.0f;
	FVector RefA = FVector(0, 1, 0);
	FVector RefB = FVector(0, 0, 1);
	FVector Ref = (std::abs(FVector::DotProduct(LightDirPost, RefA)) < 0.9f) ? RefA : RefB;
	FVector Right = FVector::CrossProduct(Ref, LightDirPost).GetSafeNormal();
	FVector Up = FVector::CrossProduct(LightDirPost, Right).GetSafeNormal();
	FMatrix LightNDCView = FMatrix::MakeViewLookAtLH(Eye, Center, Up);

	FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const FVector& C : PostCorners)
	{
		FVector4 V = FVector4(C, 1.0f) * LightNDCView;
		FVector P(V.X, V.Y, V.Z);

		Min = FVector::Min(Min, P);
		Max = FVector::Max(Max, P);
	}

	FMatrix LightNDCProj = FMatrix::MakeOrthographicOffCenterLH(Min.Y, Max.Y, Min.Z, Max.Z, Min.X, Max.X);

	FMatrix Result = LightNDCView * LightNDCProj;

	return Result;
}

FMatrix ULightComponent::ComputeBasicShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj) const
{

	return FMatrix();
}
