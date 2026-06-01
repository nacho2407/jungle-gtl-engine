#include "PhysicsAssetBuilder.h"

#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Math/Quat.h"
#include "Object/Object.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"

#include <algorithm>
#include <cmath>

namespace
{
struct FBoneShapeFit
{
	bool bHasVertices = false;
	FVector Min = FVector::ZeroVector;
	FVector Max = FVector::ZeroVector;
	TArray<FVector> Positions;

	void AddPosition(const FVector& Position)
	{
		Positions.push_back(Position);

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

struct FShapeFitFrame
{
	FTransform ElementTransform;
	FVector BoxExtent = FVector::ZeroVector;
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

FMatrix ComputeCovarianceMatrix(const TArray<FVector>& Positions)
{
	if (Positions.empty())
	{
		return FMatrix::Identity;
	}

	FVector Mean = FVector::ZeroVector;
	for (const FVector& Position : Positions)
	{
		Mean += Position;
	}
	Mean /= static_cast<float>(Positions.size());

	FMatrix Covariance;
	for (const FVector& Position : Positions)
	{
		const FVector Error = Position - Mean;
		Covariance.M[0][0] += Error.X * Error.X;
		Covariance.M[0][1] += Error.X * Error.Y;
		Covariance.M[0][2] += Error.X * Error.Z;
		Covariance.M[1][0] += Error.Y * Error.X;
		Covariance.M[1][1] += Error.Y * Error.Y;
		Covariance.M[1][2] += Error.Y * Error.Z;
		Covariance.M[2][0] += Error.Z * Error.X;
		Covariance.M[2][1] += Error.Z * Error.Y;
		Covariance.M[2][2] += Error.Z * Error.Z;
	}

	const float InvCount = 1.0f / static_cast<float>(Positions.size());
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Col = 0; Col < 3; ++Col)
		{
			Covariance.M[Row][Col] *= InvCount;
		}
	}

	Covariance.M[3][3] = 1.0f;
	return Covariance;
}

FVector ComputeDominantEigenVector(const FMatrix& Matrix)
{
	FVector Vector = FVector::ZAxisVector;
	for (int32 Iteration = 0; Iteration < 32; ++Iteration)
	{
		const FVector NextVector = Matrix.TransformVector(Vector);
		const float Length = NextVector.Length();
		if (Length > 0.0f)
		{
			Vector = NextVector / Length;
		}
	}

	return Vector.GetSafeNormal(1.0e-6f, FVector::ZAxisVector);
}

void FindBestAxisVectors(const FVector& AxisZ, FVector& AxisX, FVector& AxisY)
{
	const FVector NormalizedZ = AxisZ.GetSafeNormal(1.0e-6f, FVector::ZAxisVector);
	const FVector UpVector = (std::abs(NormalizedZ.Z) < 0.999f) ? FVector::ZAxisVector : FVector::XAxisVector;

	AxisX = UpVector.Cross(NormalizedZ).GetSafeNormal(1.0e-6f, FVector::XAxisVector);
	AxisY = NormalizedZ.Cross(AxisX).GetSafeNormal(1.0e-6f, FVector::YAxisVector);
}

float GetMaxComponent(const FVector& Value)
{
	return std::max(Value.X, std::max(Value.Y, Value.Z));
}

float GetBoneFitSize(const FBoneShapeFit& Fit)
{
	return Fit.bHasVertices ? Fit.GetExtent().Length() : 0.0f;
}

bool IsValidBoneIndex(const FSkeletalMesh& MeshAsset, int32 BoneIndex)
{
	return BoneIndex >= 0 && BoneIndex < static_cast<int32>(MeshAsset.Bones.size());
}

FBoneShapeFit TransformFitToBoneSpace(
	const FSkeletalMesh& MeshAsset,
	const FBoneShapeFit& SourceFit,
	int32 SourceBoneIndex,
	int32 TargetBoneIndex)
{
	FBoneShapeFit Result;
	if (!IsValidBoneIndex(MeshAsset, SourceBoneIndex) ||
		!IsValidBoneIndex(MeshAsset, TargetBoneIndex))
	{
		return Result;
	}

	if (!SourceFit.bHasVertices)
	{
		return Result;
	}

	if (SourceBoneIndex == TargetBoneIndex)
	{
		return SourceFit;
	}

	const FMatrix SourceToTarget =
		MeshAsset.Bones[SourceBoneIndex].GetReferenceGlobalPose() *
		MeshAsset.Bones[TargetBoneIndex].GetReferenceGlobalPose().GetAffineInverse();

	for (const FVector& Position : SourceFit.Positions)
	{
		Result.AddPosition(SourceToTarget.TransformPosition(Position));
	}

	return Result;
}

FBoneShapeFit TransformFitToBoneSpace(
	const FSkeletalMesh& MeshAsset,
	const TArray<FBoneShapeFit>& BoneFits,
	int32 SourceBoneIndex,
	int32 TargetBoneIndex)
{
	if (!IsValidBoneIndex(MeshAsset, SourceBoneIndex))
	{
		return FBoneShapeFit();
	}

	return TransformFitToBoneSpace(MeshAsset, BoneFits[SourceBoneIndex], SourceBoneIndex, TargetBoneIndex);
}

void AppendFit(FBoneShapeFit& TargetFit, const FBoneShapeFit& SourceFit)
{
	if (!SourceFit.bHasVertices)
	{
		return;
	}

	for (const FVector& Position : SourceFit.Positions)
	{
		TargetFit.AddPosition(Position);
	}
}

void AppendFitToBoneSpace(
	const FSkeletalMesh& MeshAsset,
	FBoneShapeFit& TargetFit,
	const FBoneShapeFit& SourceFit,
	int32 SourceBoneIndex,
	int32 TargetBoneIndex)
{
	AppendFit(TargetFit, TransformFitToBoneSpace(MeshAsset, SourceFit, SourceBoneIndex, TargetBoneIndex));
}

FBoneShapeFit MakeDefaultBoneFit()
{
	FBoneShapeFit Fit;
	Fit.AddPosition(FVector::ZeroVector);
	return Fit;
}

struct FUnrealMergedBoneData
{
	TArray<FBoneShapeFit> ExtraFits;
	TArray<float> MergedSizes;
	int32 ForcedRootBoneIndex = -1;
};

FUnrealMergedBoneData BuildUnrealMergedBoneData(
	const FSkeletalMesh& MeshAsset,
	const TArray<FBoneShapeFit>& BoneFits,
	const FPhysicsAssetBuildOptions& Options)
{
	FUnrealMergedBoneData Data;
	Data.ExtraFits.resize(MeshAsset.Bones.size());
	Data.MergedSizes.resize(MeshAsset.Bones.size(), 0.0f);

	for (int32 BoneIndex = static_cast<int32>(MeshAsset.Bones.size()) - 1; BoneIndex >= 0; --BoneIndex)
	{
		if (!IsValidBoneIndex(MeshAsset, BoneIndex))
		{
			continue;
		}

		const float MyMergedSize = Data.MergedSizes[BoneIndex] + GetBoneFitSize(BoneFits[BoneIndex]);
		Data.MergedSizes[BoneIndex] = MyMergedSize;

		if (!Options.bWalkPastSmall || Options.bBodyForAll)
		{
			continue;
		}

		if (MyMergedSize >= Options.MinBoneSize || MyMergedSize < Options.MinWeldSize)
		{
			continue;
		}

		const int32 ParentIndex = MeshAsset.Bones[BoneIndex].ParentIndex;
		if (!IsValidBoneIndex(MeshAsset, ParentIndex))
		{
			continue;
		}

		Data.MergedSizes[ParentIndex] += MyMergedSize;
		AppendFitToBoneSpace(
			MeshAsset,
			Data.ExtraFits[ParentIndex],
			BoneFits[BoneIndex],
			BoneIndex,
			ParentIndex);
		AppendFitToBoneSpace(
			MeshAsset,
			Data.ExtraFits[ParentIndex],
			Data.ExtraFits[BoneIndex],
			BoneIndex,
			ParentIndex);
	}

	int32 FirstParentBoneIndex = -1;
	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MeshAsset.Bones.size()); ++BoneIndex)
	{
		if (Data.MergedSizes[BoneIndex] <= Options.MinBoneSize)
		{
			continue;
		}

		const int32 ParentBoneIndex = MeshAsset.Bones[BoneIndex].ParentIndex;
		if (ParentBoneIndex == -1)
		{
			break;
		}

		if (FirstParentBoneIndex == -1)
		{
			FirstParentBoneIndex = ParentBoneIndex;
			continue;
		}

		if (ParentBoneIndex == FirstParentBoneIndex)
		{
			Data.ForcedRootBoneIndex = ParentBoneIndex;
			break;
		}
	}

