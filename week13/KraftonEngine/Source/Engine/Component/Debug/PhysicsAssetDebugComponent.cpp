#include "PhysicsAssetDebugComponent.h"

#include "Collision/Ray/RayUtils.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/GarbageCollection.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Physics/ConstraintInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Render/Proxy/PhysicsAssetSceneProxy.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

HIDE_FROM_COMPONENT_LIST(UPhysicsAssetDebugComponent)

namespace
{
	constexpr float ConstraintPickScreenScale = 0.006f;
	constexpr float MinConstraintPickRadius = 0.01f;

	struct FLocalRay
	{
		FVector Origin = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
	};

	FRay MakeNormalizedRay(const FRay& Ray)
	{
		FRay NormalizedRay = Ray;
		NormalizedRay.Direction = Ray.Direction.GetSafeNormal();
		return NormalizedRay;
	}

	FLocalRay MakeLocalRay(const FRay& WorldRay, const FTransform& LocalToWorld)
	{
		const FMatrix WorldToLocal = LocalToWorld.ToMatrix().GetAffineInverse();
		FLocalRay LocalRay;
		LocalRay.Origin = WorldToLocal.TransformPositionWithW(WorldRay.Origin);
		LocalRay.Direction = WorldToLocal.TransformVector(WorldRay.Direction).GetSafeNormal();
		return LocalRay;
	}

	bool ShouldUseHit(float Distance, const FPhysicsAssetDebugHitResult& CurrentBest)
	{
		return Distance >= 0.0f && Distance < CurrentBest.Distance;
	}

	float ComputeConstraintPickRadius(
		const FVector& CameraLocation,
		bool bIsOrtho,
		float OrthoWidth,
		const FVector& ConstraintLocation)
	{
		const float Radius = bIsOrtho
			? OrthoWidth * ConstraintPickScreenScale
			: FVector::Distance(CameraLocation, ConstraintLocation) * ConstraintPickScreenScale;
		return std::max(Radius, MinConstraintPickRadius);
	}

	void SetBestHit(
		const FRay& Ray,
		FPhysicsAssetDebugHitResult& OutHit,
		int32 BodyIndex,
		int32 ShapeIndex,
		EAggCollisionShape ShapeType,
		float Distance,
		const FVector& WorldNormal)
	{
		OutHit.Type = EPhysicsAssetDebugHitType::Body;
		OutHit.ShapeType = ShapeType;
		OutHit.BodyIndex = BodyIndex;
		OutHit.ConstraintIndex = -1;
		OutHit.ShapeIndex = ShapeIndex;
		OutHit.Distance = Distance;
		OutHit.WorldHitLocation = Ray.Origin + Ray.Direction * Distance;
		OutHit.WorldNormal = WorldNormal.GetSafeNormal();
	}

	bool IntersectSphere(
		const FRay& Ray,
		const FVector& Center,
		float Radius,
		float& OutDistance,
		FVector& OutNormal)
	{
		if (Radius <= 0.0f)
		{
			return false;
		}

		const FVector M = Ray.Origin - Center;
		const float B = M.Dot(Ray.Direction);
		const float C = M.Dot(M) - Radius * Radius;
		const float Discriminant = B * B - C;
		if (Discriminant < 0.0f)
		{
			return false;
		}

		const float SqrtDiscriminant = std::sqrt(Discriminant);
		float Distance = -B - SqrtDiscriminant;
		if (Distance < 0.0f)
		{
			Distance = -B + SqrtDiscriminant;
		}
		if (Distance < 0.0f)
		{
			return false;
		}

		const FVector HitLocation = Ray.Origin + Ray.Direction * Distance;
		OutDistance = Distance;
		OutNormal = (HitLocation - Center).GetSafeNormal();
		return true;
	}

