#pragma once


#include "ShapeElem.h"
#include "Core/Types/EngineTypes.h"

#include "Source/Engine/PhysicsEngine/SphylElem.generated.h"

class FMeshElementCollector;

USTRUCT()
struct FKSphylElem : public FKShapeElem
{
	GENERATED_BODY()


	UPROPERTY(Edit, Save, Category="Capsule", DisplayName="Center", Type=Vec3, Speed=0.01f)
	FVector Center;

	UPROPERTY(Edit, Save, Category="Capsule", DisplayName="Rotation", Type=Rotator, Speed=0.1f)
	FRotator Rotation;

	UPROPERTY(Edit, Save, Category="Capsule", DisplayName="Radius", Min=0.001f, Speed=0.01f)
	float Radius;

	UPROPERTY(Edit, Save, Category="Capsule", DisplayName="Length", Min=0.001f, Speed=0.01f)
	float Length;

	FKSphylElem()
		: FKShapeElem(EAggCollisionShape::Sphyl)
		, Center(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, Radius(1), Length(1)
	{

	}

	FKSphylElem(float InRadius, float InLength)
		: FKShapeElem(EAggCollisionShape::Sphyl)
		, Center(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, Radius(InRadius), Length(InLength)
	{

	}

	virtual ~FKSphylElem();



	friend bool operator==(const FKSphylElem& LHS, const FKSphylElem& RHS)
	{
		return (LHS.Center == RHS.Center &&
			LHS.Rotation == RHS.Rotation &&
			LHS.Radius == RHS.Radius &&
			LHS.Length == RHS.Length);
	};

	// Utility function that builds an FTransform from the current data
	virtual FTransform GetTransform() const override final { return FTransform(Center, Rotation); };

	void SetTransform(const FTransform& InTransform)
	{
		Rotation = InTransform.GetRotator();
		Center = InTransform.GetLocation();
	}



	FBoundingBox CalcAABB(const FTransform& BoneTM, float Scale) const;

	void ScaleElem(FVector DeltaSize, float MinSize);

	FKSphylElem GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const;

	float GetScaledRadius(const FVector& Scale3D) const;
	float GetScaledCylinderLength(const FVector& Scale3D) const;
	float GetScaledHalfLength(const FVector& Scale3D) const;
	float GetScaledVolume(const FVector& Scale3D) const;


	float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const;


	float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const;

	static EAggCollisionShape StaticShapeType;
};
