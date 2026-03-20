#include "PrimitiveComponent.h"
#include "Core/RayTypes.h"
#include "Mesh/MeshManager.h"
#include "Core/CollisionTypes.h"

DEFINE_CLASS(UPrimitiveComponent, USceneComponent)
DEFINE_CLASS(UCubeComponent, UPrimitiveComponent)
DEFINE_CLASS(USphereComponent, UPrimitiveComponent)
DEFINE_CLASS(UPlaneComponent, UPrimitiveComponent)
REGISTER_FACTORY(UCubeComponent)
REGISTER_FACTORY(USphereComponent)
REGISTER_FACTORY(UPlaneComponent)

void UPrimitiveComponent::UpdateWorldAABB()
{
	FVector LExt = LocalExtents;

	FMatrix worldMatrix = GetWorldMatrix();

	float NewEx = std::abs(worldMatrix.M[0][0]) * LExt.X + std::abs(worldMatrix.M[1][0]) * LExt.Y + std::abs(worldMatrix.M[2][0]) * LExt.Z;
	float NewEy = std::abs(worldMatrix.M[0][1]) * LExt.X + std::abs(worldMatrix.M[1][1]) * LExt.Y + std::abs(worldMatrix.M[2][1]) * LExt.Z;
	float NewEz = std::abs(worldMatrix.M[0][2]) * LExt.X + std::abs(worldMatrix.M[1][2]) * LExt.Y + std::abs(worldMatrix.M[2][2]) * LExt.Z;

	FVector WorldCenter = GetWorldLocation();
	WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
}

bool UPrimitiveComponent::CheckAABB(const FRay& Ray)
{
	float tMin = -INFINITY;
	float tMax = INFINITY;

	for (int i = 0; i < 3; ++i)
	{
		float invDir = 1.0f / (i == 0 ? Ray.Direction.X : (i == 1 ? Ray.Direction.Y : Ray.Direction.Z));
		float origin = (i == 0 ? Ray.Origin.X : (i == 1 ? Ray.Origin.Y : Ray.Origin.Z));
		float minBound = (i == 0 ? WorldAABBMinLocation.X : (i == 1 ? WorldAABBMinLocation.Y : WorldAABBMinLocation.Z));
		float maxBound = (i == 0 ? WorldAABBMaxLocation.X : (i == 1 ? WorldAABBMaxLocation.Y : WorldAABBMaxLocation.Z));

		float t1 = (minBound - origin) * invDir;
		float t2 = (maxBound - origin) * invDir;

		if (t1 > t2) std::swap(t1, t2);

		tMin = std::max(tMin, t1);
		tMax = std::min(tMax, t2);

		if (tMin > tMax) return false;
	}

	return tMax >= 0;
}

bool UPrimitiveComponent::Raycast(const FRay& Ray, FHitResult& OutHitResult)
{
	if(bPendingKill || !bIsVisible)
		return false;
	
	if (!CheckAABB(Ray))
		return false;

	return RaycastMesh(Ray, OutHitResult);
}

bool UPrimitiveComponent::IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir, const FVector& V0,
	const FVector& V1, const FVector& V2, float& OutT)
{
	FVector edge1 = V1 - V0;
	FVector edge2 = V2 - V0;
	FVector pvec = RayDir.Cross(edge2);
	float det = edge1.Dot(pvec);

	if (std::abs(det) < 0.0001f) return false;

	float invDet = 1.0f / det;
	FVector tvec = RayOrigin - V0;
	float u = tvec.Dot(pvec) * invDet;

	if (u < 0.0f || u > 1.0f) return false;

	FVector qvec = tvec.Cross(edge1);
	float v = RayDir.Dot(qvec) * invDet;

	if (v < 0.0f || u + v > 1.0f) return false;

	OutT = edge2.Dot(qvec) * invDet;
	return OutT > 0.0f;
}

