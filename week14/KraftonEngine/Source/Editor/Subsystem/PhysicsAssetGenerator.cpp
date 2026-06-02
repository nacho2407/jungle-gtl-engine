#include "PhysicsAssetGenerator.h"

#include "AssetFactory.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsConstraintTemplate.h"
#include "Math/Matrix.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>

namespace
{
	constexpr float KINDA_SMALL_NUMBER = 1.e-4f;
	constexpr float UEStyleMinPrimSizeRatio = 0.5f / 170.0f;
	constexpr float MinPrimSizeFloor = 1.e-4f;
	constexpr float ShapeSizePadding = 1.01f;

	struct FBoneVertInfo
	{
		TArray<FVector> Positions;
		TArray<FVector> Normals;
	};

	struct FBoxFit
	{
		bool bValid = false;
		FVector Center = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
	};

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return std::max(MinValue, std::min(Value, MaxValue));
	}

	float Max3(float A, float B, float C)
	{
		return std::max(A, std::max(B, C));
	}

	float CalcMeshLongDimension(const FSkeletalMesh& Mesh)
	{
		if (Mesh.Vertices.empty())
		{
			return 1.0f;
		}

		FVector Min = Mesh.Vertices[0].Position;
		FVector Max = Mesh.Vertices[0].Position;
		for (const FVertexPNCTBW& Vertex : Mesh.Vertices)
		{
			Min.X = std::min(Min.X, Vertex.Position.X);
			Min.Y = std::min(Min.Y, Vertex.Position.Y);
			Min.Z = std::min(Min.Z, Vertex.Position.Z);
			Max.X = std::max(Max.X, Vertex.Position.X);
			Max.Y = std::max(Max.Y, Vertex.Position.Y);
			Max.Z = std::max(Max.Z, Vertex.Position.Z);
		}

		const FVector Size = Max - Min;
		return std::max(Max3(Size.X, Size.Y, Size.Z), KINDA_SMALL_NUMBER);
	}

	float CalcEffectiveMinPrimSize(float MeshLongDimension)
	{
		return std::max(MeshLongDimension * UEStyleMinPrimSizeRatio, MinPrimSizeFloor);
	}

	float ResolveCreationSize(float Value, float MeshLongDimension)
	{
		if (Value <= 0.0f)
		{
			return Value;
		}

		// UE authoring values are usually centimeters. Meter-scale imported meshes
		// need those large UI values converted back into local mesh units.
		if (MeshLongDimension < 10.0f && Value > MeshLongDimension)
		{
			return Value * 0.01f;
		}

		return Value;
	}

	FString NormalizeBoneName(FString Value)
	{
		FString Result;
		Result.reserve(Value.size());
		for (char C : Value)
		{
			const unsigned char U = static_cast<unsigned char>(C);
			if (std::isalnum(U))
			{
				Result.push_back(static_cast<char>(std::tolower(U)));
			}
		}
		return Result;
	}

	bool ContainsText(const FString& Value, const char* Token)
	{
		return Value.find(Token) != FString::npos;
	}

	bool StartsWithText(const FString& Value, const char* Token)
	{
		const size_t TokenLength = std::strlen(Token);
		return Value.size() >= TokenLength
			&& Value.compare(0, TokenLength, Token) == 0;
	}

	bool EndsWithText(const FString& Value, const char* Token)
	{
		const size_t TokenLength = std::strlen(Token);
		return Value.size() >= TokenLength
			&& Value.compare(Value.size() - TokenLength, TokenLength, Token) == 0;
	}

	bool IsHumanoidBodyBoneName(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return ContainsText(Name, "pelvis")
			|| ContainsText(Name, "hips")
			|| ContainsText(Name, "spine")
			|| ContainsText(Name, "chest")
			|| ContainsText(Name, "abdomen")
			|| ContainsText(Name, "neck")
			|| ContainsText(Name, "head")
			|| ContainsText(Name, "clavicle")
			|| ContainsText(Name, "shoulder")
			|| ContainsText(Name, "upperarm")
			|| ContainsText(Name, "lowerarm")
			|| ContainsText(Name, "forearm")
			|| ContainsText(Name, "arm")
			|| ContainsText(Name, "elbow")
			|| ContainsText(Name, "wrist")
			|| ContainsText(Name, "hand")
			|| ContainsText(Name, "thigh")
			|| ContainsText(Name, "calf")
			|| ContainsText(Name, "shin")
			|| ContainsText(Name, "upleg")
			|| ContainsText(Name, "leg")
			|| ContainsText(Name, "knee")
			|| ContainsText(Name, "ankle")
			|| ContainsText(Name, "foot");
	}

	bool IsSecondaryBoneName(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return EndsWithText(Name, "end")
			|| ContainsText(Name, "finger")
			|| ContainsText(Name, "thumb")
			|| ContainsText(Name, "index")
			|| ContainsText(Name, "middle")
			|| ContainsText(Name, "ring")
			|| ContainsText(Name, "pinky")
			|| ContainsText(Name, "little")
			// || ContainsText(Name, "toe")
			|| ContainsText(Name, "twist")
			|| ContainsText(Name, "roll")
			|| ContainsText(Name, "skirt")
			|| ContainsText(Name, "cloth")
			|| ContainsText(Name, "dress")
			|| ContainsText(Name, "hair")
			|| ContainsText(Name, "ribbon")
			|| ContainsText(Name, "accessory")
			|| ContainsText(Name, "sleeve")
			|| ContainsText(Name, "breast")
			|| ContainsText(Name, "ear")
			|| ContainsText(Name, "eye")
			|| ContainsText(Name, "hat")
			|| ContainsText(Name, "width")
			|| StartsWithText(Name, "hb")
			|| StartsWithText(Name, "cf")
			|| StartsWithText(Name, "dm")
			|| StartsWithText(Name, "szy")
			|| StartsWithText(BoneName, "F_");
	}

	bool LooksLikeHumanoidSkeleton(const FSkeletalMesh& Mesh)
	{
		bool bHasHips = false;
		bool bHasSpine = false;
		bool bHasHead = false;
		bool bHasArm = false;
		bool bHasLeg = false;

		for (const FBone& Bone : Mesh.Bones)
		{
			const FString Name = NormalizeBoneName(Bone.Name);
			bHasHips |= ContainsText(Name, "pelvis") || ContainsText(Name, "hips");
			bHasSpine |= ContainsText(Name, "spine") || ContainsText(Name, "chest");
			bHasHead |= ContainsText(Name, "head");
			bHasArm |= ContainsText(Name, "upperarm") || ContainsText(Name, "lowerarm") || ContainsText(Name, "forearm") || ContainsText(Name, "arm");
			bHasLeg |= ContainsText(Name, "thigh") || ContainsText(Name, "calf") || ContainsText(Name, "shin") || ContainsText(Name, "upleg") || ContainsText(Name, "leg");
		}

		return bHasHips && bHasSpine && bHasHead && bHasArm && bHasLeg;
	}

	bool IsValidBoneIndex(const FSkeletalMesh& Mesh, int32 BoneIndex)
	{
		return BoneIndex >= 0 && BoneIndex < static_cast<int32>(Mesh.Bones.size());
	}

	int32 FindBoneIndexByName(const FSkeletalMesh& Mesh, FName BoneName)
	{
		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Mesh.Bones.size()); ++BoneIndex)
		{
			if (FName(Mesh.Bones[BoneIndex].Name) == BoneName)
			{
				return BoneIndex;
			}
		}
		return -1;
	}

	int32 FindMergeParentIndex(const FSkeletalMesh& Mesh, const TArray<bool>& bIgnoreBone, int32 BoneIndex)
	{
		int32 ParentIndex = Mesh.Bones[BoneIndex].ParentIndex;
		while (IsValidBoneIndex(Mesh, ParentIndex) && bIgnoreBone[ParentIndex])
		{
			ParentIndex = Mesh.Bones[ParentIndex].ParentIndex;
		}
		return ParentIndex;
	}

	void BuildReferenceGlobalMatrices(const FSkeletalMesh& Mesh, TArray<FMatrix>& OutReferenceGlobals)
	{
		const int32 NumBones = static_cast<int32>(Mesh.Bones.size());
		OutReferenceGlobals.clear();
		OutReferenceGlobals.resize(NumBones);

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const FMatrix LocalMatrix = Mesh.Bones[BoneIndex].GetReferenceLocalPose();
			const int32 ParentIndex = Mesh.Bones[BoneIndex].ParentIndex;
			OutReferenceGlobals[BoneIndex] = (ParentIndex >= 0 && ParentIndex < BoneIndex)
				? LocalMatrix * OutReferenceGlobals[ParentIndex]
				: LocalMatrix;
		}
	}

	const FMatrix& GetReferenceGlobalMatrix(const TArray<FMatrix>& ReferenceGlobals, const FSkeletalMesh& Mesh, int32 BoneIndex)
	{
		if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(ReferenceGlobals.size()))
		{
			return ReferenceGlobals[BoneIndex];
		}
		return Mesh.Bones[BoneIndex].GetReferenceGlobalPose();
	}

	FMatrix GetBoneMeshToLocalMatrix(const FSkeletalMesh& Mesh, int32 BoneIndex)
	{
		return Mesh.Bones[BoneIndex].InverseBindPoseMatrix;
	}

	FMatrix GetBoneLocalToMeshMatrix(const FSkeletalMesh& Mesh, int32 BoneIndex)
	{
		return Mesh.Bones[BoneIndex].InverseBindPoseMatrix.GetInverse();
	}

	FMatrix GetBoneLocalToBoneLocalMatrix(const FSkeletalMesh& Mesh, int32 BoneIndex, int32 TargetBoneIndex)
	{
		return GetBoneLocalToMeshMatrix(Mesh, BoneIndex) * GetBoneMeshToLocalMatrix(Mesh, TargetBoneIndex);
	}

	void AddPositionNormalToBone(const FSkeletalMesh& Mesh, int32 BoneIndex, const FVertexPNCTBW& Vertex, FBoneVertInfo& Info)
	{
		const FMatrix MeshToBone = GetBoneMeshToLocalMatrix(Mesh, BoneIndex);
		Info.Positions.push_back(MeshToBone.TransformPositionWithW(Vertex.Position));

		FVector LocalNormal = MeshToBone.TransformVector(Vertex.Normal);
		if (!LocalNormal.IsNearlyZero())
		{
			LocalNormal.Normalize();
		}
		Info.Normals.push_back(LocalNormal);
	}

	void CalcBoneVertInfos(const FSkeletalMesh& Mesh, TArray<FBoneVertInfo>& Infos, bool bOnlyDominant)
	{
		const int32 NumBones = static_cast<int32>(Mesh.Bones.size());
		Infos.clear();
		Infos.resize(NumBones);

		for (const FVertexPNCTBW& Vertex : Mesh.Vertices)
		{
			if (bOnlyDominant)
			{
				int32 BestBoneIndex = -1;
				float BestWeight = 0.0f;
				for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
				{
					const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
					const float Weight = Vertex.BoneWeights[InfluenceIndex];
					if (IsValidBoneIndex(Mesh, BoneIndex) && Weight > BestWeight)
					{
						BestWeight = Weight;
						BestBoneIndex = BoneIndex;
					}
				}

				if (BestBoneIndex >= 0)
				{
					AddPositionNormalToBone(Mesh, BestBoneIndex, Vertex, Infos[BestBoneIndex]);
				}
				continue;
			}

			for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
			{
				const int32 BoneIndex = Vertex.BoneIndices[InfluenceIndex];
				if (IsValidBoneIndex(Mesh, BoneIndex) && Vertex.BoneWeights[InfluenceIndex] > KINDA_SMALL_NUMBER)
				{
					AddPositionNormalToBone(Mesh, BoneIndex, Vertex, Infos[BoneIndex]);
				}
			}
		}
	}

	bool CalcBounds(const TArray<FVector>& Points, FVector& OutMin, FVector& OutMax)
	{
		if (Points.empty())
		{
			return false;
		}

		OutMin = Points[0];
		OutMax = Points[0];
		for (const FVector& Point : Points)
		{
			OutMin.X = std::min(OutMin.X, Point.X);
			OutMin.Y = std::min(OutMin.Y, Point.Y);
			OutMin.Z = std::min(OutMin.Z, Point.Z);
			OutMax.X = std::max(OutMax.X, Point.X);
			OutMax.Y = std::max(OutMax.Y, Point.Y);
			OutMax.Z = std::max(OutMax.Z, Point.Z);
		}

		return true;
	}

	float CalcBoneInfoLength(const FBoneVertInfo& Info)
	{
		FVector Min;
		FVector Max;
		if (!CalcBounds(Info.Positions, Min, Max))
		{
			return 0.0f;
		}

		const FVector Extent = (Max - Min) * 0.5f;
		return Extent.Length();
	}

	void AddInfoToParentInfo(const FMatrix& LocalToParentMatrix, const FBoneVertInfo& ChildInfo, FBoneVertInfo& ParentInfo)
	{
		ParentInfo.Positions.reserve(ParentInfo.Positions.size() + ChildInfo.Positions.size());
		ParentInfo.Normals.reserve(ParentInfo.Normals.size() + ChildInfo.Normals.size());

		for (const FVector& Position : ChildInfo.Positions)
		{
			ParentInfo.Positions.push_back(LocalToParentMatrix.TransformPositionWithW(Position));
		}

		for (const FVector& Normal : ChildInfo.Normals)
		{
			FVector ParentNormal = LocalToParentMatrix.TransformVector(Normal);
			if (!ParentNormal.IsNearlyZero())
			{
				ParentNormal.Normalize();
			}
			ParentInfo.Normals.push_back(ParentNormal);
		}
	}

	FMatrix ComputeCovarianceMatrix(const TArray<FVector>& Points)
	{
		if (Points.empty())
		{
			return FMatrix::Identity;
		}

		FVector Mean = FVector::ZeroVector;
		for (const FVector& Point : Points)
		{
			Mean += Point;
		}
		Mean /= static_cast<float>(Points.size());

		FMatrix Covariance = FMatrix::Identity;
		Covariance.M[0][0] = Covariance.M[0][1] = Covariance.M[0][2] = 0.0f;
		Covariance.M[1][0] = Covariance.M[1][1] = Covariance.M[1][2] = 0.0f;
		Covariance.M[2][0] = Covariance.M[2][1] = Covariance.M[2][2] = 0.0f;

		for (const FVector& Point : Points)
		{
			const FVector Error = Point - Mean;
			for (int32 Row = 0; Row < 3; ++Row)
			{
				for (int32 Col = 0; Col < 3; ++Col)
				{
					Covariance.M[Row][Col] += Error.Data[Row] * Error.Data[Col];
				}
			}
		}

		const float InvCount = 1.0f / static_cast<float>(Points.size());
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Col = 0; Col < 3; ++Col)
			{
				Covariance.M[Row][Col] *= InvCount;
			}
		}

		Covariance.M[0][3] = Covariance.M[1][3] = Covariance.M[2][3] = 0.0f;
		Covariance.M[3][0] = Covariance.M[3][1] = Covariance.M[3][2] = 0.0f;
		Covariance.M[3][3] = 1.0f;
		return Covariance;
	}

	FVector ComputeDominantEigenVector(const FMatrix& Matrix)
	{
		FVector Axis = FVector::ZAxisVector;
		for (int32 Iteration = 0; Iteration < 32; ++Iteration)
		{
			FVector Next = Matrix.TransformVector(Axis);
			if (Next.IsNearlyZero())
			{
				break;
			}
			Next.Normalize();
			Axis = Next;
		}

		if (Axis.IsNearlyZero())
		{
			return FVector::ZAxisVector;
		}

		Axis.Normalize();
		return Axis;
	}

	void FindBestAxisVectors(const FVector& ZAxis, FVector& OutXAxis, FVector& OutYAxis)
	{
		const FVector UpHint = std::fabs(ZAxis.Z) < 0.75f ? FVector::ZAxisVector : FVector::XAxisVector;
		OutXAxis = UpHint.Cross(ZAxis);
		if (OutXAxis.IsNearlyZero())
		{
			OutXAxis = FVector::YAxisVector.Cross(ZAxis);
		}
		OutXAxis.Normalize();

		OutYAxis = ZAxis.Cross(OutXAxis);
		if (OutYAxis.IsNearlyZero())
		{
			OutYAxis = FVector::YAxisVector;
		}
		OutYAxis.Normalize();
	}

	FQuat MakeRotationFromAxes(const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis)
	{
		FMatrix AxisMatrix = FMatrix::Identity;
		AxisMatrix.SetAxes(XAxis, YAxis, ZAxis);
		return AxisMatrix.ToQuat().GetNormalized();
	}

	FQuat MakeQuatFromZToAxis(const FVector& TargetAxis)
	{
		FVector Axis = TargetAxis;
		if (Axis.IsNearlyZero())
		{
			return FQuat::Identity;
		}
		Axis.Normalize();

		const float Dot = ClampFloat(FVector::ZAxisVector.Dot(Axis), -1.0f, 1.0f);
		if (Dot > 0.9999f)
		{
			return FQuat::Identity;
		}
		if (Dot < -0.9999f)
		{
			return FQuat::FromAxisAngle(FVector::XAxisVector, 3.14159265f);
		}

		FVector RotationAxis = FVector::ZAxisVector.Cross(Axis);
		RotationAxis.Normalize();
		return FQuat::FromAxisAngle(RotationAxis, std::acos(Dot)).GetNormalized();
	}

	FBoxFit FitBoneOrientedBox(const FBoneVertInfo& Info, bool bAutoOrientToBone, float MinPrimSize)
	{
		FBoxFit Result;

		if (bAutoOrientToBone && !Info.Positions.empty())
		{
			const FMatrix Covariance = ComputeCovarianceMatrix(Info.Positions);
			FVector ZAxis = ComputeDominantEigenVector(Covariance);
			FVector XAxis;
			FVector YAxis;
			FindBestAxisVectors(ZAxis, XAxis, YAxis);
			Result.Rotation = MakeRotationFromAxes(XAxis, YAxis, ZAxis);
		}

		const FQuat InvRotation = Result.Rotation.Inverse();
		TArray<FVector> RotatedPoints;
		RotatedPoints.reserve(Info.Positions.size());
		for (const FVector& Position : Info.Positions)
		{
			RotatedPoints.push_back(InvRotation.RotateVector(Position));
		}

		FVector Min;
		FVector Max;
		Result.bValid = CalcBounds(RotatedPoints, Min, Max);
		if (!Result.bValid)
		{
			Result.Center = FVector::ZeroVector;
			Result.Extent = FVector(MinPrimSize, MinPrimSize, MinPrimSize);
			return Result;
		}

		const FVector LocalCenter = (Min + Max) * 0.5f;
		FVector Extent = (Max - Min) * 0.5f;

		Extent.X = std::max(Extent.X, MinPrimSize);
		Extent.Y = std::max(Extent.Y, MinPrimSize);
		Extent.Z = std::max(Extent.Z, MinPrimSize);

		Result.Center = Result.Rotation.RotateVector(LocalCenter);
		Result.Extent = Extent;
		return Result;
	}

	void AddCollisionFromBoneInfo(UBodySetup& Body, EPhysicsAssetPrimitiveType PrimitiveType, const FBoneVertInfo& Info, bool bAutoOrientToBone, float MinPrimSize)
	{
		Body.ClearShapes();

		const FBoxFit Fit = FitBoneOrientedBox(Info, bAutoOrientToBone, MinPrimSize);
		const FVector Extent = Fit.Extent;

		switch (PrimitiveType)
		{
		case EPhysicsAssetPrimitiveType::Box:
			Body.AddBox(Fit.Center, Fit.Rotation, Extent * ShapeSizePadding);
			break;

		case EPhysicsAssetPrimitiveType::Sphere:
			Body.AddSphere(Fit.Center, Max3(Extent.X, Extent.Y, Extent.Z) * ShapeSizePadding);
			break;

		case EPhysicsAssetPrimitiveType::Capsule:
		default:
		{
			int32 LongAxis = 2;
			if (Extent.X > Extent.Z && Extent.X > Extent.Y)
			{
				LongAxis = 0;
			}
			else if (Extent.Y > Extent.Z && Extent.Y > Extent.X)
			{
				LongAxis = 1;
			}

			const FVector LocalAxis = LongAxis == 0
				? FVector::XAxisVector
				: LongAxis == 1 ? FVector::YAxisVector : FVector::ZAxisVector;
			const FVector CapsuleAxis = Fit.Rotation.RotateVector(LocalAxis);
			const FQuat CapsuleRotation = MakeQuatFromZToAxis(CapsuleAxis);

			const float LongExtent = Extent.Data[LongAxis];
			const float Radius = (LongAxis == 0)
				? std::max(Extent.Y, Extent.Z)
				: LongAxis == 1 ? std::max(Extent.X, Extent.Z) : std::max(Extent.X, Extent.Y);

			const float SegmentLength = std::max((LongExtent - Radius) * 2.0f, MinPrimSize);
			Body.AddSphyl(Fit.Center, CapsuleRotation, Radius * ShapeSizePadding, SegmentLength * ShapeSizePadding);
			break;
		}
		}
	}

	int32 FindForcedRootBoneIndex(const FSkeletalMesh& Mesh, const TArray<float>& MergedSizes, const TArray<bool>& bIgnoreBone, float MinBoneSize)
	{
		int32 FirstParentBoneIndex = -1;
		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MergedSizes.size()); ++BoneIndex)
		{
			if (bIgnoreBone[BoneIndex] || MergedSizes[BoneIndex] <= MinBoneSize)
			{
				continue;
			}

			const int32 ParentBoneIndex = Mesh.Bones[BoneIndex].ParentIndex;
			if (!IsValidBoneIndex(Mesh, ParentBoneIndex))
			{
				break;
			}
			if (bIgnoreBone[ParentBoneIndex])
			{
				continue;
			}
			if (FirstParentBoneIndex < 0)
			{
				FirstParentBoneIndex = ParentBoneIndex;
			}
			else if (ParentBoneIndex == FirstParentBoneIndex)
			{
				return ParentBoneIndex;
			}
		}

		return -1;
	}

	bool ShouldMakeBone(bool bCreateBodyForAllBones, float MinBoneSize, const TArray<float>& MergedSizes, const TArray<bool>& bIgnoreBone, int32 ForcedRootBoneIndex, int32 BoneIndex)
	{
		if (bCreateBodyForAllBones)
		{
			return true;
		}
		if (bIgnoreBone[BoneIndex])
		{
			return false;
		}
		if (MergedSizes[BoneIndex] > MinBoneSize)
		{
			return true;
		}
		return BoneIndex == ForcedRootBoneIndex;
	}

	bool GeneratePhysicsAssetBodiesInternal(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, const FPhysicsAssetCreationParams& Params)
	{
		const int32 NumBones = static_cast<int32>(Mesh.Bones.size());
		if (NumBones == 0 || Mesh.Vertices.empty())
		{
			return false;
		}

		const float MeshLongDimension = CalcMeshLongDimension(Mesh);
		const float EffectiveMinBoneSize = ResolveCreationSize(Params.MinBoneSize, MeshLongDimension);
		const float EffectiveMinWeldSize = ResolveCreationSize(Params.MinWeldSize, MeshLongDimension);
		const float EffectiveMinPrimSize = CalcEffectiveMinPrimSize(MeshLongDimension);

		TArray<bool> bIgnoreBone;
		bIgnoreBone.resize(NumBones, false);
		const bool bUseSecondaryBoneFilter = Params.bFilterSecondaryBones
			&& !Params.bCreateBodyForAllBones
			&& LooksLikeHumanoidSkeleton(Mesh);
		if (bUseSecondaryBoneFilter)
		{
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				const FString& BoneName = Mesh.Bones[BoneIndex].Name;
				bIgnoreBone[BoneIndex] = IsSecondaryBoneName(BoneName) || !IsHumanoidBodyBoneName(BoneName);
			}
		}

		TArray<FBoneVertInfo> Infos;
		CalcBoneVertInfos(Mesh, Infos, Params.VertexWeighting == EPhysicsAssetVertexWeighting::DominantWeight);

		TArray<float> MergedSizes;
		MergedSizes.resize(NumBones, 0.0f);

		using FMergedBoneIndices = TArray<int32>;
		struct FMergedBoneInfo
		{
			FMergedBoneIndices BoneIndices;
			FBoneVertInfo Info;
		};
		TMap<int32, FMergedBoneInfo> BoneToMergedBones;

		for (int32 BoneIndex = NumBones - 1; BoneIndex >= 0; --BoneIndex)
		{
			if (bIgnoreBone[BoneIndex])
			{
				continue;
			}

			const float MyMergedSize = MergedSizes[BoneIndex] += CalcBoneInfoLength(Infos[BoneIndex]);
			if (MyMergedSize < EffectiveMinBoneSize && MyMergedSize >= EffectiveMinWeldSize)
			{
				const int32 ParentIndex = FindMergeParentIndex(Mesh, bIgnoreBone, BoneIndex);
				if (!IsValidBoneIndex(Mesh, ParentIndex))
				{
					continue;
				}

				MergedSizes[ParentIndex] += MyMergedSize;
				FMergedBoneInfo& ParentMergedInfo = BoneToMergedBones[ParentIndex];
				ParentMergedInfo.BoneIndices.push_back(BoneIndex);

				const FMatrix LocalToParent = GetBoneLocalToBoneLocalMatrix(Mesh, BoneIndex, ParentIndex);
				AddInfoToParentInfo(LocalToParent, Infos[BoneIndex], ParentMergedInfo.Info);

				auto MyMergedIt = BoneToMergedBones.find(BoneIndex);
				if (MyMergedIt != BoneToMergedBones.end())
				{
					ParentMergedInfo.BoneIndices.insert(
						ParentMergedInfo.BoneIndices.end(),
						MyMergedIt->second.BoneIndices.begin(),
						MyMergedIt->second.BoneIndices.end());
					AddInfoToParentInfo(LocalToParent, MyMergedIt->second.Info, ParentMergedInfo.Info);
					BoneToMergedBones.erase(MyMergedIt);
				}
			}
		}

		const int32 ForcedRootBoneIndex = Params.bCreateBodyForAllBones
			? -1
			: FindForcedRootBoneIndex(Mesh, MergedSizes, bIgnoreBone, EffectiveMinBoneSize);

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			if (!ShouldMakeBone(Params.bCreateBodyForAllBones, EffectiveMinBoneSize, MergedSizes, bIgnoreBone, ForcedRootBoneIndex, BoneIndex))
			{
				continue;
			}

			FBoneVertInfo BodyInfo = Infos[BoneIndex];
			auto MergedIt = BoneToMergedBones.find(BoneIndex);
			if (MergedIt != BoneToMergedBones.end())
			{
				BodyInfo.Positions.insert(
					BodyInfo.Positions.end(),
					MergedIt->second.Info.Positions.begin(),
					MergedIt->second.Info.Positions.end());
				BodyInfo.Normals.insert(
					BodyInfo.Normals.end(),
					MergedIt->second.Info.Normals.begin(),
					MergedIt->second.Info.Normals.end());
			}

			UBodySetup* Body = Asset.CreateBodySetup(FName(Mesh.Bones[BoneIndex].Name));
			if (!Body)
			{
				continue;
			}

			AddCollisionFromBoneInfo(*Body, Params.PrimitiveType, BodyInfo, Params.bAutoOrientToBone, EffectiveMinPrimSize);
			if (!Body->HasSimpleCollision())
			{
				Asset.RemoveBodySetup(Body->GetBoneName());
			}
		}

		return !Asset.GetBodySetups().empty();
	}

	EAngularConstraintMode MapAngularMode(EPhysicsAssetConstraintMode In)
	{
		switch (In)
		{
		case EPhysicsAssetConstraintMode::Free:
			return EAngularConstraintMode::Free;
		case EPhysicsAssetConstraintMode::Locked:
			return EAngularConstraintMode::Locked;
		case EPhysicsAssetConstraintMode::Limited:
		default:
			return EAngularConstraintMode::Limited;
		}
	}
}

