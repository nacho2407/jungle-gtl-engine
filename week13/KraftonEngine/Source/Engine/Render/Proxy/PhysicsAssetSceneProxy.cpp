#include "Render/Proxy/PhysicsAssetSceneProxy.h"

#include "Component/Debug/PhysicsAssetDebugComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
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

bool FPhysicsAssetSceneProxy::GetPhysicsAssetBoneWorldTransform(const FName& BoneName, FTransform& OutBoneWorldTM) const
{
	USkeletalMeshComponent* SMC = GetTargetSkeletalMeshComponent();
	USkeletalMesh* Mesh = IsValid(SMC) ? SMC->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!SMC || !Asset)
	{
		return false;
	}

	const FString BoneNameString = BoneName.ToString();
	if (BoneNameString.empty())
	{
		return false;
	}

	int32 BoneIndex = -1;
	for (int32 Index = 0; Index < static_cast<int32>(Asset->Bones.size()); ++Index)
	{
		if (Asset->Bones[Index].Name == BoneNameString)
		{
			BoneIndex = Index;
			break;
		}
	}

	if (BoneIndex < 0)
	{
		return false;
	}

	TArray<FTransform> ComponentSpaceBoneTransforms;
	SMC->GetCurrentBoneGlobalTransforms(ComponentSpaceBoneTransforms);
	if (BoneIndex >= static_cast<int32>(ComponentSpaceBoneTransforms.size()))
	{
		return false;
	}

	const FTransform ComponentToWorldTM(SMC->GetWorldMatrix());
	OutBoneWorldTM = ComponentSpaceBoneTransforms[BoneIndex] * ComponentToWorldTM;
	return true;
}

bool FPhysicsAssetSceneProxy::GetConstraintWorldFrames(
	const FConstraintInstanceInitDesc& ConstraintDesc,
	FTransform& OutParentFrame,
	FTransform& OutChildFrame) const
{
	FTransform ParentBoneWorldTM;
	FTransform ChildBoneWorldTM;
	if (!GetPhysicsAssetBoneWorldTransform(ConstraintDesc.ParentBoneName, ParentBoneWorldTM) ||
		!GetPhysicsAssetBoneWorldTransform(ConstraintDesc.ChildBoneName, ChildBoneWorldTM))
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
		if (!GetPhysicsAssetBoneWorldTransform(BodySetup->BoneName, BoneWorldTM))
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
