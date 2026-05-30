#pragma once

#include "ShapeElem.h"
#include "Core/Types/EngineTypes.h"

class FMaterialRenderProxy;
class FMeshElementCollector;

struct FKBoxElem : public FKShapeElem
{
	FVector Center;
	FRotator Rotation;
	float X;
	float Y;
	float Z;

	FKBoxElem()
		: FKShapeElem(EAggCollisionShape::Box)
		, Center(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, X(1)
		, Y(1)
		, Z(1)
	{
	}

	FKBoxElem(float s)
		: FKShapeElem(EAggCollisionShape::Box)
		, Center(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, X(s)
		, Y(s)
		, Z(s)
	{
	}

	FKBoxElem(float InX, float InY, float InZ)
		: FKShapeElem(EAggCollisionShape::Box)
		, Center(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, X(InX)
		, Y(InY)
		, Z(InZ)
	{
	}

	virtual ~FKBoxElem() = default;

	friend bool operator==(const FKBoxElem& LHS, const FKBoxElem& RHS)
	{
		return LHS.Center == RHS.Center
			&& LHS.Rotation == RHS.Rotation
			&& LHS.X == RHS.X
			&& LHS.Y == RHS.Y
			&& LHS.Z == RHS.Z;
	}

	virtual FTransform GetTransform() const override final
	{
		return FTransform(Center, Rotation);
	}

	void SetTransform(const FTransform& InTransform)
	{
		Rotation = InTransform.GetRotator();
		Center = InTransform.GetLocation();
	}

	FBoundingBox CalcAABB(const FTransform& BoneTM, float Scale) const;

	void ScaleElem(FVector DeltaSize, float MinSize);

	FKBoxElem GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const;

	float GetScaledVolume(const FVector& Scale3D) const;

	inline static constexpr EAggCollisionShape StaticShapeType = EAggCollisionShape::Box;

	float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const;

	float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const;
};