void GeneratePhysicsAssetBodies(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, const FPhysicsAssetCreationParams& Params)
{
	if (GeneratePhysicsAssetBodiesInternal(Asset, Mesh, Params))
	{
		return;
	}

	FPhysicsAssetCreationParams RetryParams = Params;
	const float MeshLongDimension = CalcMeshLongDimension(Mesh);
	RetryParams.MinBoneSize = std::max(MeshLongDimension * 0.03f, CalcEffectiveMinPrimSize(MeshLongDimension) * 2.0f);
	Asset.Clear();
	GeneratePhysicsAssetBodiesInternal(Asset, Mesh, RetryParams);
}

void GeneratePhysicsAssetConstraints(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, const FPhysicsAssetCreationParams& Params)
{
	if (!Params.bCreateConstraints)
	{
		return;
	}

	const EAngularConstraintMode Mode = MapAngularMode(Params.AngularConstraintMode);

	TArray<FMatrix> ReferenceGlobals;
	BuildReferenceGlobalMatrices(Mesh, ReferenceGlobals);

	for (const UBodySetup* Body : Asset.GetBodySetups())
	{
		if (!Body)
		{
			continue;
		}

		const FName ChildBoneName = Body->GetBoneName();
		const int32 ChildIndex = FindBoneIndexByName(Mesh, ChildBoneName);
		if (!IsValidBoneIndex(Mesh, ChildIndex))
		{
			continue;
		}

		int32 ParentIndex = Mesh.Bones[ChildIndex].ParentIndex;
		while (IsValidBoneIndex(Mesh, ParentIndex))
		{
			const FName ParentBoneName(Mesh.Bones[ParentIndex].Name);
			if (Asset.FindBodySetup(ParentBoneName))
			{
				break;
			}
			ParentIndex = Mesh.Bones[ParentIndex].ParentIndex;
		}

		if (!IsValidBoneIndex(Mesh, ParentIndex))
		{
			continue;
		}

		const FMatrix ChildGlobal = GetReferenceGlobalMatrix(ReferenceGlobals, Mesh, ChildIndex);
		const FMatrix ParentGlobal = GetReferenceGlobalMatrix(ReferenceGlobals, Mesh, ParentIndex);

		const FTransform FrameA(ChildGlobal * ParentGlobal.GetInverse());
		const FTransform FrameB;
		Asset.CreateConstraint(
			FName(Mesh.Bones[ParentIndex].Name),
			ChildBoneName,
			FrameA,
			FrameB,
			Mode,
			Params.bDisableCollisionBetweenConstrainedBodies);
	}
}
