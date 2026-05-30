#pragma once

#include "Render/Geometry/DebugGeometryTypes.h"
#include "Engine/Math/Transform.h"
#include "Engine/PhysicsEngine/ConvexElem.h"

namespace FCollisionDebugGeometry
{
	void AddWireSphere(TArray<FWireLine>& OutLines, const FVector& Center, float Radius);
	void AddWireBox(TArray<FWireLine>& OutLines, const FTransform& WorldTM, const FVector& HalfExtent);
	void AddWireCapsule(TArray<FWireLine>& OutLines, const FTransform& WorldTM, float Radius, float Length);
	void AddWireConvex(TArray<FWireLine>& OutLines, const FKConvexElem& ConvexElem, const FTransform& WorldTM);

	void AddSolidSphere(FPhysicsDebugSolidMesh& OutMesh, const FTransform& WorldTM, float Radius, const FVector4& Color);
	void AddSolidBox(FPhysicsDebugSolidMesh& OutMesh, const FTransform& WorldTM, const FVector& HalfExtent, const FVector4& Color);
	void AddSolidCapsule(FPhysicsDebugSolidMesh& OutMesh, const FTransform& WorldTM, float Radius, float Length, const FVector4& Color);
	void AddSolidConvex(FPhysicsDebugSolidMesh& OutMesh, const FKConvexElem& ConvexElem, const FTransform& WorldTM, const FVector4& Color);
}