	bool IntersectBox(
		const FRay& Ray,
		const FTransform& BoxWorldTM,
		const FVector& HalfExtent,
		float& OutDistance,
		FVector& OutNormal)
	{
		if (HalfExtent.X <= 0.0f || HalfExtent.Y <= 0.0f || HalfExtent.Z <= 0.0f)
		{
			return false;
		}

		const FLocalRay LocalRay = MakeLocalRay(Ray, BoxWorldTM);
		FRay LocalRayForAABB { LocalRay.Origin, LocalRay.Direction };

		float TMin = 0.0f;
		float TMax = 0.0f;
		if (!FRayUtils::IntersectRayAABB(LocalRayForAABB, -HalfExtent, HalfExtent, TMin, TMax))
		{
			return false;
		}

		const float LocalDistance = TMin >= 0.0f ? TMin : TMax;
		if (LocalDistance < 0.0f)
		{
			return false;
		}

		const FVector LocalHit = LocalRay.Origin + LocalRay.Direction * LocalDistance;
		FVector LocalNormal = FVector::ZeroVector;
		const float DX = std::abs(std::abs(LocalHit.X) - HalfExtent.X);
		const float DY = std::abs(std::abs(LocalHit.Y) - HalfExtent.Y);
		const float DZ = std::abs(std::abs(LocalHit.Z) - HalfExtent.Z);
		if (DX <= DY && DX <= DZ)
		{
			LocalNormal = FVector(LocalHit.X >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
		}
		else if (DY <= DX && DY <= DZ)
		{
			LocalNormal = FVector(0.0f, LocalHit.Y >= 0.0f ? 1.0f : -1.0f, 0.0f);
		}
		else
		{
			LocalNormal = FVector(0.0f, 0.0f, LocalHit.Z >= 0.0f ? 1.0f : -1.0f);
		}

		const FVector WorldHit = BoxWorldTM.TransformPosition(LocalHit);
		OutDistance = FVector::Distance(Ray.Origin, WorldHit);
		OutNormal = BoxWorldTM.TransformVectorNoScale(LocalNormal).GetSafeNormal();
		return true;
	}

	bool IntersectLocalSphere(
		const FLocalRay& LocalRay,
		const FVector& Center,
		float Radius,
		float& InOutBestLocalDistance,
		FVector& OutLocalNormal)
	{
		FRay SphereRay { LocalRay.Origin, LocalRay.Direction };
		float Distance = 0.0f;
		FVector Normal = FVector::ZeroVector;
		if (IntersectSphere(SphereRay, Center, Radius, Distance, Normal) && Distance < InOutBestLocalDistance)
		{
			InOutBestLocalDistance = Distance;
			OutLocalNormal = Normal;
			return true;
		}

		return false;
	}

	bool IntersectCapsule(
		const FRay& Ray,
		const FTransform& CapsuleWorldTM,
		float Radius,
		float Length,
		float& OutDistance,
		FVector& OutNormal)
	{
		if (Radius <= 0.0f)
		{
			return false;
		}

		const FLocalRay LocalRay = MakeLocalRay(Ray, CapsuleWorldTM);
		float BestLocalDistance = FLT_MAX;
		FVector BestLocalNormal = FVector::ZeroVector;
		const float HalfLength = std::max(0.0f, Length * 0.5f);

		const float A = LocalRay.Direction.X * LocalRay.Direction.X + LocalRay.Direction.Y * LocalRay.Direction.Y;
		if (A > 1.0e-6f)
		{
			const float B = 2.0f * (LocalRay.Origin.X * LocalRay.Direction.X + LocalRay.Origin.Y * LocalRay.Direction.Y);
			const float C = LocalRay.Origin.X * LocalRay.Origin.X + LocalRay.Origin.Y * LocalRay.Origin.Y - Radius * Radius;
			const float Discriminant = B * B - 4.0f * A * C;
			if (Discriminant >= 0.0f)
			{
				const float SqrtDiscriminant = std::sqrt(Discriminant);
				const float InvDenom = 1.0f / (2.0f * A);
				const float Candidates[2] =
				{
					(-B - SqrtDiscriminant) * InvDenom,
					(-B + SqrtDiscriminant) * InvDenom
				};

				for (float Candidate : Candidates)
				{
					const float Z = LocalRay.Origin.Z + LocalRay.Direction.Z * Candidate;
					if (Candidate >= 0.0f && Candidate < BestLocalDistance && Z >= -HalfLength && Z <= HalfLength)
					{
						BestLocalDistance = Candidate;
						BestLocalNormal = FVector(
							LocalRay.Origin.X + LocalRay.Direction.X * Candidate,
							LocalRay.Origin.Y + LocalRay.Direction.Y * Candidate,
							0.0f).GetSafeNormal();
					}
				}
			}
		}

		IntersectLocalSphere(LocalRay, FVector(0.0f, 0.0f, HalfLength), Radius, BestLocalDistance, BestLocalNormal);
		IntersectLocalSphere(LocalRay, FVector(0.0f, 0.0f, -HalfLength), Radius, BestLocalDistance, BestLocalNormal);

		if (BestLocalDistance == FLT_MAX)
		{
			return false;
		}

		const FVector LocalHit = LocalRay.Origin + LocalRay.Direction * BestLocalDistance;
		const FVector WorldHit = CapsuleWorldTM.TransformPosition(LocalHit);
		OutDistance = FVector::Distance(Ray.Origin, WorldHit);
		OutNormal = CapsuleWorldTM.TransformVectorNoScale(BestLocalNormal).GetSafeNormal();
		return true;
	}

	bool IntersectConvex(
		const FRay& Ray,
		const FKConvexElem& ConvexElem,
		const FTransform& ConvexWorldTM,
		float& OutDistance,
		FVector& OutNormal)
	{
		bool bHit = false;
		float BestDistance = FLT_MAX;
		FVector BestNormal = FVector::ZeroVector;

		for (std::size_t Index = 0; Index + 2 < ConvexElem.IndexData.size(); Index += 3)
		{
			const int32 I0 = ConvexElem.IndexData[Index];
			const int32 I1 = ConvexElem.IndexData[Index + 1];
			const int32 I2 = ConvexElem.IndexData[Index + 2];
			if (I0 < 0 || I1 < 0 || I2 < 0 ||
				static_cast<std::size_t>(I0) >= ConvexElem.VertexData.size() ||
				static_cast<std::size_t>(I1) >= ConvexElem.VertexData.size() ||
				static_cast<std::size_t>(I2) >= ConvexElem.VertexData.size())
			{
				continue;
			}

			const FVector V0 = ConvexWorldTM.TransformPosition(ConvexElem.VertexData[I0]);
			const FVector V1 = ConvexWorldTM.TransformPosition(ConvexElem.VertexData[I1]);
			const FVector V2 = ConvexWorldTM.TransformPosition(ConvexElem.VertexData[I2]);

			float Distance = 0.0f;
			if (FRayUtils::IntersectTriangle(Ray.Origin, Ray.Direction, V0, V1, V2, Distance) &&
				Distance < BestDistance)
			{
				bHit = true;
				BestDistance = Distance;
				BestNormal = (V1 - V0).Cross(V2 - V0).GetSafeNormal();
			}
		}

		if (!bHit)
		{
			return false;
		}

		if (BestNormal.Dot(Ray.Direction) > 0.0f)
		{
			BestNormal = -BestNormal;
		}

		OutDistance = BestDistance;
		OutNormal = BestNormal;
		return true;
	}
}

UPhysicsAssetDebugComponent::UPhysicsAssetDebugComponent()
{
}

UPhysicsAssetDebugComponent::~UPhysicsAssetDebugComponent()
{
}

FPrimitiveSceneProxy* UPhysicsAssetDebugComponent::CreateSceneProxy()
{
	return new FPhysicsAssetSceneProxy(this);
}

bool UPhysicsAssetDebugComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	FPhysicsAssetDebugHitResult PhysicsHit;
	if (!PickBody(Ray, PhysicsHit))
	{
		return false;
	}

