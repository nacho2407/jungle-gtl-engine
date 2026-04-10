#include "OBB.h"

FOBB::FOBB()
	: Center(0.0f, 0.0f, 0.0f), Extents(0.0f, 0.0f, 0.0f), Rotation(0.0f, 0.0f, 0.0f, 1.0f)
{
}

FOBB::FOBB(const FVector& InCenter, const FVector& InExtents, const FQuat& InRotation)
	: Center(InCenter), Extents(InExtents), Rotation(InRotation)
{
}

FOBB::FOBB(const FVector& InCenter, const FVector& InExtents, const FMatrix& InMatrix)
	: Center(InCenter), Extents(InExtents), Rotation(InMatrix)
{
}

void FOBB::Reset()
{
	Center = FVector::Zero();
	Extents = FVector::Zero();
	Rotation = FQuat::Identity;
}

bool FOBB::IsValid() const
{
	return Extents.X > 0.0f && Extents.Y > 0.0f && Extents.Z > 0.0f;
}

FOBB FOBB::FromAABB(const FAABB& InAABB, const FMatrix& InTransform)
{
	FOBB Result;

	Result.Center = InAABB.GetCenter();

	FVector Scale;
	Scale.X = InTransform.GetScaledAxis(EAxis::X).Size();
	Scale.Y = InTransform.GetScaledAxis(EAxis::Y).Size();
	Scale.Z = InTransform.GetScaledAxis(EAxis::Z).Size();	

	Result.Extents = InAABB.GetExtent() * Scale;

	Result.Rotation = FQuat(InTransform);

	return Result;
}
