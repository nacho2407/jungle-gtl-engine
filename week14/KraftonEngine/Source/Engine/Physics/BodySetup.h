#pragma once

#include "Physics/BodySetupCore.h"
#include "Physics/PhysicsGeometry.h"

#include "Source/Engine/Physics/BodySetup.generated.h"

UENUM()
enum class EPhysicsAssetShapeType : uint8
{
	Sphere,
	Box,
	Sphyl,
	Convex
};

namespace physx
{
	class PxTriangleMesh;
}

struct FStaticMesh;

UCLASS()
class UBodySetup : public UBodySetupCore
{
public:
	GENERATED_BODY()

	~UBodySetup() override;
	
	void Serialize(FArchive& Ar) override; 

	FKAggregateGeom& GetAggGeom() { return AggGeom; }
	const FKAggregateGeom& GetAggGeom() const { return AggGeom; }

	bool HasSimpleCollision() const { return !AggGeom.IsEmpty(); }
	bool HasComplexCollision() const { return !ComplexCollisionVertices.empty() && ComplexCollisionIndices.size() >= 3; }

	void CreateDefaultBox(const FVector& Center, const FVector& Extents);

	void AddSphere(const FVector& Center, float Radius);
	void AddBox(const FVector& Center, const FQuat& Rotation, const FVector& Extents);
	void AddSphyl(const FVector& Center, const FQuat& Rotation, float Radius, float Length);
	int32 GetShapeCount(EPhysicsAssetShapeType ShapeType) const;
	bool RemoveShape(EPhysicsAssetShapeType ShapeType, int32 ShapeIndex);
	void ClearShapes();

	void BuildComplexCollisionFromStaticMesh(const FStaticMesh& Mesh);

	const TArray<FVector>& GetComplexCollisionVertices() const { return ComplexCollisionVertices; }
	const TArray<uint32>& GetComplexCollisionIndices() const { return ComplexCollisionIndices; }

	physx::PxTriangleMesh* GetCookedTriangleMesh() const { return CookedTriangleMesh; }
	void SetCookedTriangleMesh(physx::PxTriangleMesh* InMesh) const;

protected:
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Primitives")
	FKAggregateGeom AggGeom;

	TArray<FVector> ComplexCollisionVertices;
	TArray<uint32> ComplexCollisionIndices;

	mutable physx::PxTriangleMesh* CookedTriangleMesh = nullptr;
};
