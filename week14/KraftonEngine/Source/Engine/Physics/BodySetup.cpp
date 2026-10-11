#include "Physics/BodySetup.h"

#include "Mesh/Static/StaticMeshAsset.h"
#include "Physics/PhysXInclude.h"
#include "Serialization/Archive.h"

UBodySetup::~UBodySetup()
{
	if (CookedTriangleMesh)
	{
		CookedTriangleMesh->release();
		CookedTriangleMesh = nullptr;
	}
}

void UBodySetup::Serialize(FArchive& Ar)
{
	Ar << BoneName;
	Ar << AggGeom;
}

void UBodySetup::SerializeComplexCollision(FArchive& Ar)
{
	Ar << ComplexCollisionVertices;
	Ar << ComplexCollisionIndices;

	if (Ar.IsLoading() && CookedTriangleMesh)
	{
		CookedTriangleMesh->release();
		CookedTriangleMesh = nullptr;
	}
}

void UBodySetup::CreateDefaultBox(const FVector& Center, const FVector& Extents)
{
	AggGeom.BoxElems.clear();

	FKBoxElem Box;
	Box.Center = Center;
	Box.Rotation = FQuat::Identity;
	Box.Extents = Extents;

	AggGeom.BoxElems.push_back(Box);
}

void UBodySetup::AddSphere(const FVector& Center, float Radius)
{
	FKSphereElem Sphere;
	Sphere.Center = Center;
	Sphere.Radius = Radius;

	AggGeom.SphereElems.push_back(Sphere);
}

void UBodySetup::AddBox(const FVector& Center, const FQuat& Rotation, const FVector& Extents)
{
	FKBoxElem Box;
	Box.Center = Center;
	Box.Rotation = Rotation;
	Box.Extents = Extents;

	AggGeom.BoxElems.push_back(Box);
}

void UBodySetup::AddSphyl(const FVector& Center, const FQuat& Rotation, float Radius, float Length)
{
	FKSphylElem Sphyl;
	Sphyl.Center = Center;
	Sphyl.Rotation = Rotation;
	Sphyl.Radius = Radius;
	Sphyl.Length = Length;
	
	AggGeom.SphylElems.push_back(Sphyl);
}

int32 UBodySetup::GetShapeCount(EPhysicsAssetShapeType ShapeType) const
{
	switch (ShapeType)
	{
	case EPhysicsAssetShapeType::Sphere:
		return static_cast<int32>(AggGeom.SphereElems.size());
	case EPhysicsAssetShapeType::Box:
		return static_cast<int32>(AggGeom.BoxElems.size());
	case EPhysicsAssetShapeType::Sphyl:
		return static_cast<int32>(AggGeom.SphylElems.size());
	case EPhysicsAssetShapeType::Convex:
		return static_cast<int32>(AggGeom.ConvexElems.size());
	default:
		return 0;
	}
}

bool UBodySetup::RemoveShape(EPhysicsAssetShapeType ShapeType, int32 ShapeIndex)
{
	if (ShapeIndex < 0)
	{
		return false;
	}

	switch (ShapeType)
	{
	case EPhysicsAssetShapeType::Sphere:
		if (ShapeIndex >= static_cast<int32>(AggGeom.SphereElems.size())) return false;
		AggGeom.SphereElems.erase(AggGeom.SphereElems.begin() + ShapeIndex);
		return true;
	case EPhysicsAssetShapeType::Box:
		if (ShapeIndex >= static_cast<int32>(AggGeom.BoxElems.size())) return false;
		AggGeom.BoxElems.erase(AggGeom.BoxElems.begin() + ShapeIndex);
		return true;
	case EPhysicsAssetShapeType::Sphyl:
		if (ShapeIndex >= static_cast<int32>(AggGeom.SphylElems.size())) return false;
		AggGeom.SphylElems.erase(AggGeom.SphylElems.begin() + ShapeIndex);
		return true;
	case EPhysicsAssetShapeType::Convex:
		if (ShapeIndex >= static_cast<int32>(AggGeom.ConvexElems.size())) return false;
		AggGeom.ConvexElems.erase(AggGeom.ConvexElems.begin() + ShapeIndex);
		return true;
	default:
		return false;
	}
}

void UBodySetup::ClearShapes()
{
	AggGeom.SphereElems.clear();
	AggGeom.BoxElems.clear();
	AggGeom.SphylElems.clear();
	AggGeom.ConvexElems.clear();
}

void UBodySetup::BuildComplexCollisionFromStaticMesh(const FStaticMesh& Mesh)
{
	if (CookedTriangleMesh)
	{
		CookedTriangleMesh->release();
		CookedTriangleMesh = nullptr;
	}

	ComplexCollisionVertices.clear();
	ComplexCollisionIndices.clear();

	if (Mesh.Vertices.empty() || Mesh.Indices.size() < 3) return;

	ComplexCollisionVertices.reserve(Mesh.Vertices.size());
	for (const FNormalVertex& Vertex : Mesh.Vertices)
	{
		ComplexCollisionVertices.push_back(Vertex.pos);
	}

	ComplexCollisionIndices.reserve(Mesh.Indices.size());
	for (uint32 Index : Mesh.Indices)
	{
		if (Index >= static_cast<uint32>(ComplexCollisionVertices.size()))
		{
			ComplexCollisionVertices.clear();
			ComplexCollisionIndices.clear();
			return;
		}

		ComplexCollisionIndices.push_back(Index);
	}
}
void UBodySetup::SetCookedTriangleMesh(physx::PxTriangleMesh* InMesh) const
{
	if (CookedTriangleMesh == InMesh) return;

	if (CookedTriangleMesh)
	{
		CookedTriangleMesh->release();
	}

	CookedTriangleMesh = InMesh;
}