bool UPrimitiveComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!MeshData || MeshData->Indices.empty())
	{
		return false;
	}

	FMatrix invWorld = CachedWorldMatrix.GetInverse();
	FVector localOrigin = invWorld.TransformPositionWithW(Ray.Origin);
	FVector localDirection = invWorld.TransformVector(Ray.Direction);
	localDirection.Normalize();

	bool bHit = false;
	float closestT = FLT_MAX;

	for (size_t i = 0; i < MeshData->Indices.size(); i += 3)
	{
		FVector v0 = MeshData->Vertices[MeshData->Indices[i]].Position;
		FVector v1 = MeshData->Vertices[MeshData->Indices[i + 1]].Position;
		FVector v2 = MeshData->Vertices[MeshData->Indices[i + 2]].Position;

		float t = 0.0f;

		if (IntersectTriangle(localOrigin, localDirection, v0, v1, v2, t))
		{
			if (t > 0.0f && t < closestT)
			{
				closestT = t;
				bHit = true;
				OutHitResult.FaceIndex = static_cast<int>(i);
			}
		}
	}

	OutHitResult.bHit = bHit;

	if (bHit)
	{
		FVector localHitPoint = localOrigin + (localDirection * closestT);
		FVector worldHitPoint = CachedWorldMatrix.TransformPositionWithW(localHitPoint);
		OutHitResult.Distance = FVector::Distance(Ray.Origin, worldHitPoint);
		OutHitResult.HitComponent = this;
		return true;
	}

	return false;
}

void UPrimitiveComponent::UpdateWorldMatrix()
{
	USceneComponent::UpdateWorldMatrix();

	UpdateWorldAABB();
}


UCubeComponent::UCubeComponent()
{
	MeshData = &FMeshManager::GetCube();
}

bool UCubeComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) {
	if (!MeshData || !bIsVisible) {
		return false;
	}

	// Fetch vertexbuffer info here
	/*OutCommand.VertexBuffer = ...;
	OutCommand.VertexCount = ...;
	OutCommand.Stride = sizeof(vbuffer);*/

	return UPrimitiveComponent::GetRenderCommand(viewMatrix, projMatrix, OutCommand);
}

//bool UCubeComponent::Raycast(const FRay& Ray, float& OutDistance)
//{
//	FVector center = RelativeLocation;
//	FVector rotation = RelativeRotation;
//	FVector extents = RelativeScale3D / 2;
//
//	FMatrix rotationMatrix = FMatrix::MakeRotationEuler(RelativeRotation);
//
//	FVector Axes[3];
//	Axes[0] = FVector(rotationMatrix.M[0][0], rotationMatrix.M[0][1], rotationMatrix.M[0][2]);
//	Axes[1] = FVector(rotationMatrix.M[1][0], rotationMatrix.M[1][1], rotationMatrix.M[1][2]);
//	Axes[2] = FVector(rotationMatrix.M[2][0], rotationMatrix.M[2][1], rotationMatrix.M[2][2]);
//
//	float tMin = -INFINITY;
//	float tMax = INFINITY;
//	FVector delta = center - Ray.Origin;
//	float ext[3] = { extents.X, extents.Y, extents.Z };
//
//	for (int i = 0; i < 3; ++i)
//	{
//		float e = Axes[i].Dot(delta);
//		float f = Axes[i].Dot(Ray.Direction);
//
//		if (std::abs(f) > 0.00001f)
//		{
//			float t1 = (e + ext[i]) / f;
//			float t2 = (e - ext[i]) / f;
//
//			if (t1 > t2) std::swap(t1, t2);
//
//			if (t1 > tMin) tMin = t1;
//			if (t2 < tMax) tMax = t2;
//
//			if (tMin > tMax) return false;
//			if (tMax < 0.0f) return false;
//		}
//		else
//		{
//			if (-e - ext[i] > 0.0f || -e + ext[i] < 0.0f) return false;
//		}
//	}
//	return true;
//}


