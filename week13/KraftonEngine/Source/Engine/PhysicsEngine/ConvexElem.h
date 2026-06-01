#pragma once

#include "ShapeElem.h"
#include "Core/Types/EngineTypes.h"

#include "Source/Engine/PhysicsEngine/ConvexElem.generated.h"

class FMeshElementCollector;

USTRUCT()
struct FKConvexElem : public FKShapeElem
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Save, Category="Convex", DisplayName="Vertex Data", Type=Array)
	TArray<FVector> VertexData;

	UPROPERTY(VisibleAnywhere, Save, Category="Convex", DisplayName="Index Data", Type=Array)
	TArray<int32> IndexData;

	UPROPERTY(VisibleAnywhere, Save, Category="Convex", DisplayName="Elem Box", Type=Struct)
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
	UPROPERTY(Edit, Save, Category="Convex", DisplayName="Transform", Type=Struct)
	FTransform Transform;
};
