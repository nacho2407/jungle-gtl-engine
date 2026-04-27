#include "SpotlightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(USpotlightComponent, UPointLightComponent)
REGISTER_FACTORY(USpotlightComponent)

void USpotlightComponent::PostDuplicate(UObject* Original)
{
    UPointLightComponent::PostDuplicate(Original);
}

void USpotlightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPointLightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Inner Cone Angle", EPropertyType::Float, &InnerConeAngle });
    OutProps.push_back({ "Outer Cone Angle", EPropertyType::Float, &OuterConeAngle });
}

void USpotlightComponent::Serialize(FArchive& Ar)
{
	UPointLightComponent::Serialize(Ar);
	Ar << "InnerConeAngle" << InnerConeAngle;
	Ar << "OuterConeAngle" << OuterConeAngle;
}

FMatrix USpotlightComponent::ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	FVector CamDir = FVector(CamView[0][0], CamView[1][0], CamView[2][0]) * -1.0f;
	FVector LightDir = GetForwardVector().GetSafeNormal();

	float Dot = MathUtil::Abs(FVector::DotProduct(CamDir, LightDir));
	if (Dot < 0.1f)
	{
		return ComputeBasicShadowMatrix(CamView, CamProj);
	}

	FMatrix CamViewProj = CamView * CamProj;

	auto ToPost = [&](const FVector& P)
		{
			FVector4 Clip = FVector4(P, 1.0f) * CamViewProj;
			if (Clip.W < MathUtil::Epsilon)
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
				FVector PostPoint = ToPost(Corners[i]);
				RelevantPoints.push_back(PostPoint);
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

	FVector PostLightPos = ToPost(GetWorldLocation());
	FVector PostLightTarget = ToPost(GetWorldLocation() + GetForwardVector() * AttenuationRadius);
	FVector LightDirPost = (PostLightTarget - PostLightPos).GetSafeNormal();

	FVector Up = (MathUtil::Abs(LightDirPost.X) < 0.9f) ? FVector(1, 0, 0) : FVector(0, 1, 0);
	FMatrix LightView = FMatrix::MakeViewLookAtLH(PostLightPos, PostLightTarget, Up);

	FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const FVector& C : RelevantPoints)
	{
		FVector4 V = FVector4(C, 1.0f) * LightView;
		FVector P(V.X, V.Y, V.Z);

		Min = FVector::Min(Min, P);
		Max = FVector::Max(Max, P);
	}

	FMatrix LightProj = FMatrix::MakeOrthographicOffCenterLH(Min.Y, Max.Y, Min.Z, Max.Z, Min.X - 1.0f, Max.X);

	return LightView * LightProj;
}
