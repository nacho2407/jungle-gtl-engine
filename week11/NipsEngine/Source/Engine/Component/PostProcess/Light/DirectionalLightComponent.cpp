#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UDirectionalLightComponent, ULightComponent)
REGISTER_FACTORY(UDirectionalLightComponent)

void UDirectionalLightComponent::PostDuplicate(UObject* Original)
{
    ULightComponentBase::PostDuplicate(Original);
    UDirectionalLightComponent* Orig = Cast<UDirectionalLightComponent>(Original);
}

void UDirectionalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Shadow Resolution Scale", EPropertyType::Float, &ShadowResolutionScale });
	OutProps.push_back({ "Shadow Bias", EPropertyType::Float, &ShadowBias });
	OutProps.push_back({ "Shadow Slope Bias", EPropertyType::Float, &ShadowSlopeBias });
	OutProps.push_back({ "Shadow Sharpen", EPropertyType::Float, &ShadowSharpen });
}

FMatrix UDirectionalLightComponent::ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	FVector LightDir = GetForwardVector().GetSafeNormal();
	FMatrix CamViewProj = CamView * CamProj;

	auto ToPost = [&](const FVector& P)
		{
			FVector4 Clip = FVector4(P, 1.0f) * CamViewProj;

			float InvW = (MathUtil::Abs(Clip.W) > MathUtil::Epsilon) ? (1.0f / Clip.W) : 1.0f;

			return FVector(
				Clip.X * InvW,
				Clip.Y * InvW,
				Clip.Z * InvW
			);
		};

	FVector CombinedCenterWorld(0, 0, 0);
	TArray<FVector> RelevantPoints;

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

	if (VisibleObjectsBounds == nullptr)
	{
		for (int i = 0; i < 8; ++i)
		{
			RelevantPoints.push_back(PostCorners[i]);
		}
		CombinedCenterWorld = FVector::ZeroVector;
	}
	else
	{
		FBoundingBox TotalBox;
		for (const auto& Box : *VisibleObjectsBounds)
		{
			TotalBox.Merge(Box);
			FVector Corners[8];
			Box.GetVertices(Corners);

			for (int i = 0; i < 8; ++i)
			{
				RelevantPoints.push_back(ToPost(Corners[i]));
			}
		}
		CombinedCenterWorld = TotalBox.GetCenter();
	}

	FVector Center = FVector::ZeroVector;
	for (const FVector& C : PostCorners)
	{
		Center += C;
	}
	Center /= 8.0f;

	const float Delta = 1.0f;

	FVector NearCenterWorld = FVector::ZeroVector;

	FVector Post0 = ToPost(NearCenterWorld);
	FVector Post1 = ToPost(NearCenterWorld + LightDir * Delta);

	FVector LightDirPost = (Post1 - Post0).GetSafeNormal();
	if (LightDirPost.IsNearlyZero())
	{
		LightDirPost = FVector(1, 0, 0);
	}

	FVector Eye = Center - LightDirPost * 100.0f;
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

	return LightNDCView * LightNDCProj;;
}

