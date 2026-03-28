#include "ViewportTypes.h"

#include <algorithm>
#include "Math/MathUtility.h"

FViewportLocalState FViewportLocalState::CreateDefault(EViewportType Type)
{
	FViewportLocalState State;
	State.ProjectionType = Type;

	State.NearPlane = 0.1f;
	State.FarPlane = 10000.0f;
	State.FovY = 60.0f;

	State.OrthoTarget = FVector::ZeroVector;
	State.OrthoZoom = 1000.0f;

	State.bShowGrid = true;
	State.GridSize = 100.0f;
	State.ViewMode = ERenderMode::Lighting;

	switch (Type)
	{
	case EViewportType::Perspective:
		State.Position = FVector(0.0f, 0.0f, -5.0f);
		State.Rotation = FRotator::ZeroRotator;
		break;

	case EViewportType::OrthoTop:
	case EViewportType::OrthoFront:
	case EViewportType::OrthoRight:
		State.Position = FVector::ZeroVector;
		State.Rotation = FRotator::ZeroRotator;
		break;

	default:
		break;
	}

	return State;
}

FMatrix FViewportLocalState::BuildViewMatrix() const
{
	switch (ProjectionType)
	{
	case EViewportType::Perspective:
	{
		const FVector Eye = Position;
		const FVector Forward = Rotation.Vector().GetSafeNormal();
		const FVector Up = Rotation.RotateVector(FVector::UpVector).GetSafeNormal();

		return FMatrix::MakeViewLookAtLH(Eye, Eye + Forward, Up);
	}

	case EViewportType::OrthoTop:
	{
		const FVector Eye = OrthoTarget + FVector::UpVector * OrthoZoom;
		const FVector Forward = FVector::DownVector;
		const FVector Up = FVector::ForwardVector;
		return FMatrix::MakeViewLookAtLH(Eye, Eye + Forward, Up);
	}

	case EViewportType::OrthoFront:
	{
		const FVector Eye = OrthoTarget + FVector::ForwardVector * OrthoZoom;
		const FVector Forward = FVector::BackwardVector;
		const FVector Up = FVector::UpVector;
		return FMatrix::MakeViewLookAtLH(Eye, Eye + Forward, Up);
	}

	case EViewportType::OrthoRight:
	{
		const FVector Eye = OrthoTarget + FVector::RightVector * OrthoZoom;
		const FVector Forward = FVector::LeftVector;
		const FVector Up = FVector::UpVector;
		return FMatrix::MakeViewLookAtLH(Eye, Eye + Forward, Up);
	}

	default:
		return FMatrix::Identity;
	}
}

FMatrix FViewportLocalState::BuildProjMatrix(float AspectRatio) const
{
	const float SafeAspect = (std::max)(AspectRatio, 0.01f);
	const float SafeNear = (std::max)(NearPlane, 0.001f);
	const float SafeFar = (std::max)(FarPlane, SafeNear + 0.001f);

	if (ProjectionType == EViewportType::Perspective)
	{
		return FMatrix::MakePerspectiveFovLH(
			FMath::DegreesToRadians(FovY),
			SafeAspect,
			SafeNear,
			SafeFar
		);
	}

	const float SafeZoom = (std::max)(OrthoZoom, 0.01f);
	const float ViewHeight = SafeZoom * 2.0f;
	const float ViewWidth = ViewHeight * SafeAspect;

	return FMatrix::MakeOrthographicLH(
		ViewWidth,
		ViewHeight,
		SafeNear,
		SafeFar
	);
}