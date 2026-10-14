#include "Physics/ClothCollisionGeometryUtils.h"

#include "Physics/ClothCollisionTypes.h"

namespace
{
	FVector ToClothLocalPoint(const FMatrix& WorldToCloth, const physx::PxVec3& WorldPoint)
	{
		return WorldToCloth.TransformPositionWithW(FVector(WorldPoint.x, WorldPoint.y, WorldPoint.z));
	}

	FVector ToClothLocalVector(const FMatrix& WorldToCloth, const physx::PxVec3& WorldVector)
	{
		FVector V = WorldToCloth.TransformVector(FVector(WorldVector.x, WorldVector.y, WorldVector.z));
		V.Normalize();
		return V;
	}

	void AppendSphereClothCollision(
		const physx::PxSphereGeometry& Sphere,
		const physx::PxTransform& ShapeWorldPose,
		const FClothCollisionGatherParams& Params,
		FClothCollisionData& OutData)
	{
		FVector LocalCenter = ToClothLocalPoint(Params.WorldToCloth, ShapeWorldPose.p);

		OutData.Spheres.push_back(physx::PxVec4(
			LocalCenter.X,
			LocalCenter.Y,
			LocalCenter.Z,
			Sphere.radius));
	}

	void AppendCapsuleClothCollision(
		const physx::PxCapsuleGeometry& Capsule,
		const physx::PxTransform& ShapeWorldPose,
		const FClothCollisionGatherParams& Params,
		FClothCollisionData& OutData)
	{
		const uint32 BaseIndex = static_cast<uint32>(OutData.Spheres.size());

		const physx::PxVec3 LocalA(Capsule.halfHeight, 0.0f, 0.0f);
		const physx::PxVec3 LocalB(-Capsule.halfHeight, 0.0f, 0.0f);

		const physx::PxVec3 WorldA = ShapeWorldPose.transform(LocalA);
		const physx::PxVec3 WorldB = ShapeWorldPose.transform(LocalB);

		FVector ClothA = ToClothLocalPoint(Params.WorldToCloth, WorldA);
		FVector ClothB = ToClothLocalPoint(Params.WorldToCloth, WorldB);

		OutData.Spheres.push_back(physx::PxVec4(ClothA.X, ClothA.Y, ClothA.Z, Capsule.radius));
		OutData.Spheres.push_back(physx::PxVec4(ClothB.X, ClothB.Y, ClothB.Z, Capsule.radius));

		OutData.Capsules.push_back(BaseIndex);
		OutData.Capsules.push_back(BaseIndex + 1);
	}

	void AppendPlaneFromWorldPointNormal(
		const physx::PxVec3& WorldPoint,
		const physx::PxVec3& WorldNormal,
		const FClothCollisionGatherParams& Params,
		FClothCollisionData& OutData)
	{
		FVector LocalPoint = ToClothLocalPoint(Params.WorldToCloth, WorldPoint);
		FVector LocalNormal = ToClothLocalVector(Params.WorldToCloth, WorldNormal);

		const float D = -LocalNormal.Dot(LocalPoint);

		OutData.Planes.push_back(physx::PxVec4(
			LocalNormal.X,
			LocalNormal.Y,
			LocalNormal.Z,
			D));
	}

	void AppendBoxClothCollision(
		const physx::PxBoxGeometry& Box,
		const physx::PxTransform& ShapeWorldPose,
		const FClothCollisionGatherParams& Params,
		FClothCollisionData& OutData)
	{
		const uint32 PlaneBaseIndex = static_cast<uint32>(OutData.Planes.size());
		const physx::PxVec3 Extents = Box.halfExtents;

		struct FBoxPlane
		{
			physx::PxVec3 LocalNormal;
			physx::PxVec3 LocalPoint;
		};

		const FBoxPlane Planes[6] =
		{
			{ physx::PxVec3(1.0f,  0.0f,  0.0f), physx::PxVec3(Extents.x, 0.0f, 0.0f) },
			{ physx::PxVec3(-1.0f, 0.0f,  0.0f), physx::PxVec3(-Extents.x, 0.0f, 0.0f) },
			{ physx::PxVec3(0.0f,  1.0f,  0.0f), physx::PxVec3(0.0f, Extents.y, 0.0f) },
			{ physx::PxVec3(0.0f, -1.0f,  0.0f), physx::PxVec3(0.0f, -Extents.y, 0.0f) },
			{ physx::PxVec3(0.0f,  0.0f,  1.0f), physx::PxVec3(0.0f, 0.0f, Extents.z) },
			{ physx::PxVec3(0.0f,  0.0f, -1.0f), physx::PxVec3(0.0f, 0.0f, -Extents.z) },
		};

		for (const FBoxPlane& Plane : Planes)
		{
			const physx::PxVec3 WorldPoint = ShapeWorldPose.transform(Plane.LocalPoint);
			const physx::PxVec3 WorldNormal = ShapeWorldPose.rotate(Plane.LocalNormal);

			AppendPlaneFromWorldPointNormal(WorldPoint, WorldNormal, Params, OutData);
		}

		uint32 ConvexMask = 0;
		for (uint32 Index = 0; Index < 6; ++Index)
		{
			ConvexMask |= 1u << (PlaneBaseIndex + Index);
		}

		OutData.ConvexMasks.push_back(ConvexMask);
	}
}

void AppendShapeClothCollision(
	const physx::PxShape& Shape,
	const physx::PxTransform& ShapeWorldPose,
	const FClothCollisionGatherParams& Params,
	FClothCollisionData& OutData)
{
	const physx::PxGeometryHolder Geometry = Shape.getGeometry();

	switch (Geometry.getType())
	{
	case physx::PxGeometryType::eSPHERE:
		AppendSphereClothCollision(Geometry.sphere(), ShapeWorldPose, Params, OutData);
		break;

	case physx::PxGeometryType::eCAPSULE:
		AppendCapsuleClothCollision(Geometry.capsule(), ShapeWorldPose, Params, OutData);
		break;

	case physx::PxGeometryType::eBOX:
		AppendBoxClothCollision(Geometry.box(), ShapeWorldPose, Params, OutData);
		break;

	default:
		break;
	}
}
