#pragma once

#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Core/Types/CoreTypes.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Physics/PhysicsGeometry.generated.h"

USTRUCT()
struct FKSphereElem
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Shape")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Shape", Min=0.1f)
	float Radius = 0.5f;
};

USTRUCT()
struct FKBoxElem
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Shape")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Shape")
	FQuat Rotation = FQuat::Identity;

	UPROPERTY(Edit, Save, Category="Shape", Min=0.01f)
	FVector Extents = FVector::OneVector;
};

USTRUCT()
struct FKSphylElem
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Shape")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category="Shape")
	FQuat Rotation = FQuat::Identity;

	UPROPERTY(Edit, Save, Category="Shape", Min=0.01f)
	float Radius = 0.5f;

	UPROPERTY(Edit, Save, Category="Shape", Min=0.01f)
	float Length = 1.0f;
};

USTRUCT()
struct FKConvexElem
{
	GENERATED_BODY()

	TArray<FVector> VertexData;
};

USTRUCT()
struct FKAggregateGeom
{
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="Shape")
	TArray<FKSphereElem> SphereElems;

	UPROPERTY(Edit, Save, Category="Shape")
	TArray<FKBoxElem> BoxElems;

	UPROPERTY(Edit, Save, Category="Shape")
	TArray<FKSphylElem> SphylElems;

	UPROPERTY(Edit, Save, Category="Shape")
	TArray<FKConvexElem> ConvexElems;

	bool IsEmpty() const
	{
		return SphereElems.empty()
			&& BoxElems.empty()
			&& SphylElems.empty()
			&& ConvexElems.empty();
	}
};
