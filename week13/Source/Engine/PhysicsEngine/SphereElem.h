#pragma once

#include "ShapeElem.h"
#include "Engine/Math/Vector.h"
#include "Core/Types/EngineTypes.h"

#include "Source/Engine/PhysicsEngine/SphereElem.generated.h"

USTRUCT()
struct FKSphereElem : public FKShapeElem
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Sphere", DisplayName="Center", Type=Vec3, Speed=0.01f)
	FVector Center;

	UPROPERTY(Edit, Save, Category="Sphere", DisplayName="Radius", Min=0.001f, Speed=0.01f)
	float Radius;

	FKSphereElem()
		: FKShapeElem(EAggCollisionShape::Sphere)
		, Center(FVector::ZeroVector)
		, Radius(1)
	{
	}

	FKSphereElem(float r)
		: FKShapeElem(EAggCollisionShape::Sphere)
		, Center(FVector::ZeroVector)
		, Radius(r)
	{
	}

	virtual ~FKSphereElem() = default;

	friend bool operator==(const FKSphereElem& LHS, const FKSphereElem& RHS)
	{
		return LHS.Center == RHS.Center
			&& LHS.Radius == RHS.Radius;
	}

	virtual FTransform GetTransform() const override final
	{
		return FTransform(Center);
	}

	void SetTransform(const FTransform& InTransform)
	{
		Center = InTransform.GetLocation();
	}

	FBoundingBox CalcAABB(const FTransform& BoneTM, float Scale) const;

	void ScaleElem(FVector DeltaSize, float MinSize);

	inline static constexpr EAggCollisionShape StaticShapeType = EAggCollisionShape::Sphere;

	FKSphereElem GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const;

	float GetScaledVolume(const FVector& Scale3D) const;

	float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const;

	float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const;
};