	OutHitResult = {};
	OutHitResult.bHit = true;
	OutHitResult.HitComponent = this;
	OutHitResult.Distance = PhysicsHit.Distance;
	OutHitResult.WorldHitLocation = PhysicsHit.WorldHitLocation;
	OutHitResult.WorldNormal = PhysicsHit.WorldNormal;
	OutHitResult.ImpactNormal = PhysicsHit.WorldNormal;
	OutHitResult.FaceIndex = PhysicsHit.BodyIndex;
	return true;
}

void UPhysicsAssetDebugComponent::SetTarget(
	USkeletalMeshComponent* InTargetSkeletalMeshComponent,
	UPhysicsAsset* InPhysicsAsset)
{
	if (TargetSkeletalMeshComponent.Get() == InTargetSkeletalMeshComponent &&
		PhysicsAsset.Get() == InPhysicsAsset)
	{
		return;
	}

	TargetSkeletalMeshComponent = InTargetSkeletalMeshComponent;
	PhysicsAsset = InPhysicsAsset;
	MarkPhysicsAssetDebugDirty();
}

void UPhysicsAssetDebugComponent::MarkPhysicsAssetDebugDirty()
{
	++PhysicsAssetDebugRevision;
}

bool UPhysicsAssetDebugComponent::GetPhysicsAssetBoneWorldTransform(const FName& BoneName, FTransform& OutBoneWorldTM) const
{
	USkeletalMeshComponent* SMC = TargetSkeletalMeshComponent.Get();
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

bool UPhysicsAssetDebugComponent::GetConstraintWorldFrames(
	const FConstraintInstanceInitDesc& ConstraintDesc,
	FTransform& OutParentFrame,
	FTransform& OutChildFrame) const
{
	FTransform ParentBoneWorldTM;
	FTransform ChildBoneWorldTM;
	if (!GetPhysicsAssetBoneWorldTransform(ConstraintDesc.ChildBoneName, ChildBoneWorldTM))
	{
		return false;
	}

	if (ConstraintDesc.ParentBoneName == FName::None ||
		!GetPhysicsAssetBoneWorldTransform(ConstraintDesc.ParentBoneName, ParentBoneWorldTM))
	{
		ParentBoneWorldTM = ChildBoneWorldTM;
	}

	OutParentFrame = ConstraintDesc.ParentFrame * ParentBoneWorldTM;
	OutChildFrame = ConstraintDesc.ChildFrame * ChildBoneWorldTM;
	return true;
}

bool UPhysicsAssetDebugComponent::GetConstraintWorldLocation(
	const FConstraintInstanceInitDesc& ConstraintDesc,
	FVector& OutWorldLocation) const
{
	FTransform ParentWorldFrame;
	FTransform ChildWorldFrame;
	if (!GetConstraintWorldFrames(ConstraintDesc, ParentWorldFrame, ChildWorldFrame))
	{
		return false;
	}

	OutWorldLocation = (ParentWorldFrame.GetLocation() + ChildWorldFrame.GetLocation()) * 0.5f;
	return true;
}

bool UPhysicsAssetDebugComponent::SetConstraintWorldLocation(
	FConstraintInstanceInitDesc& ConstraintDesc,
	const FVector& WorldLocation)
{
	FTransform ParentBoneWorldTM;
	FTransform ChildBoneWorldTM;
	if (!GetPhysicsAssetBoneWorldTransform(ConstraintDesc.ChildBoneName, ChildBoneWorldTM))
	{
		return false;
	}

	if (ConstraintDesc.ParentBoneName == FName::None ||
		!GetPhysicsAssetBoneWorldTransform(ConstraintDesc.ParentBoneName, ParentBoneWorldTM))
	{
		ParentBoneWorldTM = ChildBoneWorldTM;
	}

	const FMatrix ParentWorldToLocal = ParentBoneWorldTM.ToMatrix().GetAffineInverse();
	const FMatrix ChildWorldToLocal = ChildBoneWorldTM.ToMatrix().GetAffineInverse();
	ConstraintDesc.ParentFrame.Location = ParentWorldToLocal.TransformPosition(WorldLocation);
	ConstraintDesc.ChildFrame.Location = ChildWorldToLocal.TransformPosition(WorldLocation);

	MarkPhysicsAssetDebugDirty();
	return true;
}

bool UPhysicsAssetDebugComponent::SyncConstraintFrameLocation(
	FConstraintInstanceInitDesc& ConstraintDesc,
	EPhysicsAssetConstraintFrameSide SourceFrameSide)
{
	FTransform ParentWorldFrame;
	FTransform ChildWorldFrame;
	if (!GetConstraintWorldFrames(ConstraintDesc, ParentWorldFrame, ChildWorldFrame))
	{
		return false;
	}

	const FTransform SourceWorldFrame =
		(SourceFrameSide == EPhysicsAssetConstraintFrameSide::Parent)
			? ParentWorldFrame
			: ChildWorldFrame;
	return SetConstraintWorldLocation(ConstraintDesc, SourceWorldFrame.GetLocation());
}

bool UPhysicsAssetDebugComponent::PickBody(const FRay& Ray, FPhysicsAssetDebugHitResult& OutHit) const
{
	OutHit = {};

	const UPhysicsAsset* CurrentPhysicsAsset = PhysicsAsset.Get();
	if (!CurrentPhysicsAsset || !IsValid(TargetSkeletalMeshComponent.Get()))
	{
		return false;
	}

	const FRay NormalizedRay = MakeNormalizedRay(Ray);
	if (NormalizedRay.Direction.IsNearlyZero())
	{
		return false;
	}

	const TArray<UBodySetup*>& BodySetups = CurrentPhysicsAsset->GetBodySetups();
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

		const FVector BoneScale3D = BoneWorldTM.Scale;
		const float UniformScale = BoneScale3D.GetAbsMax();
		BoneWorldTM.Scale = FVector::OneVector;

		const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(AggGeom.SphereElems.size()); ++ShapeIndex)
		{
			const FKSphereElem& SphereElem = AggGeom.SphereElems[ShapeIndex];
			const FVector WorldCenter = BoneWorldTM.TransformPosition(SphereElem.Center * UniformScale);
			float Distance = 0.0f;
			FVector Normal = FVector::ZeroVector;
			if (IntersectSphere(NormalizedRay, WorldCenter, SphereElem.Radius * UniformScale, Distance, Normal) &&
				ShouldUseHit(Distance, OutHit))
			{
				SetBestHit(NormalizedRay, OutHit, BodyIndex, ShapeIndex, EAggCollisionShape::Sphere, Distance, Normal);
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(AggGeom.BoxElems.size()); ++ShapeIndex)
		{
			const FKBoxElem& BoxElem = AggGeom.BoxElems[ShapeIndex];
			FTransform ShapeLocalTM(BoxElem.Center * UniformScale, BoxElem.Rotation);
			const FTransform ShapeWorldTM = ShapeLocalTM * BoneWorldTM;
			const FVector HalfExtent(
				BoxElem.X * 0.5f * UniformScale,
				BoxElem.Y * 0.5f * UniformScale,
				BoxElem.Z * 0.5f * UniformScale);
			float Distance = 0.0f;
			FVector Normal = FVector::ZeroVector;
			if (IntersectBox(NormalizedRay, ShapeWorldTM, HalfExtent, Distance, Normal) &&
				ShouldUseHit(Distance, OutHit))
			{
				SetBestHit(NormalizedRay, OutHit, BodyIndex, ShapeIndex, EAggCollisionShape::Box, Distance, Normal);
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(AggGeom.SphylElems.size()); ++ShapeIndex)
		{
			const FKSphylElem& SphylElem = AggGeom.SphylElems[ShapeIndex];
			FTransform ShapeLocalTM(SphylElem.Center * UniformScale, SphylElem.Rotation);
			const FTransform ShapeWorldTM = ShapeLocalTM * BoneWorldTM;
			float Distance = 0.0f;
			FVector Normal = FVector::ZeroVector;
			if (IntersectCapsule(
					NormalizedRay,
					ShapeWorldTM,
					SphylElem.Radius * UniformScale,
					SphylElem.Length * UniformScale,
					Distance,
					Normal) &&
				ShouldUseHit(Distance, OutHit))
			{
				SetBestHit(NormalizedRay, OutHit, BodyIndex, ShapeIndex, EAggCollisionShape::Sphyl, Distance, Normal);
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(AggGeom.ConvexElems.size()); ++ShapeIndex)
		{
			const FKConvexElem& ConvexElem = AggGeom.ConvexElems[ShapeIndex];
			FTransform ShapeWorldTM = ConvexElem.GetTransform() * BoneWorldTM;
			ShapeWorldTM.Scale = ShapeWorldTM.Scale * FVector(UniformScale, UniformScale, UniformScale);
			float Distance = 0.0f;
			FVector Normal = FVector::ZeroVector;
			if (IntersectConvex(NormalizedRay, ConvexElem, ShapeWorldTM, Distance, Normal) &&
				ShouldUseHit(Distance, OutHit))
			{
				SetBestHit(NormalizedRay, OutHit, BodyIndex, ShapeIndex, EAggCollisionShape::Convex, Distance, Normal);
			}
		}
	}

	return OutHit.Type == EPhysicsAssetDebugHitType::Body;
}

bool UPhysicsAssetDebugComponent::PickConstraint(
	const FRay& Ray,
	const FVector& CameraLocation,
	bool bIsOrtho,
	float OrthoWidth,
	FPhysicsAssetDebugHitResult& OutHit) const
{
	OutHit = {};

	const UPhysicsAsset* CurrentPhysicsAsset = PhysicsAsset.Get();
	if (!CurrentPhysicsAsset || !IsValid(TargetSkeletalMeshComponent.Get()))
	{
		return false;
	}

	const FRay NormalizedRay = MakeNormalizedRay(Ray);
	if (NormalizedRay.Direction.IsNearlyZero())
	{
		return false;
	}

	const TArray<FConstraintInstanceInitDesc>& ConstraintDescs = CurrentPhysicsAsset->GetConstraintInitDescs();
	for (int32 ConstraintIndex = 0; ConstraintIndex < static_cast<int32>(ConstraintDescs.size()); ++ConstraintIndex)
	{
		const FConstraintInstanceInitDesc& ConstraintDesc = ConstraintDescs[ConstraintIndex];
		FVector ConstraintLocation = FVector::ZeroVector;
		if (!GetConstraintWorldLocation(ConstraintDesc, ConstraintLocation))
		{
			continue;
		}

		const float PickRadius = ComputeConstraintPickRadius(
			CameraLocation,
			bIsOrtho,
			OrthoWidth,
			ConstraintLocation);
		float Distance = 0.0f;
		FVector Normal = FVector::ZeroVector;
		if (!IntersectSphere(NormalizedRay, ConstraintLocation, PickRadius, Distance, Normal) ||
			!ShouldUseHit(Distance, OutHit))
		{
			continue;
		}

		OutHit.Type = EPhysicsAssetDebugHitType::Constraint;
		OutHit.ShapeType = EAggCollisionShape::Unknown;
		OutHit.BodyIndex = CurrentPhysicsAsset->FindBodyIndexByBoneName(ConstraintDesc.ChildBoneName);
		OutHit.ConstraintIndex = ConstraintIndex;
		OutHit.ShapeIndex = -1;
		OutHit.Distance = Distance;
		OutHit.WorldHitLocation = NormalizedRay.Origin + NormalizedRay.Direction * Distance;
		OutHit.WorldNormal = Normal.GetSafeNormal();
	}

	return OutHit.Type == EPhysicsAssetDebugHitType::Constraint;
}

void UPhysicsAssetDebugComponent::SetSelectedBodyIndex(int32 InSelectedBodyIndex)
{
	if (SelectedBodyIndex == InSelectedBodyIndex)
	{
		return;
	}

	SelectedBodyIndex = InSelectedBodyIndex;
	MarkPhysicsAssetDebugDirty();
}

void UPhysicsAssetDebugComponent::SetSelectedConstraintIndex(int32 InSelectedConstraintIndex)
{
	if (SelectedConstraintIndex == InSelectedConstraintIndex)
	{
		return;
	}

	SelectedConstraintIndex = InSelectedConstraintIndex;
	MarkPhysicsAssetDebugDirty();
}

void UPhysicsAssetDebugComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
	UPrimitiveComponent::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(TargetSkeletalMeshComponent.Get(), "UPhysicsAssetDebugComponent.TargetSkeletalMeshComponent");
	Collector.AddReferencedObject(PhysicsAsset.Get(), "UPhysicsAssetDebugComponent.PhysicsAsset");
}
