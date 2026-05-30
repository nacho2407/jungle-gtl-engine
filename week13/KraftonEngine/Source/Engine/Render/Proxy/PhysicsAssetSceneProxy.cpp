#include "Render/Proxy/PhysicsAssetSceneProxy.h"

#include "Component/Debug/PhysicsAssetDebugComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
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
	const float ConstraintAxisScreenScale = 0.03f;
	const float MinConstraintAxisLength = 0.01f;

	float ComputeConstraintAxisLength(const FFrameContext& Frame, const FVector& ConstraintLocation)
	{
		const float AxisLength = Frame.bIsOrtho
			? Frame.OrthoWidth * ConstraintAxisScreenScale
			: FVector::Distance(Frame.View.GetInverseFast().GetLocation(), ConstraintLocation) * ConstraintAxisScreenScale;
		return AxisLength < MinConstraintAxisLength ? MinConstraintAxisLength : AxisLength;
	}

	uint64 MakeSolidMeshRevision(uint64 DebugRevision, uint64 SkinnedRevision)
	{
		uint64 Revision = 1469598103934665603ull;
		Revision = (Revision ^ DebugRevision) * 1099511628211ull;
		Revision = (Revision ^ SkinnedRevision) * 1099511628211ull;
		return Revision;
	}

	void AddConstraintAxisLine(
		TArray<FPhysicsDebugLine>& Lines,
		const FVector& Origin,
		const FVector& Axis,
		float Length,
		const FVector4& Color)
	{
		Lines.push_back(FPhysicsDebugLine{ Origin, Origin + Axis * Length, Color });
	}

	void AddConstraintFrameAxes(
		TArray<FPhysicsDebugLine>& Lines,
		const FTransform& Frame,
		const FVector& Origin,
		float Length,
		const FVector4& XColor,
		const FVector4& YColor,
		const FVector4& ZColor)
	{
		AddConstraintAxisLine(Lines, Origin, Frame.TransformVectorNoScale(FVector(1.0f, 0.0f, 0.0f)), Length, XColor);
		AddConstraintAxisLine(Lines, Origin, Frame.TransformVectorNoScale(FVector(0.0f, 1.0f, 0.0f)), Length, YColor);
		AddConstraintAxisLine(Lines, Origin, Frame.TransformVectorNoScale(FVector(0.0f, 0.0f, 1.0f)), Length, ZColor);
	}
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

	const USkeletalMeshComponent* TargetComponent = DebugComponent->GetTargetSkeletalMeshComponent();
	const uint64 DebugRevision = DebugComponent->GetPhysicsAssetDebugRevision();
	const uint64 SkinnedRevision = IsValid(TargetComponent) ? TargetComponent->GetSkinnedRevision() : 0;
	if (bCachedSolidMeshFresh &&
		CachedSolidDebugRevision == DebugRevision &&
		CachedSolidSkinnedRevision == SkinnedRevision)
	{
		OutMesh = CachedSolidMesh;
		return;
	}

	CachedSolidMesh.Reset();

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
			FCollisionDebugGeometry::AddSolidSphere(CachedSolidMesh, SphereWorldTM, SphereElem.Radius * UniformScale, SolidColor);
		}

		for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
		{
			FTransform ShapeLocalTM(BoxElem.Center * UniformScale, BoxElem.Rotation);
			const FTransform ShapeWorldTM = ShapeLocalTM * BoneWorldTM;
			const FVector HalfExtent(
				BoxElem.X * 0.5f * UniformScale,
				BoxElem.Y * 0.5f * UniformScale,
				BoxElem.Z * 0.5f * UniformScale);
			FCollisionDebugGeometry::AddSolidBox(CachedSolidMesh, ShapeWorldTM, HalfExtent, SolidColor);
		}

		for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
		{
			FTransform ShapeLocalTM(SphylElem.Center * UniformScale, SphylElem.Rotation);
			const FTransform ShapeWorldTM = ShapeLocalTM * BoneWorldTM;
			FCollisionDebugGeometry::AddSolidCapsule(
				CachedSolidMesh,
				ShapeWorldTM,
				SphylElem.Radius * UniformScale,
				SphylElem.Length * UniformScale,
				SolidColor);
		}

		for (const FKConvexElem& ConvexElem : AggGeom.ConvexElems)
		{
			FTransform ShapeWorldTM = ConvexElem.GetTransform() * BoneWorldTM;
			ShapeWorldTM.Scale = ShapeWorldTM.Scale * FVector(UniformScale, UniformScale, UniformScale);
			FCollisionDebugGeometry::AddSolidConvex(CachedSolidMesh, ConvexElem, ShapeWorldTM, SolidColor);
		}
	}

	CachedSolidMesh.Revision = MakeSolidMeshRevision(DebugRevision, SkinnedRevision);
	CachedSolidDebugRevision = DebugRevision;
	CachedSolidSkinnedRevision = SkinnedRevision;
	bCachedSolidMeshFresh = true;
	OutMesh = CachedSolidMesh;
}

void FPhysicsAssetSceneProxy::BuildPhysicsAssetConstraintAxisLines(
	const FFrameContext& Frame,
	TArray<FPhysicsDebugLine>& OutLines) const
{
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

	const FVector4 ConstraintXColor(1.0f, 0.12f, 0.12f, 1.0f);
	const FVector4 ConstraintYColor(0.12f, 1.0f, 0.12f, 1.0f);
	const FVector4 ConstraintZColor(0.18f, 0.45f, 1.0f, 1.0f);
	const FVector4 SelectedConstraintXColor(1.0f, 0.55f, 0.20f, 1.0f);
	const FVector4 SelectedConstraintYColor(0.55f, 1.0f, 0.35f, 1.0f);
	const FVector4 SelectedConstraintZColor(0.45f, 0.75f, 1.0f, 1.0f);

	const TArray<FConstraintInstanceInitDesc>& ConstraintDescs = PhysicsAsset->GetConstraintInitDescs();
	const int32 SelectedConstraintIndex = DebugComponent->GetSelectedConstraintIndex();
	for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(ConstraintDescs.size()); ++ConstraintIndex)
	{
		const FConstraintInstanceInitDesc& ConstraintDesc = ConstraintDescs[ConstraintIndex];
		FTransform ParentWorldFrame;
		FTransform ChildWorldFrame;
		if (!DebugComponent->GetConstraintWorldFrames(ConstraintDesc, ParentWorldFrame, ChildWorldFrame))
		{
			continue;
		}

		const FVector ConstraintLocation =
			(ParentWorldFrame.GetLocation() + ChildWorldFrame.GetLocation()) * 0.5f;
		const float ConstraintAxisLength = ComputeConstraintAxisLength(Frame, ConstraintLocation);
		const bool bSelectedConstraint = ConstraintIndex == SelectedConstraintIndex;

		AddConstraintFrameAxes(
			OutLines,
			ParentWorldFrame,
			ConstraintLocation,
			ConstraintAxisLength,
			bSelectedConstraint ? SelectedConstraintXColor : ConstraintXColor,
			bSelectedConstraint ? SelectedConstraintYColor : ConstraintYColor,
			bSelectedConstraint ? SelectedConstraintZColor : ConstraintZColor);
	}
}