USphereComponent::USphereComponent()
{
	MeshData = &FMeshManager::GetSphere();
}
bool USphereComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) {
	if (!MeshData || !bIsVisible) {
		return false;
	}

	// Fetch vertexbuffer info here
	/*OutCommand.VertexBuffer = ...;
	OutCommand.VertexCount = ...;
	OutCommand.Stride = sizeof(vbuffer);*/

	return UPrimitiveComponent::GetRenderCommand(viewMatrix, projMatrix, OutCommand);
}

//bool USphereComponent::Raycast(const FRay& Ray, float& OutDistance)
//{
//	FMatrix invWorld = CachedWorldMatrix.GetInverse();
//
//	FVector localOrigin = invWorld.TransformVector(Ray.Origin);
//	FVector localDirection = invWorld.TransformVector(Ray.Direction);
//
//	float a = localDirection.Dot(localDirection);
//	float b = localOrigin.Dot(localDirection);
//	float c = localOrigin.Dot(localOrigin) - 1.0f;
//
//	float discriminant = (b * b) - (a * c);
//
//	if (discriminant < 0.0f)
//	{
//		return false;
//	}
//
//	float sqrtD = std::sqrt(discriminant);
//
//	float t0 = (-b - sqrtD) / a;
//	float t1 = (-b + sqrtD) / a;
//
//	if (t1 < 0.0f)
//	{
//		return false;
//	}
//
//	OutDistance = (t0 < 0.0f) ? t1 : t0;
//
//	return true;
//}

UPlaneComponent::UPlaneComponent()
{
	MeshData = &FMeshManager::GetPlane();
}
bool UPlaneComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) {
	if (!MeshData || !bIsVisible) {
		return false;
	}

	// Fetch vertexbuffer info here
	/*OutCommand.VertexBuffer = ...;
	OutCommand.VertexCount = ...;
	OutCommand.Stride = sizeof(vbuffer);*/

	return UPrimitiveComponent::GetRenderCommand(viewMatrix, projMatrix, OutCommand);
}

//bool UPlaneComponent::Raycast(const FRay& Ray, float& OutDistance)
//{
//	FMatrix invWorld = CachedWorldMatrix.GetInverse();
//
//	FVector localOrigin = invWorld.TransformVector(Ray.Origin);
//	FVector localDirection = invWorld.TransformVector(Ray.Direction);
//
//	float halfX = RelativeScale3D.X / 2.0f;
//	float halfZ = RelativeScale3D.Z / 2.0f;
//
//	FVector v0(-halfX, 0.0f, -halfZ);
//	FVector v1(-halfX, 0.0f, halfZ);
//	FVector v2(halfX, 0.0f, halfZ);
//	FVector v3(halfX, 0.0f, -halfZ);
//
//	auto intersectTriangle = [&](const FVector& V0, const FVector& V1, const FVector& V2, float& T) -> bool
//		{
//			FVector edge1 = V1 - V0;
//			FVector edge2 = V2 - V0;
//			FVector pvec = localDirection.Cross(edge2);
//			float det = edge1.Dot(pvec);
//
//			if (std::abs(det) < 0.0001f) return false;
//
//			float invDet = 1.0f / det;
//			FVector tvec = localOrigin - V0;
//			float u = tvec.Dot(pvec) * invDet;
//
//			if (u < 0.0f || u > 1.0f) return false;
//
//			FVector qvec = tvec.Cross(edge1);
//			float v = localDirection.Dot(qvec) * invDet;
//
//			if (v < 0.0f || u + v > 1.0f) return false;
//
//			T = edge2.Dot(qvec) * invDet;
//			return T > 0.0f;
//		};
//
//	float t1 = FLT_MAX, t2 = FLT_MAX;
//	bool hit1 = intersectTriangle(v0, v1, v2, t1);
//	bool hit2 = intersectTriangle(v0, v2, v3, t2);
//
//	if (hit1 || hit2)
//	{
//		OutDistance = std::min(t1, t2);
//		return true;
//	}
//	return false;
//}