	return Data;
}

int32 FindBoneIndexByName(const FSkeletalMesh& MeshAsset, const FName& BoneName)
{
	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MeshAsset.Bones.size()); ++BoneIndex)
	{
		if (FName(MeshAsset.Bones[BoneIndex].Name) == BoneName)
		{
			return BoneIndex;
		}
	}

	return -1;
}

void BuildConstraintInitDescsForBodies(UPhysicsAsset* PhysicsAsset, const FSkeletalMesh& MeshAsset)
{
	TArray<FConstraintInstanceInitDesc>& ConstraintInitDescs = PhysicsAsset->GetConstraintInitDescsMutable();
	ConstraintInitDescs.clear();

	for (const UBodySetup* ChildBodySetup : PhysicsAsset->GetBodySetups())
	{
		if (!ChildBodySetup)
		{
			continue;
		}

		const int32 ChildBoneIndex = FindBoneIndexByName(MeshAsset, ChildBodySetup->BoneName);
		if (ChildBoneIndex < 0)
		{
			continue;
		}

		int32 ParentBodyBoneIndex = -1;
		int32 ParentBoneIndex = MeshAsset.Bones[ChildBoneIndex].ParentIndex;
		while (ParentBoneIndex >= 0 && ParentBoneIndex < static_cast<int32>(MeshAsset.Bones.size()))
		{
			const FName ParentBoneName(MeshAsset.Bones[ParentBoneIndex].Name);
			if (PhysicsAsset->FindBodyIndexByBoneName(ParentBoneName) != -1)
			{
				ParentBodyBoneIndex = ParentBoneIndex;
				break;
			}

			ParentBoneIndex = MeshAsset.Bones[ParentBoneIndex].ParentIndex;
		}

		if (ParentBodyBoneIndex < 0)
		{
			continue;
		}

		FConstraintInstanceInitDesc Desc;
		Desc.ParentBoneName = FName(MeshAsset.Bones[ParentBodyBoneIndex].Name);
		Desc.ChildBoneName = ChildBodySetup->BoneName;
		Desc.ParentFrame = FTransform::FromMatrixWithScale(
			MeshAsset.Bones[ChildBoneIndex].GetReferenceGlobalPose() *
			MeshAsset.Bones[ParentBodyBoneIndex].GetReferenceGlobalPose().GetAffineInverse());
		Desc.ChildFrame = FTransform();
		Desc.TwistLimitDegrees = 45.0f;
		Desc.Swing1LimitDegrees = 35.0f;
		Desc.Swing2LimitDegrees = 35.0f;
		Desc.bEnableCollision = false;
		Desc.bEnableProjection = true;
		Desc.ProjectionLinearTolerance = 10.0f;
		Desc.ProjectionAngularToleranceDegrees = 30.0f;

		ConstraintInitDescs.push_back(Desc);
	}
}

