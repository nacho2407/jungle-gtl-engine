#pragma once

#include "Core/Types/CoreTypes.h"
#include "Engine/Object/Reflection/ObjectMacros.h"
#include "Engine/Object/Reflection/UStruct.h"
#include "Engine/Object/FName.h"
#include "Core/Types/CollisionTypes.h"
#include "Engine/Math/Transform.h"

#include "Source/Engine/PhysicsEngine/ShapeElem.generated.h"

UENUM()
enum class EAggCollisionShape : uint8
{
	Sphere,
	Box,
	Sphyl,
	Convex,
	TaperedCapsule,
	LevelSet,
	SkinnedLevelSet,
	MLLevelSet,
	SkinnedTriangleMesh,

	Unknown
};

USTRUCT()
struct FKShapeElem
{
	GENERATED_BODY()

	FKShapeElem()
		: RestOffset(0.f)
		, ShapeType(EAggCollisionShape::Unknown)
		, bContributeToMass(true)
		, CollisionEnabled(ECollisionEnabled::QueryAndPhysics)
	{
	}

	FKShapeElem(EAggCollisionShape InShapeType)
		: RestOffset(0.f)
		, ShapeType(InShapeType)
		, bContributeToMass(true)
		, CollisionEnabled(ECollisionEnabled::QueryAndPhysics)
	{
	}

	FKShapeElem(const FKShapeElem& Copy)
		: RestOffset(Copy.RestOffset)
		, Name(Copy.Name)
		, ShapeType(Copy.ShapeType)
		, bContributeToMass(Copy.bContributeToMass)
		, CollisionEnabled(Copy.CollisionEnabled)
	{
	}

	virtual ~FKShapeElem() = default;

	const FName& GetName() const { return Name; }
	void SetName(const FName& InName) { Name = InName; }

	EAggCollisionShape GetShapeType() const { return ShapeType; }
	bool GetContributeToMass() const { return bContributeToMass; }

	void SetContributeToMass(bool bInContributeToMass) { bContributeToMass = bInContributeToMass; }
	void SetCollisionEnabled(ECollisionEnabled InCollisionEnabled) { CollisionEnabled = InCollisionEnabled; }

	ECollisionEnabled GetCollisionEnabled() const { return CollisionEnabled; }

	virtual FTransform GetTransform() const { return FTransform(); }

	const FKShapeElem& operator=(const FKShapeElem& Other)
	{
		CloneElem(Other);
		return *this;
	}

	template <typename T>
	T* GetShapeCheck()
	{
		assert(T::StaticShapeType == ShapeType);
		return static_cast<T*>(this);
	}

	inline static constexpr EAggCollisionShape StaticShapeType = EAggCollisionShape::Unknown;

	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Rest Offset", Min=0.0f, Speed=0.01f)
	float RestOffset;

protected:
	void CloneElem(const FKShapeElem& Other)
	{
		RestOffset = Other.RestOffset;
		ShapeType = Other.ShapeType;
		Name = Other.Name;
		bContributeToMass = Other.bContributeToMass;
		CollisionEnabled = Other.CollisionEnabled;
	}

private:
	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Name")
	FName Name;

	EAggCollisionShape ShapeType;

	UPROPERTY(Edit, Save, Category="Shape", DisplayName="Contribute To Mass")
	bool bContributeToMass = true;

	ECollisionEnabled CollisionEnabled;
};
