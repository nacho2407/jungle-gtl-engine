#include "LightComponent.h"
#include "Object/ObjectFactory.h"

#include "UI/EditorConsoleWidget.h"

DEFINE_CLASS(ULightComponent, ULightComponentBase)
REGISTER_FACTORY(ULightComponent)

FMatrix ULightComponent::GetPSMMatrix(const FMatrix& CamView, const FMatrix& CamProj) const
{
	FVector LightPos = GetWorldLocation();
	FVector LightDir = GetForwardVector();

	FMatrix CamViewProj = CamView * CamProj;

	auto ToPost = [&](const FVector& P)
		{
			FVector4 Clip = FVector4(P.X, P.Y, P.Z, 1.0f) * CamViewProj;
			return FVector(
				Clip.X / Clip.W,
				Clip.Y / Clip.W,
				Clip.Z / Clip.W
			);
		};

	FVector P0 = FVector::ZeroVector;
	FVector P1 = P0 + LightDir;

	FVector Post0 = ToPost(P0);
	FVector Post1 = ToPost(P1);

	FVector LightNDCDir = Post1 - Post0;
	LightNDCDir.Normalize();

	FVector Center = FVector(0.0f, 0.0f, 0.5f);
	FVector Eye = Center - LightNDCDir * 2.0f;

	FMatrix LightNDCView = FMatrix::MakeViewLookAtLH(Eye, Center, FVector(0, 1, 0));

	FVector Corners[8] =
	{
		{ -1, 1, 0 },
		{ 1, -1, 0 },
		{ -1, -1, 0 },
		{ 1, 1, 0 },
		{ -1, 1, 1 },
		{ 1, -1, 1 },
		{ -1, -1, 1 },
		{ 1, 1, 1 }
	};

	FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const FVector& C : Corners)
	{
		FVector4 C4 = FVector4(C.X, C.Y, C.Z, 1.0f);
		FVector4 V = C4 * LightNDCView;
		FVector P(V.X, V.Y, V.Z);

		Min = FVector::Min(Min, P);
		Max = FVector::Max(Max, P);
	}

	FMatrix LightNDCProj = FMatrix::MakeOrthographicOffCenterLH(Min.Y, Max.Y, Min.Z, Max.Z, Min.X, Max.X);

	FMatrix Result = LightNDCView * LightNDCProj;

	return Result;
}
