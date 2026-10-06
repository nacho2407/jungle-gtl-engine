#pragma once

#include "ShapeElem.h"
#include "Core/Types/EngineTypes.h"

#include "Source/Engine/PhysicsEngine/BoxElem.generated.h"

class FMaterialRenderProxy;
class FMeshElementCollector;

USTRUCT()
struct FKBoxElem : public FKShapeElem
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Box", DisplayName="Center", Type=Vec3, Speed=0.01f)
	FVector Center;

	UPROPERTY(Edit, Save, Category="Box", DisplayName="Rotation", Type=Rotator, Speed=0.1f)
	FRotator Rotation;

	UPROPERTY(Edit, Save, Category="Box", DisplayName="X", Min=0.001f, Speed=0.01f)
	float X;

	UPROPERTY(Edit, Save, Category="Box", DisplayName="Y", Min=0.001f, Speed=0.01f)
	float Y;

	UPROPERTY(Edit, Save, Category="Box", DisplayName="Z", Min=0.001f, Speed=0.01f)
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
