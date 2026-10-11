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

	bool IsAlphaNumeric(char C)
	{
		return std::isalnum(static_cast<unsigned char>(C)) != 0;
	}

	bool IsLower(char C)
	{
		return std::islower(static_cast<unsigned char>(C)) != 0;
	}

	bool IsUpper(char C)
	{
		return std::isupper(static_cast<unsigned char>(C)) != 0;
	}

	bool IsDigit(char C)
	{
		return std::isdigit(static_cast<unsigned char>(C)) != 0;
	}

	bool ContainsBoneToken(const FString& BoneName, const char* Token)
	{
		const size_t TokenLength = std::strlen(Token);
		if (TokenLength == 0)
		{
			return false;
		}

		FString LowerName;
		LowerName.reserve(BoneName.size());
		for (char C : BoneName)
		{
			LowerName.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(C))));
		}

		const FString LowerToken(Token);
		size_t FoundIndex = LowerName.find(LowerToken);
		while (FoundIndex != FString::npos)
		{
			const size_t EndIndex = FoundIndex + TokenLength;
			const bool bStartBoundary = FoundIndex == 0
				|| !IsAlphaNumeric(BoneName[FoundIndex - 1])
				|| (IsLower(BoneName[FoundIndex - 1]) && IsUpper(BoneName[FoundIndex]));
			const bool bEndBoundary = EndIndex >= BoneName.size()
				|| !IsAlphaNumeric(BoneName[EndIndex])
				|| IsDigit(BoneName[EndIndex])
				|| (IsLower(BoneName[EndIndex - 1]) && IsUpper(BoneName[EndIndex]));

			if (bStartBoundary && bEndBoundary)
			{
				return true;
			}

			FoundIndex = LowerName.find(LowerToken, FoundIndex + 1);
		}

		return false;
	}

	bool IsHumanoidBodyBoneName(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return ContainsText(Name, "pelvis")
			|| ContainsText(Name, "hips")
			|| ContainsText(Name, "hip")
			|| ContainsText(Name, "base")
			|| ContainsText(Name, "waist")
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

	bool IsBodyFitHelperBoneName(const FString& BoneName)
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
			|| ContainsText(Name, "toe")
			|| ContainsText(Name, "twist")
			|| ContainsText(Name, "roll")
			|| ContainsText(Name, "wrist")
			|| ContainsText(Name, "sleeve");
	}

	bool IsDecorativeSecondaryBoneName(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return ContainsText(Name, "skirt")
			|| ContainsText(Name, "cloth")
			|| ContainsText(Name, "dress")
			|| ContainsText(Name, "hair")
			|| ContainsText(Name, "ribbon")
			|| ContainsText(Name, "accessory")
			|| ContainsText(Name, "breast")
			|| ContainsBoneToken(BoneName, "ear")
			|| ContainsText(Name, "eye")
			|| ContainsText(Name, "hat")
			|| ContainsText(Name, "width")
			|| StartsWithText(Name, "hb")
			|| StartsWithText(Name, "cf")
			|| StartsWithText(Name, "dm")
			|| StartsWithText(Name, "szy")
			|| StartsWithText(BoneName, "F_");
	}

	bool IsSecondaryBoneName(const FString& BoneName)
	{
		return IsBodyFitHelperBoneName(BoneName) || IsDecorativeSecondaryBoneName(BoneName);
	}

	bool IsHumanoidBodyCandidateBoneName(const FString& BoneName)
	{
		return IsHumanoidBodyBoneName(BoneName)
			&& !IsBodyFitHelperBoneName(BoneName)
			&& !IsDecorativeSecondaryBoneName(BoneName);
	}

	bool IsMajorHumanoidBodyBoneName(const FString& BoneName)
	{
		if (!IsHumanoidBodyCandidateBoneName(BoneName))
		{
			return false;
		}

		const FString Name = NormalizeBoneName(BoneName);
		return ContainsText(Name, "pelvis")
			|| ContainsText(Name, "hips")
			|| ContainsText(Name, "hip")
			|| ContainsText(Name, "base")
			|| ContainsText(Name, "waist")
			|| ContainsText(Name, "spine")
			|| ContainsText(Name, "chest")
			|| ContainsText(Name, "abdomen")
			|| ContainsText(Name, "head")
			|| ContainsText(Name, "upperarm")
			|| ContainsText(Name, "lowerarm")
			|| ContainsText(Name, "forearm")
			|| ContainsText(Name, "elbow")
			|| ContainsText(Name, "arm")
			|| ContainsText(Name, "thigh")
			|| ContainsText(Name, "calf")
			|| ContainsText(Name, "shin")
			|| ContainsText(Name, "upleg")
			|| ContainsText(Name, "leg")
			|| ContainsText(Name, "knee")
			|| ContainsText(Name, "ankle")
			|| ContainsText(Name, "foot");
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
			bHasHips |= ContainsText(Name, "pelvis") || ContainsText(Name, "hips") || ContainsText(Name, "hip")
				|| ContainsText(Name, "base") || ContainsText(Name, "waist");
			bHasSpine |= ContainsText(Name, "spine") || ContainsText(Name, "chest") || ContainsText(Name, "abdomen");
			bHasHead |= ContainsText(Name, "head");
			bHasArm |= ContainsText(Name, "upperarm") || ContainsText(Name, "lowerarm") || ContainsText(Name, "forearm") || ContainsText(Name, "arm");
			bHasLeg |= ContainsText(Name, "thigh") || ContainsText(Name, "calf") || ContainsText(Name, "shin")
				|| ContainsText(Name, "upleg") || ContainsText(Name, "leg") || ContainsText(Name, "foot");
		}

		return bHasHips && bHasSpine && bHasHead && bHasArm && bHasLeg;
	}

	bool IsVehicleWheelBoneName(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return ContainsText(Name, "wheel")
			|| ContainsText(Name, "tire")
			|| ContainsText(Name, "tyre")
			|| ContainsText(Name, "whl");
	}

	bool IsVehicleChassisBoneName(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return Name == "root"
			|| ContainsText(Name, "chassis")
			|| ContainsText(Name, "vehicle")
			|| ContainsText(Name, "carbody")
			|| ContainsText(Name, "body")
			|| ContainsText(Name, "hull");
	}

	bool IsVehicleSuspensionBoneName(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return ContainsText(Name, "axle")
			|| ContainsText(Name, "susp")
			|| ContainsText(Name, "strut")
			|| ContainsText(Name, "steer");
	}

	bool IsVehicleBodyCandidateBoneName(const FString& BoneName)
	{
		return IsVehicleWheelBoneName(BoneName)
			|| IsVehicleChassisBoneName(BoneName)
			|| IsVehicleSuspensionBoneName(BoneName);
	}

	bool LooksLikeVehicleSkeleton(const FSkeletalMesh& Mesh)
	{
		int32 WheelBoneCount = 0;
		int32 SuspensionBoneCount = 0;
		bool bHasChassis = false;

		for (const FBone& Bone : Mesh.Bones)
		{
			if (IsVehicleWheelBoneName(Bone.Name))
			{
				++WheelBoneCount;
			}
			if (IsVehicleSuspensionBoneName(Bone.Name))
			{
				++SuspensionBoneCount;
			}
			bHasChassis |= IsVehicleChassisBoneName(Bone.Name);
		}

		return WheelBoneCount >= 2
			|| (WheelBoneCount >= 1 && bHasChassis)
			|| (bHasChassis && SuspensionBoneCount >= 2);
	}

	EPhysicsAssetPrimitiveType SelectVehiclePrimitiveType(const FString& BoneName, EPhysicsAssetPrimitiveType RequestedType)
	{
		if (IsVehicleChassisBoneName(BoneName))
		{
			return EPhysicsAssetPrimitiveType::Box;
		}
		if (IsVehicleWheelBoneName(BoneName))
		{
			return EPhysicsAssetPrimitiveType::Capsule;
		}
		return RequestedType;
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

	int32 FindMergeParentIndex(const FSkeletalMesh& Mesh, const TArray<bool>& bCanCreateBody, int32 BoneIndex)
	{
		int32 ParentIndex = Mesh.Bones[BoneIndex].ParentIndex;
		while (IsValidBoneIndex(Mesh, ParentIndex) && !bCanCreateBody[ParentIndex])
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

	constexpr float AutoBodyMinRadiusScale = 0.08f;
	constexpr float AutoBodyMaxRadiusToLength = 0.25f;
	constexpr float AutoBodyLengthFromBone = 0.9f;
	constexpr float AutoBodyRadiusPercentile = 0.85f;

	int32 FindOnlyDirectChild(const FSkeletalMesh& Mesh, int32 BoneIndex)
	{
		int32 FoundChild = -1;
		for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(Mesh.Bones.size()); ++ChildIndex)
		{
			if (Mesh.Bones[ChildIndex].ParentIndex != BoneIndex)
			{
				continue;
			}
			if (FoundChild >= 0)
			{
				return -1;
			}
			FoundChild = ChildIndex;
		}
		return FoundChild;
	}

	int32 FindChildSegmentLocal(const FSkeletalMesh& Mesh, const TArray<FMatrix>& ReferenceGlobals, int32 BoneIndex, bool bWalkPastSmallBones, float MinSegmentLength, FVector& OutAxis, float& OutLength)
	{
		if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Mesh.Bones.size()))
		{
			return 0;
		}

		const FMatrix BoneRefInv = GetReferenceGlobalMatrix(ReferenceGlobals, Mesh, BoneIndex).GetInverse();
		float BestLength = 0.0f;
		FVector BestVector = FVector::ZeroVector;
		int32 BestChildIndex = -1;
		int32 ValidChildCount = 0;

		for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(Mesh.Bones.size()); ++ChildIndex)
		{
			if (Mesh.Bones[ChildIndex].ParentIndex != BoneIndex)
			{
				continue;
			}

			const FVector ChildLocal = BoneRefInv.TransformPositionWithW(GetReferenceGlobalMatrix(ReferenceGlobals, Mesh, ChildIndex).GetLocation());
			const float Length = ChildLocal.Length();
			if (Length <= 1.e-4f)
			{
				continue;
			}

			++ValidChildCount;
			if (Length > BestLength)
			{
				BestLength = Length;
				BestVector = ChildLocal;
				BestChildIndex = ChildIndex;
			}
		}

		if (bWalkPastSmallBones && ValidChildCount == 1 && BestLength > 1.e-4f && BestLength < MinSegmentLength)
		{
			int32 Current = BestChildIndex;
			for (int32 Depth = 0; Depth < 4 && Current >= 0; ++Depth)
			{
				const int32 Child = FindOnlyDirectChild(Mesh, Current);
				if (Child < 0)
				{
					break;
				}

				const FVector ChildLocal = BoneRefInv.TransformPositionWithW(GetReferenceGlobalMatrix(ReferenceGlobals, Mesh, Child).GetLocation());
				const float Length = ChildLocal.Length();
				if (Length > BestLength)
				{
					BestLength = Length;
					BestVector = ChildLocal;
					if (BestLength >= MinSegmentLength)
					{
						break;
					}
				}
				Current = Child;
			}
		}

		if (BestLength > 1.e-4f)
		{
			OutAxis = BestVector / BestLength;
			OutLength = BestLength;
		}
		return ValidChildCount;
	}

	int32 CountDirectChildren(const FSkeletalMesh& Mesh, int32 BoneIndex)
	{
		if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Mesh.Bones.size()))
		{
			return 0;
		}

		int32 Count = 0;
		for (const FBone& Bone : Mesh.Bones)
		{
			if (Bone.ParentIndex == BoneIndex)
			{
				++Count;
			}
		}
		return Count;
	}

	bool IsHumanoidDetailBoneName(const FString& NormalizedName)
	{
		return EndsWithText(NormalizedName, "end")
			|| ContainsText(NormalizedName, "finger")
			|| ContainsText(NormalizedName, "thumb")
			|| ContainsText(NormalizedName, "index")
			|| ContainsText(NormalizedName, "middle")
			|| ContainsText(NormalizedName, "ring")
			|| ContainsText(NormalizedName, "pinky")
			|| ContainsText(NormalizedName, "toe")
			|| ContainsText(NormalizedName, "twist")
			|| ContainsText(NormalizedName, "roll")
			|| ContainsText(NormalizedName, "skirt")
			|| ContainsText(NormalizedName, "cloth")
			|| ContainsText(NormalizedName, "dress")
			|| ContainsText(NormalizedName, "hair")
			|| ContainsText(NormalizedName, "ribbon")
			|| ContainsText(NormalizedName, "accessory");
	}

	bool IsAutoBodyTipMarkerBone(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return EndsWithText(Name, "end");
	}

	bool IsDeferredClothOrAccessoryBone(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return ContainsText(Name, "cloth")
			|| ContainsText(Name, "skirt")
			|| ContainsText(Name, "dress")
			|| ContainsText(Name, "cape")
			|| ContainsText(Name, "coat")
			|| ContainsText(Name, "sleeve")
			|| ContainsText(Name, "ribbon")
			|| ContainsText(Name, "accessory")
			|| ContainsText(Name, "hair");
	}

	bool IsExcludedAutoBodyBone(const FString& BoneName)
	{
		return IsAutoBodyTipMarkerBone(BoneName)
			|| IsDeferredClothOrAccessoryBone(BoneName);
	}

	bool IsHumanoidHeadBoneName(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return ContainsText(Name, "head") || ContainsText(Name, "skull");
	}

	bool IsLeadingSingleChildHelperBone(const FSkeletalMesh& Mesh, int32 BoneIndex)
	{
		if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Mesh.Bones.size()))
		{
			return false;
		}
		if (CountDirectChildren(Mesh, BoneIndex) != 1)
		{
			return false;
		}

		int32 ParentIndex = Mesh.Bones[BoneIndex].ParentIndex;
		while (ParentIndex >= 0)
		{
			if (CountDirectChildren(Mesh, ParentIndex) != 1)
			{
				return false;
			}
			ParentIndex = Mesh.Bones[ParentIndex].ParentIndex;
		}
		return true;
	}

	bool FindParentSegmentLocal(const FSkeletalMesh& Mesh, const TArray<FMatrix>& ReferenceGlobals, int32 BoneIndex, FVector& OutAwayFromParentAxis, float& OutLength)
	{
		if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Mesh.Bones.size()))
		{
			return false;
		}

		const int32 ParentIndex = Mesh.Bones[BoneIndex].ParentIndex;
		if (ParentIndex < 0 || ParentIndex >= static_cast<int32>(Mesh.Bones.size()))
		{
			return false;
		}

		const FMatrix BoneRefInv = GetReferenceGlobalMatrix(ReferenceGlobals, Mesh, BoneIndex).GetInverse();
		const FVector ParentLocal = BoneRefInv.TransformPositionWithW(GetReferenceGlobalMatrix(ReferenceGlobals, Mesh, ParentIndex).GetLocation());
		const float Length = ParentLocal.Length();
		if (Length <= 1.e-4f)
		{
			return false;
		}

		OutAwayFromParentAxis = (ParentLocal * -1.0f) / Length;
		OutLength = Length;
		return true;
	}

	float Percentile(std::vector<float>& Values, float Percent)
	{
		if (Values.empty())
		{
			return 0.0f;
		}

		std::sort(Values.begin(), Values.end());
		const int32 LastIndex = static_cast<int32>(Values.size()) - 1;
		const int32 Index = static_cast<int32>(ClampFloat(static_cast<float>(LastIndex) * Percent, 0.0f, static_cast<float>(LastIndex)));
		return Values[Index];
	}

	float MeasurePointCloudSize(const TArray<FVector>& Pts)
	{
		if (Pts.empty())
		{
			return 0.0f;
		}

		FVector Min = Pts[0];
		FVector Max = Pts[0];
		for (const FVector& P : Pts)
		{
			Min.X = std::min(Min.X, P.X);
			Min.Y = std::min(Min.Y, P.Y);
			Min.Z = std::min(Min.Z, P.Z);
			Max.X = std::max(Max.X, P.X);
			Max.Y = std::max(Max.Y, P.Y);
			Max.Z = std::max(Max.Z, P.Z);
		}

		const FVector Size = Max - Min;
		return Max3(Size.X, Size.Y, Size.Z);
	}

	void AppendTransformedVerts(TArray<FVector>& OutVerts, const TArray<FVector>& InVerts, const FMatrix& Transform)
	{
		if (InVerts.empty())
		{
			return;
		}

		OutVerts.reserve(OutVerts.size() + InVerts.size());
		for (const FVector& P : InVerts)
		{
			OutVerts.push_back(Transform.TransformPositionWithW(P));
		}
	}

	void AddAxisAlignedCapsule(UBodySetup& Body, const FVector& Center, const FVector& Extents, float MinExtent)
	{
		int32 Axis = 0;
		if (Extents.Y > Extents.Data[Axis]) Axis = 1;
		if (Extents.Z > Extents.Data[Axis]) Axis = 2;

		const float HalfLen = Extents.Data[Axis];
		const float Radius = (Axis == 0) ? std::max(Extents.Y, Extents.Z)
			: (Axis == 1) ? std::max(Extents.X, Extents.Z)
				: std::max(Extents.X, Extents.Y);

		const float SegHalf = HalfLen - Radius;
		if (SegHalf <= MinExtent)
		{
			Body.AddSphere(Center, std::max(Radius, HalfLen));
			return;
		}

		FVector AxisDir = FVector::ZAxisVector;
		if (Axis == 0) AxisDir = FVector::XAxisVector;
		else if (Axis == 1) AxisDir = FVector::YAxisVector;

		Body.AddSphyl(Center, MakeQuatFromZToAxis(AxisDir), Radius, SegHalf * 2.0f);
	}

	void AddBoneOrientedCapsule(UBodySetup& Body, const TArray<FVector>& Pts, const FVector& Axis, float BoneSegmentLength, float MinExtent)
	{
		float MinProj = 0.0f;
		float MaxProj = BoneSegmentLength;
		FVector PerpCenter = FVector::ZeroVector;

		if (!Pts.empty())
		{
			MinProj = Pts[0].Dot(Axis);
			MaxProj = MinProj;
			for (const FVector& P : Pts)
			{
				const float Projection = P.Dot(Axis);
				MinProj = std::min(MinProj, Projection);
				MaxProj = std::max(MaxProj, Projection);
				PerpCenter += P - Axis * Projection;
			}
			PerpCenter /= static_cast<float>(Pts.size());
		}

		const float VertexSpan = std::max(MaxProj - MinProj, MinExtent * 2.0f);
		const float UnclampedLength = std::max(VertexSpan, BoneSegmentLength * AutoBodyLengthFromBone);
		const float TargetTotalLength = ClampFloat(UnclampedLength, BoneSegmentLength * 0.45f, BoneSegmentLength * 1.15f);
		const float UnclampedCenterProj = VertexSpan >= BoneSegmentLength * 0.4f
			? (MinProj + MaxProj) * 0.5f
			: BoneSegmentLength * 0.5f;
		const float CenterProj = ClampFloat(UnclampedCenterProj, BoneSegmentLength * 0.15f, BoneSegmentLength * 0.85f);

		float Radius = TargetTotalLength * AutoBodyMinRadiusScale;
		std::vector<float> PerpDistances;
		PerpDistances.reserve(Pts.size());
		for (const FVector& P : Pts)
		{
			const float Projection = P.Dot(Axis);
			const FVector Perp = P - Axis * Projection - PerpCenter;
			PerpDistances.push_back(Perp.Length());
		}
		Radius = std::max(Radius, Percentile(PerpDistances, AutoBodyRadiusPercentile));

		Radius = ClampFloat(Radius, MinExtent, TargetTotalLength * AutoBodyMaxRadiusToLength);
		const float CylinderLength = std::max(TargetTotalLength - Radius * 2.0f, MinExtent);
		const FVector Center = PerpCenter + Axis * CenterProj;

		Body.AddSphyl(Center, MakeQuatFromZToAxis(Axis), Radius, CylinderLength);
	}

	void AddBoneSegmentPrimitive(UBodySetup& Body, EPhysicsAssetPrimitiveType PrimitiveType, const FVector& Axis, float BoneSegmentLength, float MinExtent)
	{
		const float TargetTotalLength = std::max(BoneSegmentLength * AutoBodyLengthFromBone, MinExtent * 4.0f);
		const float Radius = ClampFloat(TargetTotalLength * 0.12f, MinExtent, TargetTotalLength * AutoBodyMaxRadiusToLength);
		const FVector Center = Axis * (BoneSegmentLength * 0.5f);
		const FQuat Rotation = MakeQuatFromZToAxis(Axis);

		switch (PrimitiveType)
		{
		case EPhysicsAssetPrimitiveType::Box:
			Body.AddBox(Center, Rotation, FVector(Radius, Radius, TargetTotalLength * 0.5f));
			break;
		case EPhysicsAssetPrimitiveType::Sphere:
			Body.AddSphere(Center, std::max(Radius, TargetTotalLength * 0.25f));
			break;
		case EPhysicsAssetPrimitiveType::Capsule:
		default:
			Body.AddSphyl(Center, Rotation, Radius, std::max(TargetTotalLength - Radius * 2.0f, MinExtent));
			break;
		}
	}

	void AddHumanoidHeadPrimitive(UBodySetup& Body, EPhysicsAssetPrimitiveType PrimitiveType, const FVector& AwayFromParentAxis, float ParentSegmentLength, float ModelSize, float MinExtent)
	{
		const float Radius = ClampFloat(ParentSegmentLength * 0.45f, MinExtent * 2.0f, ModelSize * 0.08f);
		const FVector Center = AwayFromParentAxis * (Radius * 0.35f);

		if (PrimitiveType == EPhysicsAssetPrimitiveType::Box)
		{
			Body.AddBox(Center, FQuat::Identity, FVector(Radius * 0.9f, Radius * 0.8f, Radius));
			return;
		}

		Body.AddSphere(Center, Radius);
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

	bool AddMeshBoundsBoxForBone(UBodySetup& Body, const FSkeletalMesh& Mesh, int32 BoneIndex, float MinExtent)
	{
		if (!IsValidBoneIndex(Mesh, BoneIndex) || Mesh.Vertices.empty())
		{
			return false;
		}

		const FMatrix MeshToBone = GetBoneMeshToLocalMatrix(Mesh, BoneIndex);
		FVector Min = MeshToBone.TransformPositionWithW(Mesh.Vertices[0].Position);
		FVector Max = Min;
		for (const FVertexPNCTBW& Vertex : Mesh.Vertices)
		{
			const FVector Local = MeshToBone.TransformPositionWithW(Vertex.Position);
			Min.X = std::min(Min.X, Local.X);
			Min.Y = std::min(Min.Y, Local.Y);
			Min.Z = std::min(Min.Z, Local.Z);
			Max.X = std::max(Max.X, Local.X);
			Max.Y = std::max(Max.Y, Local.Y);
			Max.Z = std::max(Max.Z, Local.Z);
		}

		FVector Extent = (Max - Min) * 0.5f;
		Extent.X = std::max(Extent.X, MinExtent * 2.0f);
		Extent.Y = std::max(Extent.Y, MinExtent * 2.0f);
		Extent.Z = std::max(Extent.Z, MinExtent * 2.0f);

		Body.ClearShapes();
		Body.AddBox((Min + Max) * 0.5f, FQuat::Identity, Extent * ShapeSizePadding);
		return Body.HasSimpleCollision();
	}

	bool AddVehicleWheelPrimitive(UBodySetup& Body, float ModelSize, float MinExtent)
	{
		const float Radius = ClampFloat(ModelSize * 0.045f, MinExtent * 2.0f, ModelSize * 0.12f);
		const float Width = std::max(Radius * 0.65f, MinExtent * 2.0f);

		Body.ClearShapes();
		Body.AddSphyl(FVector::ZeroVector, MakeQuatFromZToAxis(FVector::YAxisVector), Radius, Width);
		return Body.HasSimpleCollision();
	}

	bool AddReferenceBodyPrimitive(UBodySetup& Body, const FSkeletalMesh& Mesh, const TArray<FMatrix>& ReferenceGlobals,
		int32 BoneIndex, EPhysicsAssetPrimitiveType PrimitiveType, const FPhysicsAssetCreationParams& Params,
		float EffectiveMinBoneSize, float MinExtent, float ModelSize)
	{
		FVector BoneAxis = FVector::ZAxisVector;
		float BoneSegmentLength = 0.0f;
		const float SegmentThreshold = std::max(EffectiveMinBoneSize, MinExtent * 2.0f);
		const int32 ChildCount = FindChildSegmentLocal(
			Mesh,
			ReferenceGlobals,
			BoneIndex,
			Params.bWalkPastSmallBones,
			SegmentThreshold,
			BoneAxis,
			BoneSegmentLength);

		if (ChildCount <= 0 || BoneSegmentLength < MinExtent * 2.0f)
		{
			if (!FindParentSegmentLocal(Mesh, ReferenceGlobals, BoneIndex, BoneAxis, BoneSegmentLength))
			{
				BoneAxis = FVector::ZAxisVector;
				BoneSegmentLength = std::max(ModelSize * 0.08f, MinExtent * 4.0f);
			}
		}

		Body.ClearShapes();
		AddBoneSegmentPrimitive(
			Body,
			PrimitiveType,
			BoneAxis,
			std::max(BoneSegmentLength, MinExtent * 4.0f),
			MinExtent);
		return Body.HasSimpleCollision();
	}

	bool AddVehicleReferencePrimitive(UBodySetup& Body, const FSkeletalMesh& Mesh, const TArray<FMatrix>& ReferenceGlobals,
		int32 BoneIndex, EPhysicsAssetPrimitiveType PrimitiveType, const FPhysicsAssetCreationParams& Params,
		float EffectiveMinBoneSize, float MinExtent, float ModelSize)
	{
		const FString& BoneName = Mesh.Bones[BoneIndex].Name;
		if (IsVehicleChassisBoneName(BoneName) && AddMeshBoundsBoxForBone(Body, Mesh, BoneIndex, MinExtent))
		{
			return true;
		}
		if (IsVehicleWheelBoneName(BoneName) && AddVehicleWheelPrimitive(Body, ModelSize, MinExtent))
		{
			return true;
		}
		return AddReferenceBodyPrimitive(
			Body,
			Mesh,
			ReferenceGlobals,
			BoneIndex,
			PrimitiveType,
			Params,
			EffectiveMinBoneSize,
			MinExtent,
			ModelSize);
	}

	int32 FindForcedRootBoneIndex(const FSkeletalMesh& Mesh, const TArray<float>& MergedSizes, const TArray<bool>& bCanCreateBody, float MinBoneSize)
	{
		int32 FirstParentBoneIndex = -1;
		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(MergedSizes.size()); ++BoneIndex)
		{
			if (!bCanCreateBody[BoneIndex] || MergedSizes[BoneIndex] <= MinBoneSize)
			{
				continue;
			}

			const int32 ParentBoneIndex = Mesh.Bones[BoneIndex].ParentIndex;
			if (!IsValidBoneIndex(Mesh, ParentBoneIndex))
			{
				break;
			}
			if (!bCanCreateBody[ParentBoneIndex])
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

	int32 AddReferenceSkeletonFallbackBodies(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, const TArray<FMatrix>& ReferenceGlobals,
		const FPhysicsAssetCreationParams& Params, float EffectiveMinBoneSize, float MinExtent, float ModelSize)
	{
		const int32 NumBones = static_cast<int32>(Mesh.Bones.size());
		const float SegmentThreshold = std::max(EffectiveMinBoneSize, MinExtent * 2.0f);
		int32 CreatedCount = 0;
		int32 FirstUsableBone = -1;

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			if (IsExcludedAutoBodyBone(Mesh.Bones[BoneIndex].Name))
			{
				continue;
			}

			if (FirstUsableBone < 0)
			{
				FirstUsableBone = BoneIndex;
			}

			if (IsLeadingSingleChildHelperBone(Mesh, BoneIndex))
			{
				continue;
			}

			FVector BoneAxis = FVector::ZAxisVector;
			float BoneSegmentLength = 0.0f;
			const int32 ChildCount = FindChildSegmentLocal(
				Mesh,
				ReferenceGlobals,
				BoneIndex,
				Params.bWalkPastSmallBones,
				SegmentThreshold,
				BoneAxis,
				BoneSegmentLength);

			if (ChildCount <= 0 || BoneSegmentLength < SegmentThreshold)
			{
				continue;
			}

			if (UBodySetup* Body = Asset.CreateBodySetup(FName(Mesh.Bones[BoneIndex].Name)))
			{
				AddBoneSegmentPrimitive(*Body, Params.PrimitiveType, BoneAxis, BoneSegmentLength, MinExtent);
				++CreatedCount;
			}
		}

		if (CreatedCount == 0 && FirstUsableBone >= 0)
		{
			FVector Axis = FVector::ZAxisVector;
			float Length = std::max(ModelSize * 0.1f, MinExtent * 4.0f);
			FVector ParentAwayAxis = FVector::ZAxisVector;
			float ParentLength = 0.0f;
			if (FindParentSegmentLocal(Mesh, ReferenceGlobals, FirstUsableBone, ParentAwayAxis, ParentLength))
			{
				Axis = ParentAwayAxis;
				Length = std::max(ParentLength, Length);
			}

			if (UBodySetup* Body = Asset.CreateBodySetup(FName(Mesh.Bones[FirstUsableBone].Name)))
			{
				AddBoneSegmentPrimitive(*Body, Params.PrimitiveType, Axis, Length, MinExtent);
				++CreatedCount;
			}
		}

		return CreatedCount;
	}

	bool ShouldMakeBone(
		bool bCreateBodyForAllBones,
		float MinBoneSize,
		const TArray<float>& MergedSizes,
		const TArray<bool>& bCanCreateBody,
		const TArray<bool>& bForceMakeBody,
		int32 ForcedRootBoneIndex,
		int32 BoneIndex)
	{
		if (bCreateBodyForAllBones)
		{
			return true;
		}
		if (!bCanCreateBody[BoneIndex])
		{
			return false;
		}
		if (bForceMakeBody[BoneIndex] && MergedSizes[BoneIndex] > KINDA_SMALL_NUMBER)
		{
			return true;
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

		TArray<bool> bCanCreateBody;
		bCanCreateBody.resize(NumBones, true);
		TArray<bool> bContributesToBodyFit;
		bContributesToBodyFit.resize(NumBones, true);
		TArray<bool> bForceMakeBody;
		bForceMakeBody.resize(NumBones, false);
		const bool bLooksLikeVehicle = LooksLikeVehicleSkeleton(Mesh);
		const bool bUseSecondaryBoneFilter = Params.bFilterSecondaryBones
			&& !Params.bCreateBodyForAllBones
			&& !bLooksLikeVehicle
			&& LooksLikeHumanoidSkeleton(Mesh);
		if (bUseSecondaryBoneFilter)
		{
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				const FString& BoneName = Mesh.Bones[BoneIndex].Name;
				const bool bDecorative = IsDecorativeSecondaryBoneName(BoneName);
				const bool bFitHelper = IsBodyFitHelperBoneName(BoneName) && !bDecorative;
				const bool bBodyCandidate = IsHumanoidBodyCandidateBoneName(BoneName);

				bCanCreateBody[BoneIndex] = bBodyCandidate;
				bContributesToBodyFit[BoneIndex] = bBodyCandidate || bFitHelper;
				bForceMakeBody[BoneIndex] = bBodyCandidate && IsMajorHumanoidBodyBoneName(BoneName);
			}
		}
		else if (bLooksLikeVehicle && !Params.bCreateBodyForAllBones)
		{
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				const FString& BoneName = Mesh.Bones[BoneIndex].Name;
				const bool bVehicleCandidate = IsVehicleBodyCandidateBoneName(BoneName);
				bCanCreateBody[BoneIndex] = bVehicleCandidate;
				bContributesToBodyFit[BoneIndex] = true;
				bForceMakeBody[BoneIndex] = bVehicleCandidate;
			}
		}

		TArray<FBoneVertInfo> Infos;
		CalcBoneVertInfos(Mesh, Infos, Params.VertexWeighting == EPhysicsAssetVertexWeighting::DominantWeight);

		TArray<FMatrix> ReferenceGlobals;
		if (bLooksLikeVehicle)
		{
			BuildReferenceGlobalMatrices(Mesh, ReferenceGlobals);
		}

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
			if (!bContributesToBodyFit[BoneIndex])
			{
				continue;
			}

			const float MyMergedSize = MergedSizes[BoneIndex] += CalcBoneInfoLength(Infos[BoneIndex]);
			const bool bMustMergeToBodyParent = !bCanCreateBody[BoneIndex];
			const bool bShouldMergeBySize = !Params.bCreateBodyForAllBones
				&& !bForceMakeBody[BoneIndex]
				&& MyMergedSize < EffectiveMinBoneSize
				&& MyMergedSize >= EffectiveMinWeldSize;

			if (bMustMergeToBodyParent || bShouldMergeBySize)
			{
				const int32 ParentIndex = FindMergeParentIndex(Mesh, bCanCreateBody, BoneIndex);
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

		if (bLooksLikeVehicle)
		{
			const float ForcedVehicleSize = std::max(EffectiveMinBoneSize + KINDA_SMALL_NUMBER, EffectiveMinPrimSize * 4.0f);
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				if (bForceMakeBody[BoneIndex] && MergedSizes[BoneIndex] <= KINDA_SMALL_NUMBER)
				{
					MergedSizes[BoneIndex] = ForcedVehicleSize;
				}
			}
		}

		const int32 ForcedRootBoneIndex = Params.bCreateBodyForAllBones
			? -1
			: FindForcedRootBoneIndex(Mesh, MergedSizes, bCanCreateBody, EffectiveMinBoneSize);

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			if (!ShouldMakeBone(
				Params.bCreateBodyForAllBones,
				EffectiveMinBoneSize,
				MergedSizes,
				bCanCreateBody,
				bForceMakeBody,
				ForcedRootBoneIndex,
				BoneIndex))
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

			const EPhysicsAssetPrimitiveType PrimitiveType = bLooksLikeVehicle
				? SelectVehiclePrimitiveType(Mesh.Bones[BoneIndex].Name, Params.PrimitiveType)
				: Params.PrimitiveType;

			if (bLooksLikeVehicle && BodyInfo.Positions.empty())
			{
				AddVehicleReferencePrimitive(
					*Body,
					Mesh,
					ReferenceGlobals,
					BoneIndex,
					PrimitiveType,
					Params,
					EffectiveMinBoneSize,
					EffectiveMinPrimSize,
					MeshLongDimension);
			}
			else
			{
				AddCollisionFromBoneInfo(*Body, PrimitiveType, BodyInfo, Params.bAutoOrientToBone, EffectiveMinPrimSize);
			}

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
	if (Mesh.Bones.empty() || Mesh.Vertices.empty())
	{
		return;
	}

	if (GeneratePhysicsAssetBodiesInternal(Asset, Mesh, Params))
	{
		return;
	}

	FPhysicsAssetCreationParams RetryParams = Params;
	const float MeshLongDimension = CalcMeshLongDimension(Mesh);
	RetryParams.MinBoneSize = std::max(MeshLongDimension * 0.03f, CalcEffectiveMinPrimSize(MeshLongDimension) * 2.0f);
	Asset.Clear();
	if (GeneratePhysicsAssetBodiesInternal(Asset, Mesh, RetryParams))
	{
		return;
	}

	if (!Params.bCreateBodyForAllBones)
	{
		TArray<FMatrix> ReferenceGlobals;
		BuildReferenceGlobalMatrices(Mesh, ReferenceGlobals);

		FVector ModelMin = Mesh.Vertices[0].Position;
		FVector ModelMax = Mesh.Vertices[0].Position;
		for (const FVertexPNCTBW& V : Mesh.Vertices)
		{
			ModelMin.X = std::min(ModelMin.X, V.Position.X);
			ModelMin.Y = std::min(ModelMin.Y, V.Position.Y);
			ModelMin.Z = std::min(ModelMin.Z, V.Position.Z);
			ModelMax.X = std::max(ModelMax.X, V.Position.X);
			ModelMax.Y = std::max(ModelMax.Y, V.Position.Y);
			ModelMax.Z = std::max(ModelMax.Z, V.Position.Z);
		}
		const float ModelSize = (ModelMax - ModelMin).Length();
		const float MinExtent = std::max(ModelSize * 0.005f, 1.e-4f);
		const float EffectiveMinBoneSize = Params.MinBoneSize > 0.0f
			? std::min(Params.MinBoneSize, std::max(ModelSize * 0.08f, MinExtent * 2.0f))
			: 0.0f;

		AddReferenceSkeletonFallbackBodies(Asset, Mesh, ReferenceGlobals, Params, EffectiveMinBoneSize, MinExtent, ModelSize);
	}
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
