#include "PhysicsAssetBuilder.h"

#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"

#include <algorithm>
#include <cctype>

namespace
{
struct FBoneShapeFit
{
	bool bHasVertices = false;
	FVector Min = FVector::ZeroVector;
	FVector Max = FVector::ZeroVector;

	void AddPosition(const FVector& Position)
	{
		if (!bHasVertices)
		{
			Min = Position;
			Max = Position;
			bHasVertices = true;
			return;
		}

		Min.X = std::min(Min.X, Position.X);
		Min.Y = std::min(Min.Y, Position.Y);
		Min.Z = std::min(Min.Z, Position.Z);
		Max.X = std::max(Max.X, Position.X);
		Max.Y = std::max(Max.Y, Position.Y);
		Max.Z = std::max(Max.Z, Position.Z);
	}

	FVector GetCenter() const
	{
		return (Min + Max) * 0.5f;
	}

	FVector GetExtent() const
	{
		return (Max - Min) * 0.5f;
	}
};

int32 GetDominantBoneIndex(const FVertexPNCTBW& Vertex)
{
	int32 BestBoneIndex = -1;
	float BestWeight = 0.0f;

	for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
	{
		const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
		const float Weight = Vertex.BoneWeights[InfluenceIndex];
		if (BoneIndex >= 0 && Weight > BestWeight)
		{
			BestBoneIndex = BoneIndex;
			BestWeight = Weight;
		}
	}

	if (BestBoneIndex == -1 && Vertex.BoneIndices[0] >= 0)
	{
		BestBoneIndex = Vertex.BoneIndices[0];
	}

	return BestBoneIndex;
}

TArray<FBoneShapeFit> BuildBoneShapeFits(
	const FSkeletalMesh& MeshAsset,
	const FPhysicsAssetBuildOptions& Options)
{
	TArray<FBoneShapeFit> Fits;
	Fits.resize(MeshAsset.Bones.size());

	for (const FVertexPNCTBW& Vertex : MeshAsset.Vertices)
	{
		if (Options.bUseDominantBoneWeight)
		{
			const int32 BoneIndex = GetDominantBoneIndex(Vertex);
			if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(MeshAsset.Bones.size()))
			{
				const FVector LocalPosition = MeshAsset.Bones[BoneIndex].GetInverseBindPose().TransformPosition(Vertex.Position);
				Fits[BoneIndex].AddPosition(LocalPosition);
			}
			continue;
		}

		for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
		{
			const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
			if (BoneIndex >= 0 &&
				BoneIndex < static_cast<int32>(MeshAsset.Bones.size()) &&
				Vertex.BoneWeights[InfluenceIndex] > 0.0f)
			{
				const FVector LocalPosition = MeshAsset.Bones[BoneIndex].GetInverseBindPose().TransformPosition(Vertex.Position);
				Fits[BoneIndex].AddPosition(LocalPosition);
			}
		}
	}

	return Fits;
}

FString ToLowerString(const FString& Source)
{
	FString LowerName = Source;
	std::transform(
		LowerName.begin(),
		LowerName.end(),
		LowerName.begin(),
		[](unsigned char C) { return static_cast<char>(std::tolower(C)); });
	return LowerName;
}

FVector ClampExtent(const FVector& Extent, float MinPrimitiveSize)
{
	return FVector(
		std::max(Extent.X, MinPrimitiveSize),
		std::max(Extent.Y, MinPrimitiveSize),
		std::max(Extent.Z, MinPrimitiveSize));
}

float GetMaxComponent(const FVector& Value)
{
	return std::max(Value.X, std::max(Value.Y, Value.Z));
}

float GetBoneFitSize(const FBoneShapeFit& Fit)
{
	return Fit.bHasVertices ? Fit.GetExtent().Length() : 0.0f;
}
}

UPhysicsAsset* FPhysicsAssetBuilder::CreateFromSkeletalMesh(
	USkeletalMesh* SkeletalMesh,
	const FPhysicsAssetBuildOptions& Options)
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return nullptr;
	}

	UPhysicsAsset* PhysicsAsset = UObjectManager::Get().CreateObject<UPhysicsAsset>(SkeletalMesh);
	if (!PhysicsAsset)
	{
		return nullptr;
	}

	CreateBodies(PhysicsAsset, SkeletalMesh, Options);

	SkeletalMesh->SetPhysicsAsset(PhysicsAsset);
	return PhysicsAsset;
}

