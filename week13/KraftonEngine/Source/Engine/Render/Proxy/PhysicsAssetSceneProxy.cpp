#include "Render/Proxy/PhysicsAssetSceneProxy.h"

#include "Component/Debug/PhysicsAssetDebugComponent.h"
#include "Object/Object.h"
#include "Physics/ConstraintInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Render/Geometry/CollisionDebugGeometry.h"
#include "Render/Types/FrameContext.h"

namespace
{
	const FVector4 DefaultBodyColor(0.1f, 0.7f, 1.0f, 0.22f);
	const FVector4 SelectedBodyColor(0.0f, 0.95f, 1.0f, 0.48f);
}

FPhysicsAssetSceneProxy::FPhysicsAssetSceneProxy(UPhysicsAssetDebugComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags = EPrimitiveProxyFlags::EditorOnly
		| EPrimitiveProxyFlags::NeverCull
		| EPrimitiveProxyFlags::PhysicsAssetDebug;
}

UPhysicsAssetDebugComponent* FPhysicsAssetSceneProxy::GetPhysicsAssetDebugComponent() const
{
	return Cast<UPhysicsAssetDebugComponent>(GetOwner());
}

USkeletalMeshComponent* FPhysicsAssetSceneProxy::GetTargetSkeletalMeshComponent() const
{
	const UPhysicsAssetDebugComponent* DebugComponent = GetPhysicsAssetDebugComponent();
	return IsValid(DebugComponent) ? DebugComponent->GetTargetSkeletalMeshComponent() : nullptr;
}

bool FPhysicsAssetSceneProxy::GetConstraintWorldFrames(
	const FConstraintInstanceInitDesc& ConstraintDesc,
	FTransform& OutParentFrame,
	FTransform& OutChildFrame) const
{
	const UPhysicsAssetDebugComponent* DebugComponent = GetPhysicsAssetDebugComponent();
	if (!IsValid(DebugComponent))
	{
		return false;
	}

	FTransform ParentBoneWorldTM;
	FTransform ChildBoneWorldTM;
	if (!DebugComponent->GetPhysicsAssetBoneWorldTransform(ConstraintDesc.ParentBoneName, ParentBoneWorldTM) ||
		!DebugComponent->GetPhysicsAssetBoneWorldTransform(ConstraintDesc.ChildBoneName, ChildBoneWorldTM))
	{
		return false;
	}

	OutParentFrame = ConstraintDesc.ParentFrame * ParentBoneWorldTM;
	OutChildFrame = ConstraintDesc.ChildFrame * ChildBoneWorldTM;
	return true;
}

void FPhysicsAssetSceneProxy::BuildPhysicsAssetSolidMesh(
	const FFrameContext& Frame,
	FPhysicsDebugSolidMesh& OutMesh) const
{
	OutMesh.Reset();

	if (!Frame.RenderOptions.ShowFlags.bDebugPhysicsAsset)
	{
		return;
	}

	const UPhysicsAssetDebugComponent* DebugComponent = GetPhysicsAssetDebugComponent();
	const UPhysicsAsset* PhysicsAsset = IsValid(DebugComponent) ? DebugComponent->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		return;
	}

	const int32 SelectedBodyIndex = DebugComponent->GetSelectedBodyIndex();
	const TArray<UBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();
	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(BodySetups.size()); ++BodyIndex)
	{
		const UBodySetup* BodySetup = BodySetups[BodyIndex];
		if (!BodySetup)
		{
			continue;
		}

		FTransform BoneWorldTM;
		if (!DebugComponent->GetPhysicsAssetBoneWorldTransform(BodySetup->BoneName, BoneWorldTM))
		{
			continue;
		}

		const FVector4& SolidColor = (BodyIndex == SelectedBodyIndex)
			? SelectedBodyColor
			: DefaultBodyColor;

		const FVector BoneScale3D = BoneWorldTM.Scale;
		const float UniformScale = BoneScale3D.GetAbsMax();
		BoneWorldTM.Scale = FVector::OneVector;

		const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
		for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
		{
			const FVector WorldCenter = BoneWorldTM.TransformPosition(SphereElem.Center * UniformScale);
			const FTransform SphereWorldTM(WorldCenter, BoneWorldTM.Rotation, FVector::OneVector);
			FCollisionDebugGeometry::AddSolidSphere(OutMesh, SphereWorldTM, SphereElem.Radius * UniformScale, SolidColor);
		}

		for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
		{
			FTransform ShapeLocalTM(BoxElem.Center * UniformScale, BoxElem.Rotation);
			const FTransform ShapeWorldTM = ShapeLocalTM * BoneWorldTM;
			const FVector HalfExtent(
				BoxElem.X * 0.5f * UniformScale,
				BoxElem.Y * 0.5f * UniformScale,
				BoxElem.Z * 0.5f * UniformScale);
			FCollisionDebugGeometry::AddSolidBox(OutMesh, ShapeWorldTM, HalfExtent, SolidColor);
		}

		for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
		{
			FTransform ShapeLocalTM(SphylElem.Center * UniformScale, SphylElem.Rotation);
			const FTransform ShapeWorldTM = ShapeLocalTM * BoneWorldTM;
			FCollisionDebugGeometry::AddSolidCapsule(
				OutMesh,
				ShapeWorldTM,
				SphylElem.Radius * UniformScale,
				SphylElem.Length * UniformScale,
				SolidColor);
		}

		for (const FKConvexElem& ConvexElem : AggGeom.ConvexElems)
		{
			FTransform ShapeWorldTM = ConvexElem.GetTransform() * BoneWorldTM;
			ShapeWorldTM.Scale = ShapeWorldTM.Scale * FVector(UniformScale, UniformScale, UniformScale);
			FCollisionDebugGeometry::AddSolidConvex(OutMesh, ConvexElem, ShapeWorldTM, SolidColor);
		}
	}
}
