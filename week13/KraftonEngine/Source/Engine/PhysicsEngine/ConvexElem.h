#pragma once

#include "ShapeElem.h"
#include "Core/Types/EngineTypes.h"

class FMeshElementCollector;

struct FKConvexElem : public FKShapeElem
{
	TArray<FVector> VertexData;
	TArray<int32> IndexData;
	FBoundingBox ElemBox;

	FKConvexElem()
		: FKShapeElem(EAggCollisionShape::Convex)
		, ElemBox()
		, Transform()
	{
	}

	virtual ~FKConvexElem() = default;

	virtual FTransform GetTransform() const override final
	{
		return Transform;
	}

	void SetTransform(const FTransform& InTransform)
	{
		Transform = InTransform;
	}

	void Reset();

	void UpdateElemBox();

	FBoundingBox CalcAABB(const FTransform& BoneTM, const FVector& Scale3D) const;

	void ScaleElem(FVector DeltaSize, float MinSize);

	float GetScaledVolume(const FVector& Scale3D) const;

	float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const;

	float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const;

	inline static constexpr EAggCollisionShape StaticShapeType = EAggCollisionShape::Convex;

private:
	FTransform Transform;
};