void FPhysicsAssetBuilder::CreateBodies(
	UPhysicsAsset* PhysicsAsset,
	USkeletalMesh* SkeletalMesh,
	const FPhysicsAssetBuildOptions& Options)
{
	if (!PhysicsAsset || !SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
	const TArray<FBoneShapeFit> BoneFits = BuildBoneShapeFits(*MeshAsset, Options);

	TArray<UBodySetup*>& BodySetups = PhysicsAsset->GetBodySetupsMutable();
	BodySetups.clear();

	int32 LargestFitBoneIndex = -1;
	float LargestFitBoneSize = -1.0f;

	auto CreateBodyForBone = [&](int32 BoneIndex) -> bool
	{
		const FBone& Bone = MeshAsset->Bones[BoneIndex];
		const FBoneShapeFit& Fit = BoneFits[BoneIndex];

		UBodySetup* BodySetup = UObjectManager::Get().CreateObject<UBodySetup>(PhysicsAsset);
		if (!BodySetup)
		{
			return false;
		}

		BodySetup->BoneName = FName(Bone.Name);
		AddDefaultShapeForBone(
			BodySetup,
			Bone.Name,
			Fit.GetCenter(),
			Fit.GetExtent(),
			true,
			Options);

		BodySetups.push_back(BodySetup);
		return true;
	};

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MeshAsset->Bones.size()); ++BoneIndex)
	{
		const FBone& Bone = MeshAsset->Bones[BoneIndex];
		if (Options.bSkipRootBody && Bone.ParentIndex == -1)
		{
			continue;
		}

		const FBoneShapeFit& Fit = BoneFits[BoneIndex];
		if (!Fit.bHasVertices)
		{
			continue;
		}

		const float BoneFitSize = GetBoneFitSize(Fit);
		if (BoneFitSize > LargestFitBoneSize)
		{
			LargestFitBoneIndex = BoneIndex;
			LargestFitBoneSize = BoneFitSize;
		}

		if (BoneFitSize < Options.MinBoneSize)
		{
			continue;
		}

		CreateBodyForBone(BoneIndex);
	}

	if (BodySetups.empty() && LargestFitBoneIndex != -1)
	{
		CreateBodyForBone(LargestFitBoneIndex);
	}
}

void FPhysicsAssetBuilder::AddDefaultShapeForBone(
	UBodySetup* BodySetup,
	const FString& BoneName,
	const FVector& FitCenter,
	const FVector& FitExtent,
	bool bHasFit,
	const FPhysicsAssetBuildOptions& Options)
{
	if (!BodySetup)
	{
		return;
	}

	const FString LowerName = ToLowerString(BoneName);
	const FVector SafeExtent = ClampExtent(FitExtent, Options.MinPrimitiveSize);

	if (LowerName.find("head") != FString::npos)
	{
		FKSphereElem Sphere;
		Sphere.Center = bHasFit ? FitCenter : FVector::ZeroVector;
		Sphere.Radius = bHasFit
			? std::max(GetMaxComponent(SafeExtent) * Options.FitPadding, Options.MinPrimitiveSize)
			: Options.DefaultBodyRadius;
		BodySetup->GetAggGeom().SphereElems.push_back(Sphere);
		return;
	}

	if (LowerName.find("root") != FString::npos ||
		LowerName.find("pelvis") != FString::npos ||
		LowerName.find("hip") != FString::npos)
	{
		FKBoxElem Box;
		Box.Center = bHasFit ? FitCenter : FVector::ZeroVector;
		Box.X = bHasFit ? std::max(SafeExtent.X * 2.0f * Options.FitPadding, Options.MinPrimitiveSize) : Options.DefaultBoxSize;
		Box.Y = bHasFit ? std::max(SafeExtent.Y * 2.0f * Options.FitPadding, Options.MinPrimitiveSize) : Options.DefaultBoxSize;
		Box.Z = bHasFit ? std::max(SafeExtent.Z * 2.0f * Options.FitPadding, Options.MinPrimitiveSize) : Options.DefaultBoxSize;
		BodySetup->GetAggGeom().BoxElems.push_back(Box);
		return;
	}

	FKSphylElem Sphyl;
	if (!bHasFit)
	{
		Sphyl.Radius = Options.DefaultBodyRadius;
		Sphyl.Length = Options.DefaultBodyLength;
		BodySetup->GetAggGeom().SphylElems.push_back(Sphyl);
		return;
	}

	Sphyl.Center = FitCenter;
	float LongExtent = SafeExtent.Z;
	float Radius = std::max(SafeExtent.X, SafeExtent.Y);

	if (SafeExtent.X > SafeExtent.Y && SafeExtent.X > SafeExtent.Z)
	{
		Sphyl.Rotation = FRotator(90.0f, 0.0f, 0.0f);
		LongExtent = SafeExtent.X;
		Radius = std::max(SafeExtent.Y, SafeExtent.Z);
	}
	else if (SafeExtent.Y > SafeExtent.X && SafeExtent.Y > SafeExtent.Z)
	{
		Sphyl.Rotation = FRotator(0.0f, 0.0f, -90.0f);
		LongExtent = SafeExtent.Y;
		Radius = std::max(SafeExtent.X, SafeExtent.Z);
	}

	Sphyl.Radius = std::max(Radius * Options.FitPadding, Options.MinPrimitiveSize);
	Sphyl.Length = std::max((LongExtent - Sphyl.Radius) * 2.0f * Options.FitPadding, Options.MinPrimitiveSize);
	BodySetup->GetAggGeom().SphylElems.push_back(Sphyl);
}
