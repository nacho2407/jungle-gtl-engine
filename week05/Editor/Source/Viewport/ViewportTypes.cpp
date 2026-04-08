#include "ViewportTypes.h"

#include <algorithm>
#include "Math/MathUtility.h"

FViewportLocalState FViewportLocalState::CreateDefault(EViewportType Type)
{
	FViewportLocalState State;
	State.ProjectionType = Type;

	State.NearPlane = 0.001f;
	State.FarPlane = 10000.0f;
	State.FovY = 60.0f;

	State.OrthoTarget = FVector::ZeroVector + FVector(0.0f, 0.0f, 1.0f);
	State.OrthoZoom = 10.0f;
	State.bShowGrid = true;
	State.ViewMode = ERenderMode::Lighting;

	switch (Type)
	{
	case EViewportType::Perspective:
		State.Position = FVector(-10.0f, 0.0f, 1.0f);
		State.Rotation = FRotator::ZeroRotator;
		break;

	case EViewportType::OrthoTop:
	case EViewportType::OrthoBottom:
	case EViewportType::OrthoLeft:
	case EViewportType::OrthoRight:
	case EViewportType::OrthoFront:
	case EViewportType::OrthoBack:
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
		const FVector Up = FVector::UpVector;

		return FMatrix::MakeViewLookAtLH(Eye, Eye + Forward, Up);
	}

	case EViewportType::OrthoTop:
	{
		const FVector Eye = OrthoTarget + FVector::UpVector * OrthoZoom;
		const FVector Forward = FVector::DownVector;
		const FVector Up = FVector::ForwardVector;
		return FMatrix::MakeViewLookAtLH(Eye, Eye + Forward, Up);
	}

	case EViewportType::OrthoBottom:
	{
		const FVector Eye = OrthoTarget + FVector::DownVector * OrthoZoom;
		const FVector Forward = FVector::UpVector;
		const FVector Up = FVector::ForwardVector;
		return FMatrix::MakeViewLookAtLH(Eye, Eye + Forward, Up);
	}

	case EViewportType::OrthoLeft:
	{
		const FVector Eye = OrthoTarget + FVector::LeftVector * OrthoZoom;
		const FVector Forward = FVector::RightVector;
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

	case EViewportType::OrthoFront:
	{
		const FVector Eye = OrthoTarget + FVector::ForwardVector * OrthoZoom;
		const FVector Forward = FVector::BackwardVector;
		const FVector Up = FVector::UpVector;
		return FMatrix::MakeViewLookAtLH(Eye, Eye + Forward, Up);
	}

	case EViewportType::OrthoBack:
	{
		const FVector Eye = OrthoTarget + FVector::BackwardVector * OrthoZoom;
		const FVector Forward = FVector::ForwardVector;
		const FVector Up = FVector::UpVector;
		return FMatrix::MakeViewLookAtLH(Eye, Eye + Forward, Up);
	}

	default:
	{
		const FVector Eye = Position;
		const FVector Forward = Rotation.Vector().GetSafeNormal();
		const FVector Up = FVector::UpVector;

		return FMatrix::MakeViewLookAtLH(Eye, Eye + Forward, Up);
	}
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

	const float SafeZoom = (std::max)(OrthoZoom, 0.001f);
	const float ViewHeight = SafeZoom * 2.0f;
	const float ViewWidth = ViewHeight * SafeAspect;

	return FMatrix::MakeOrthographicLH(
		ViewWidth,
		ViewHeight,
		SafeNear,
		SafeFar
	);
}