FTransform MakeRotationOnlyTransform(const FVector& Axis, float AngleRad)
{
	return FTransform(
		FVector::ZeroVector,
		FQuat::FromAxisAngle(Axis.GetSafeNormal(), AngleRad),
		FVector::OneVector);
}

FShapeFitFrame BuildShapeFitFrame(
	const FVector& FitCenter,
	const FVector& FitExtent,
	const TArray<FVector>& FitPositions,
	const FPhysicsAssetBuildOptions& Options)
{
	FShapeFitFrame Frame;

	if (Options.bAutoOrientToBone && !FitPositions.empty())
	{
		const FMatrix Covariance = ComputeCovarianceMatrix(FitPositions);
		const FVector AxisZ = ComputeDominantEigenVector(Covariance);
		FVector AxisX;
		FVector AxisY;
		FindBestAxisVectors(AxisZ, AxisX, AxisY);

		FMatrix ElementMatrix = FMatrix::Identity;
		ElementMatrix.SetAxes(AxisX, AxisY, AxisZ);
		Frame.ElementTransform = FTransform(ElementMatrix);
	}

	bool bHasBounds = false;
	FVector LocalMin = FVector::ZeroVector;
	FVector LocalMax = FVector::ZeroVector;

	for (const FVector& Position : FitPositions)
	{
		const FVector LocalPosition = Frame.ElementTransform.InverseTransformPositionNoScale(Position);
		if (!bHasBounds)
		{
			LocalMin = LocalPosition;
			LocalMax = LocalPosition;
			bHasBounds = true;
			continue;
		}

		LocalMin.X = std::min(LocalMin.X, LocalPosition.X);
		LocalMin.Y = std::min(LocalMin.Y, LocalPosition.Y);
		LocalMin.Z = std::min(LocalMin.Z, LocalPosition.Z);
		LocalMax.X = std::max(LocalMax.X, LocalPosition.X);
		LocalMax.Y = std::max(LocalMax.Y, LocalPosition.Y);
		LocalMax.Z = std::max(LocalMax.Z, LocalPosition.Z);
	}

	FVector BoxCenter = FitCenter;
	Frame.BoxExtent = FitExtent;
	if (bHasBounds)
	{
		BoxCenter = (LocalMin + LocalMax) * 0.5f;
		Frame.BoxExtent = (LocalMax - LocalMin) * 0.5f;
	}

	Frame.BoxExtent.X = std::max(Frame.BoxExtent.X, Options.MinPrimitiveSize);
	Frame.BoxExtent.Y = std::max(Frame.BoxExtent.Y, Options.MinPrimitiveSize);
	Frame.BoxExtent.Z = std::max(Frame.BoxExtent.Z, Options.MinPrimitiveSize);

	Frame.ElementTransform.Location = Frame.ElementTransform.TransformPositionNoScale(BoxCenter);
	return Frame;
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
	TArray<UBodySetup*>& BodySetups = PhysicsAsset->GetBodySetupsMutable();

	auto PopulateBodies = [&](const FPhysicsAssetBuildOptions& BuildOptions) -> bool
	{
		BodySetups.clear();

		const TArray<FBoneShapeFit> BoneFits = BuildBoneShapeFits(*MeshAsset, BuildOptions);
		const FUnrealMergedBoneData MergedBoneData = BuildUnrealMergedBoneData(
			*MeshAsset,
			BoneFits,
			BuildOptions);

		auto ShouldMakeBone = [&](int32 BoneIndex) -> bool
		{
			if (BuildOptions.bBodyForAll)
			{
				return true;
			}

			if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(MergedBoneData.MergedSizes.size()) &&
				MergedBoneData.MergedSizes[BoneIndex] > BuildOptions.MinBoneSize)
			{
				return true;
			}

			return BoneIndex == MergedBoneData.ForcedRootBoneIndex;
		};

		auto CreateBodyFromFit = [&](int32 BodyBoneIndex, const FBoneShapeFit& Fit) -> bool
		{
			if (!IsValidBoneIndex(*MeshAsset, BodyBoneIndex) || !Fit.bHasVertices)
			{
				return false;
			}

			const FBone& Bone = MeshAsset->Bones[BodyBoneIndex];

			UBodySetup* BodySetup = UObjectManager::Get().CreateObject<UBodySetup>(PhysicsAsset);
			if (!BodySetup)
			{
				return false;
			}

			BodySetup->BoneName = FName(Bone.Name);
			AddFittedShapeForBone(
				BodySetup,
				Fit.GetCenter(),
				Fit.GetExtent(),
				Fit.Positions,
				BuildOptions);

			BodySetups.push_back(BodySetup);
			return true;
		};

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MeshAsset->Bones.size()); ++BoneIndex)
		{
			if (!ShouldMakeBone(BoneIndex))
			{
				continue;
			}

			FBoneShapeFit BodyFit = BoneFits[BoneIndex];
			if (BoneIndex < static_cast<int32>(MergedBoneData.ExtraFits.size()))
			{
				AppendFit(BodyFit, MergedBoneData.ExtraFits[BoneIndex]);
			}

			if (!BodyFit.bHasVertices && BuildOptions.bBodyForAll)
			{
				BodyFit = MakeDefaultBoneFit();
			}

			CreateBodyFromFit(BoneIndex, BodyFit);
		}

		return !BodySetups.empty();
	};

	if (!PopulateBodies(Options))
	{
		FPhysicsAssetBuildOptions RetryOptions = Options;
		RetryOptions.MinBoneSize = 1.0f;
		PopulateBodies(RetryOptions);
	}

	BuildConstraintInitDescsForBodies(PhysicsAsset, *MeshAsset);
}

