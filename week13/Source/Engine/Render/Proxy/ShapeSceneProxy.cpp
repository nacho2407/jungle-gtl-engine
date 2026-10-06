#include "Render/Proxy/ShapeSceneProxy.h"

#include "Component/ShapeComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Math/MathUtils.h"
#include "Render/Geometry/CollisionDebugGeometry.h"
#include "Object/Object.h"

FShapeSceneProxy::FShapeSceneProxy(UShapeComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags = EPrimitiveProxyFlags::EditorOnly
	           | EPrimitiveProxyFlags::NeverCull
	           | EPrimitiveProxyFlags::WireShape;

	bDrawOnlyIfSelected = InComponent->IsDrawOnlyIfSelected();
	WireColor = InComponent->GetShapeColorVec4();

	bCastShadow = false;
	bCastShadowAsTwoSided = false;

	RebuildLines();
}

void FShapeSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();
	RebuildLines();
}

void FShapeSceneProxy::UpdateVisibility()
{
	FPrimitiveSceneProxy::UpdateVisibility();

	if (bVisible && bDrawOnlyIfSelected)
	{
		bVisible = IsSelected();
	}
}

void FShapeSceneProxy::RebuildLines()
{
	CachedLines.clear();

	UPrimitiveComponent* OwnerComp = GetOwner();
	if (!IsValid(OwnerComp))
	{
		return;
	}

	const FQuat WorldRot = OwnerComp->GetWorldMatrix().ToQuat();

	if (const UBoxComponent* Box = Cast<UBoxComponent>(OwnerComp))
	{
		const FTransform WorldTM(Box->GetWorldLocation(), WorldRot, FVector::OneVector);
		FCollisionDebugGeometry::AddWireBox(
			CachedLines,
			WorldTM,
			Box->GetScaledBoxExtent());
	}
	else if (const USphereComponent* Sphere = Cast<USphereComponent>(OwnerComp))
	{
		FCollisionDebugGeometry::AddWireSphere(
			CachedLines,
			Sphere->GetWorldLocation(),
			Sphere->GetScaledSphereRadius());
	}
	else if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(OwnerComp))
	{
		const FTransform WorldTM(Capsule->GetWorldLocation(), WorldRot, FVector::OneVector);
		const float Radius = Capsule->GetScaledCapsuleRadius();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const float CylinderLength = FMath::Max(0.0f, HalfHeight * 2.0f - Radius * 2.0f);

		FCollisionDebugGeometry::AddWireCapsule(
			CachedLines,
			WorldTM,
			Radius,
			CylinderLength);
	}
}
