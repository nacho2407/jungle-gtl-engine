#include "Editor/Viewport/ViewportCamera.h"

void FViewportCamera::SetLocation(const FVector& InLocation)
{
	Location = InLocation;
	MarkViewDirty();
}

void FViewportCamera::SetRotation(const FQuat& InRotation)
{
	Rotation = InRotation;
	Rotation.Normalize();
	MarkViewDirty();
}

void FViewportCamera::SetRotation(const FRotator& InRotation)
{
	Rotation = InRotation.Quaternion();
	Rotation.Normalize();
	MarkViewDirty();
}

FVector FViewportCamera::GetForwardVector() const
{
	return Rotation.GetForwardVector();
}

FVector FViewportCamera::GetRightVector() const
{
	return Rotation.GetRightVector();
}

FVector FViewportCamera::GetUpVector() const
{
	return Rotation.GetUpVector();
}

FMatrix FViewportCamera::GetViewMatrix() const
{
	if (bIsViewDirty)
	{
		const FVector Forward = GetForwardVector().GetSafeNormal();
		CachedViewMatrix = FMatrix::MakeViewLookAtLH(Location, Location + Forward);
		bIsViewDirty = false;
	}

	return CachedViewMatrix;
}

FMatrix FViewportCamera::GetProjectionMatrix() const
{
	if (bIsProjectionDirty)
	{
		switch (ProjectionType)
		{
		case EViewportProjectionType::Perspective:
		{
			CachedProjectionMatrix =
				FMatrix::MakePerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
			break;
		}
		case EViewportProjectionType::Orthographic:
		{
			CachedProjectionMatrix =
				FMatrix::MakeOrthographicLH(OrthoHeight * AspectRatio, OrthoHeight, NearPlane, FarPlane);
			break;
		}
		default:
			break;
		}

		bIsProjectionDirty = false;
	}

	return CachedProjectionMatrix;
}

FMatrix FViewportCamera::GetViewProjectionMatrix() const
{
	return GetViewMatrix() * GetProjectionMatrix();
}

FRay FViewportCamera::DeprojectScreenToWorld(float ScreenX, float ScreenY, float ScreenWidth, float ScreenHeight) const
{
	if (ScreenWidth <= 0.0f || ScreenHeight <= 0.0f)
	{
		return FRay{ GetLocation(), GetForwardVector().GetSafeNormal() };
	}

	const float NdcX = (2.0f * ScreenX) / ScreenWidth - 1.0f;
	const float NdcY = 1.0f - (2.0f * ScreenY) / ScreenHeight;

	const FVector NdcNear(NdcX, NdcY, 0.0f);
	const FVector NdcFar(NdcX, NdcY, 1.0f);

	const FMatrix InverseViewProjection = GetViewProjectionMatrix().GetInverse();
	const FVector WorldNear = InverseViewProjection.TransformPosition(NdcNear);
	const FVector WorldFar = InverseViewProjection.TransformPosition(NdcFar);

	FRay Ray{};
	Ray.Origin = WorldNear;
	Ray.Direction = (WorldFar - WorldNear).GetSafeNormal();
	if (Ray.Direction.IsNearlyZero())
	{
		Ray.Direction = GetForwardVector().GetSafeNormal();
	}

	return Ray;
}

void FViewportCamera::SetProjectionType(EViewportProjectionType InType)
{
	ProjectionType = InType;
	MarkProjectionDirty();
}

void FViewportCamera::SetFOV(float InFOV)
{
	FOV = InFOV;
	MarkProjectionDirty();
}

void FViewportCamera::SetNearPlane(float InNear)
{
	NearPlane = InNear;
	MarkProjectionDirty();
}

void FViewportCamera::SetFarPlane(float InFar)
{
	FarPlane = InFar;
	MarkProjectionDirty();
}

void FViewportCamera::SetOrthoHeight(float InHeight)
{
	OrthoHeight = InHeight;
	MarkProjectionDirty();
}

void FViewportCamera::SetLookAt(const FVector &Target) {
	FVector Forward = (Target - Location).GetSafeNormal();
	if (Forward.IsNearlyZero()) return;

	FVector UpRef = FVector::UpVector;
	if (std::abs(Forward.DotProduct(UpRef)) > 0.99f) 
	{
		UpRef = FVector(1.0f, 0.0f, 0.0f);
	}

	FVector Right = FVector::CrossProduct(UpRef, Forward).GetSafeNormal();
	FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();

	FMatrix RotMat = FMatrix::Identity;
	RotMat.SetAxes(Forward, Right, Up);

	FQuat QuatRot(RotMat);
	QuatRot.Normalize();
	SetRotation(QuatRot);
}

void FViewportCamera::OnResize(uint32 InWidth, uint32 InHeight)
{
	Width = InWidth;
	Height = (InHeight == 0) ? 1u : InHeight;
	AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
	MarkProjectionDirty();
}