void FPhysicsAssetBuilder::AddFittedShapeForBone(
	UBodySetup* BodySetup,
	const FVector& FitCenter,
	const FVector& FitExtent,
	const TArray<FVector>& FitPositions,
	const FPhysicsAssetBuildOptions& Options)
{
	if (!BodySetup)
	{
		return;
	}

	const FShapeFitFrame FitFrame = BuildShapeFitFrame(FitCenter, FitExtent, FitPositions, Options);
	const FTransform& ElementTransform = FitFrame.ElementTransform;
	const FVector& BoxExtent = FitFrame.BoxExtent;

	if (Options.GeomType == EPhysicsAssetFitGeomType::Box)
	{
		FKBoxElem Box;
		Box.SetTransform(ElementTransform);
		Box.X = std::max(BoxExtent.X * 2.0f * Options.FitPadding, Options.MinPrimitiveSize);
		Box.Y = std::max(BoxExtent.Y * 2.0f * Options.FitPadding, Options.MinPrimitiveSize);
		Box.Z = std::max(BoxExtent.Z * 2.0f * Options.FitPadding, Options.MinPrimitiveSize);
		BodySetup->GetAggGeom().BoxElems.push_back(Box);
		return;
	}

	if (Options.GeomType == EPhysicsAssetFitGeomType::Sphere)
	{
		FKSphereElem Sphere;
		Sphere.Center = ElementTransform.GetLocation();
		Sphere.Radius = std::max(GetMaxComponent(BoxExtent) * Options.FitPadding, Options.MinPrimitiveSize);
		BodySetup->GetAggGeom().SphereElems.push_back(Sphere);
		return;
	}

	FKSphylElem Sphyl;
	if (BoxExtent.X > BoxExtent.Z && BoxExtent.X > BoxExtent.Y)
	{
		constexpr float HalfPi = 1.57079632679f;
		Sphyl.SetTransform(MakeRotationOnlyTransform(FVector::YAxisVector, -HalfPi) * ElementTransform);
		Sphyl.Radius = std::max(std::max(BoxExtent.Y, BoxExtent.Z) * Options.FitPadding, Options.MinPrimitiveSize);
		Sphyl.Length = std::max(BoxExtent.X * Options.FitPadding, Options.MinPrimitiveSize);
	}
	else if (BoxExtent.Y > BoxExtent.Z && BoxExtent.Y > BoxExtent.X)
	{
		constexpr float HalfPi = 1.57079632679f;
		Sphyl.SetTransform(MakeRotationOnlyTransform(FVector::XAxisVector, HalfPi) * ElementTransform);
		Sphyl.Radius = std::max(std::max(BoxExtent.X, BoxExtent.Z) * Options.FitPadding, Options.MinPrimitiveSize);
		Sphyl.Length = std::max(BoxExtent.Y * Options.FitPadding, Options.MinPrimitiveSize);
	}
	else
	{
		Sphyl.SetTransform(ElementTransform);
		Sphyl.Radius = std::max(std::max(BoxExtent.X, BoxExtent.Y) * Options.FitPadding, Options.MinPrimitiveSize);
		Sphyl.Length = std::max(BoxExtent.Z * Options.FitPadding, Options.MinPrimitiveSize);
	}

	BodySetup->GetAggGeom().SphylElems.push_back(Sphyl);
}
