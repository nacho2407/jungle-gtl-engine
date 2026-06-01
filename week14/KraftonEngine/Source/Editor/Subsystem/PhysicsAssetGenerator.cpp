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
#include <vector>

namespace
{
	constexpr float AutoBodyMinRadiusScale = 0.08f;
	constexpr float AutoBodyMaxRadiusToLength = 0.25f;
	constexpr float AutoBodyLengthFromBone = 0.9f;
	constexpr float AutoBodyRadiusPercentile = 0.85f;

	// +Z 를 TargetAxis(단위벡터)로 돌리는 쿼터니언 만들기
	FQuat MakeQuatFromZToAxis(const FVector& TargetAxis)
	{
		const FVector Z = FVector::ZAxisVector;
		const float Dot = Z.Dot(TargetAxis);

		if (Dot > 0.9999f) // 이미 +Z 방향 → 회전 불필요
		{
			return FQuat::Identity;
		}
		if (Dot < -0.9999f) // 정반대(-Z) → X축 기준 180도
		{
			return FQuat::FromAxisAngle(FVector::XAxisVector, 3.14159265f);
		}

		const FVector Axis = Z.Cross(TargetAxis).Normalized();
		const float   Angle = acosf(std::clamp(Dot, -1.0f, 1.0f)); // 라디안
		return FQuat::FromAxisAngle(Axis, Angle);
	}
	
	// 본 이름 → 본 인덱스 (FName 비교는 대소문자 무시)
	int32 FindBoneIndexByName(const FSkeletalMesh& Mesh, FName BoneName)
	{
		for (int32 i = 0; i < static_cast<int32>(Mesh.Bones.size()); ++i)
		{
			if (FName(Mesh.Bones[i].Name) == BoneName)
			{
				return i;
			}
		}
		return -1;
	}

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

	bool EndsWithText(const FString& Value, const char* Token)
	{
		const size_t TokenLength = std::strlen(Token);
		return Value.size() >= TokenLength
			&& Value.compare(Value.size() - TokenLength, TokenLength, Token) == 0;
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

	bool IsHumanoidBodyBoneName(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		if (IsHumanoidDetailBoneName(Name))
		{
			return false;
		}

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
			|| ContainsText(Name, "hand")
			|| ContainsText(Name, "thigh")
			|| ContainsText(Name, "calf")
			|| ContainsText(Name, "shin")
			|| ContainsText(Name, "upleg")
			|| ContainsText(Name, "leg")
			|| ContainsText(Name, "foot");
	}

	bool IsHumanoidHeadBoneName(const FString& BoneName)
	{
		const FString Name = NormalizeBoneName(BoneName);
		return ContainsText(Name, "head") || ContainsText(Name, "skull");
	}

	bool LooksLikeHumanoidSkeleton(const FSkeletalMesh& Mesh)
	{
		bool bHasPelvis = false;
		bool bHasSpine = false;
		bool bHasHead = false;
		bool bHasArm = false;
		bool bHasLeg = false;

		for (const FBone& Bone : Mesh.Bones)
		{
			const FString Name = NormalizeBoneName(Bone.Name);
			bHasPelvis |= ContainsText(Name, "pelvis") || ContainsText(Name, "hips");
			bHasSpine |= ContainsText(Name, "spine") || ContainsText(Name, "chest");
			bHasHead |= ContainsText(Name, "head");
			bHasArm |= ContainsText(Name, "upperarm") || ContainsText(Name, "lowerarm") || ContainsText(Name, "forearm");
			bHasLeg |= ContainsText(Name, "thigh") || ContainsText(Name, "calf") || ContainsText(Name, "shin") || ContainsText(Name, "upleg");
		}

		return bHasPelvis && bHasSpine && bHasHead && bHasArm && bHasLeg;
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

	float Max3(float A, float B, float C)
	{
		return std::max(A, std::max(B, C));
	}

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return std::max(MinValue, std::min(Value, MaxValue));
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
	
	// Editor enum → Engine enum
	EAngularConstraintMode MapAngularMode(EPhysicsAssetConstraintMode In)
	{
		switch (In)
		{
		case EPhysicsAssetConstraintMode::Free:   return EAngularConstraintMode::Free;
		case EPhysicsAssetConstraintMode::Locked: return EAngularConstraintMode::Locked;
		case EPhysicsAssetConstraintMode::Limited:
		default:                                  return EAngularConstraintMode::Limited;
		}
	}
}

void GeneratePhysicsAssetBodies(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, const FPhysicsAssetCreationParams& Params)
{
	const int32 NumBones = static_cast<int32>(Mesh.Bones.size());
	if (NumBones == 0 || Mesh.Vertices.empty())
	{
		return;
	}

	TArray<FMatrix> ReferenceGlobals;
	BuildReferenceGlobalMatrices(Mesh, ReferenceGlobals);
	
	// 본별 버텍스 수집
	TArray<TArray<FVector>> BoneLocalVerts;
	BoneLocalVerts.resize(NumBones);
	
	for (const FVertexPNCTBW& Vertex : Mesh.Vertices)
	{	
		if (Params.VertexWeighting == EPhysicsAssetVertexWeighting::DominantWeight) // 가중치 가장 큰 놈 하나
		{
			int32 BestBone = -1;
			float BestWeight = 0.0f;
			for (int32 k = 0; k < 4; ++k)
			{
				if (Vertex.BoneIndices[k] >= 0 && Vertex.BoneWeights[k] > BestWeight)
				{
					BestWeight = Vertex.BoneWeights[k];
					BestBone = Vertex.BoneIndices[k];
				}
			}
			if (BestBone >= 0 && BestBone < NumBones)
			{
				const FVector Local = Mesh.Bones[BestBone].InverseBindPoseMatrix.TransformPositionWithW(Vertex.Position);
				BoneLocalVerts[BestBone].push_back(Local);
			}
		}
		else
		{
			for (int32 k = 0; k < 4; ++k)
			{
				const int32 BoneIdx = Vertex.BoneIndices[k];
				if (BoneIdx >= 0 && BoneIdx < NumBones && Vertex.BoneWeights[k] > 1.e-4f)
				{
					const FVector Local = Mesh.Bones[BoneIdx].InverseBindPoseMatrix.TransformPositionWithW(Vertex.Position);
					BoneLocalVerts[BoneIdx].push_back(Local);
				}
			}
		}
	}
	
	// 본별 도형 피팅

	// 최소 두께: 모델 전체 크기에 비례해서 잡는다.
	// 절대값(예: 0.5)은 m 단위 작은 모델에선 본보다 커서 도형이 거대해진다(이번 버그 원인).
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
	const float MinExtent = std::max(ModelSize * 0.005f, 1.e-4f); // 모델 대각의 0.5%
	const float EffectiveMinBoneSize = Params.MinBoneSize > 0.0f
		? std::min(Params.MinBoneSize, std::max(ModelSize * 0.08f, MinExtent * 2.0f))
		: 0.0f;
	const bool bUseHumanoidBodyFilter = false;

	TArray<TArray<FVector>> BodyLocalVerts = BoneLocalVerts;
	std::vector<uint8> bMergedIntoParent(static_cast<size_t>(NumBones), 0);
	if (!Params.bCreateBodyForAllBones && EffectiveMinBoneSize > 0.0f)
	{
		for (int32 BoneIndex = NumBones - 1; BoneIndex >= 0; --BoneIndex)
		{
			const float MergedSize = MeasurePointCloudSize(BodyLocalVerts[BoneIndex]);
			if (MergedSize <= 1.e-4f || MergedSize >= EffectiveMinBoneSize)
			{
				continue;
			}

			const int32 ParentIndex = Mesh.Bones[BoneIndex].ParentIndex;
			if (ParentIndex < 0 || ParentIndex >= NumBones)
			{
				continue;
			}

			const FMatrix BoneLocalToParentLocal =
				Mesh.Bones[BoneIndex].InverseBindPoseMatrix.GetInverse() * Mesh.Bones[ParentIndex].InverseBindPoseMatrix;
			AppendTransformedVerts(BodyLocalVerts[ParentIndex], BodyLocalVerts[BoneIndex], BoneLocalToParentLocal);
			BodyLocalVerts[BoneIndex].clear();
			bMergedIntoParent[static_cast<size_t>(BoneIndex)] = 1;
		}
	}

	TArray<float> BodyCandidateSizes;
	BodyCandidateSizes.resize(NumBones);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		BodyCandidateSizes[BoneIndex] = MeasurePointCloudSize(BodyLocalVerts[BoneIndex]);
	}

	std::vector<uint8> bForceMakeBone(static_cast<size_t>(NumBones), 0);
	if (!Params.bCreateBodyForAllBones)
	{
		int32 FirstParentBoneIndex = -1;
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			if (bMergedIntoParent[static_cast<size_t>(BoneIndex)]
				|| BodyCandidateSizes[BoneIndex] <= EffectiveMinBoneSize)
			{
				continue;
			}

			const int32 ParentBoneIndex = Mesh.Bones[BoneIndex].ParentIndex;
			if (ParentBoneIndex < 0)
			{
				break;
			}
			if (FirstParentBoneIndex < 0)
			{
				FirstParentBoneIndex = ParentBoneIndex;
				continue;
			}
			if (ParentBoneIndex == FirstParentBoneIndex && ParentBoneIndex < NumBones)
			{
				bForceMakeBone[static_cast<size_t>(ParentBoneIndex)] = 1;
				break;
			}
		}
	}

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const TArray<FVector>& Pts = BodyLocalVerts[BoneIndex];
		const bool bForcedBody = !Params.bCreateBodyForAllBones && bForceMakeBone[static_cast<size_t>(BoneIndex)] != 0;

		if (!Params.bCreateBodyForAllBones && IsLeadingSingleChildHelperBone(Mesh, BoneIndex))
		{
			continue;
		}
		if (!Params.bCreateBodyForAllBones && bMergedIntoParent[static_cast<size_t>(BoneIndex)] && !bForcedBody)
		{
			continue;
		}
		if (bUseHumanoidBodyFilter && !IsHumanoidBodyBoneName(Mesh.Bones[BoneIndex].Name))
		{
			continue;
		}
		
		const bool bHasEnoughVerts = Pts.size() >= 4;
		FVector BoneAxis = FVector::ZAxisVector;
		float BoneSegmentLength = 0.0f;
		const int32 DirectChildCount = Params.bAutoOrientToBone
			? FindChildSegmentLocal(Mesh, ReferenceGlobals, BoneIndex, Params.bWalkPastSmallBones, EffectiveMinBoneSize, BoneAxis, BoneSegmentLength)
			: 0;
		const bool bHasUsableBoneSegment = DirectChildCount == 1 && BoneSegmentLength > MinExtent * 2.0f;
		FVector ParentAwayAxis = FVector::ZAxisVector;
		float ParentSegmentLength = 0.0f;
		const bool bUseHumanoidHeadPrimitive = bUseHumanoidBodyFilter
			&& IsHumanoidHeadBoneName(Mesh.Bones[BoneIndex].Name)
			&& !bHasUsableBoneSegment
			&& FindParentSegmentLocal(Mesh, ReferenceGlobals, BoneIndex, ParentAwayAxis, ParentSegmentLength);

		if (!bHasEnoughVerts && !Params.bCreateBodyForAllBones && !bHasUsableBoneSegment && !bForcedBody)
		{
			continue;
		}
		
		// 본 로컬 AABB
		FVector Min, Max;
		if (bHasEnoughVerts)
		{
			Min = Pts[0];
			Max = Pts[0];
			for (const FVector& P : Pts)
			{
				Min.X = std::min(Min.X, P.X); Min.Y = std::min(Min.Y, P.Y); Min.Z = std::min(Min.Z, P.Z);
				Max.X = std::max(Max.X, P.X); Max.Y = std::max(Max.Y, P.Y); Max.Z = std::max(Max.Z, P.Z);
			}
		}
		else if (bHasUsableBoneSegment)
		{
			Min = FVector(-MinExtent, -MinExtent, -MinExtent);
			Max = BoneAxis * BoneSegmentLength + FVector(MinExtent, MinExtent, MinExtent);
		}
		else // 버텍스 부족 + bCreateBodyForAllBones : 작은 기본 박스(모델 비례)
		{
			const float D = ModelSize * 0.01f;
			Min = FVector(-D, -D, -D);
			Max = FVector( D,  D,  D);
		}
		
		const FVector Center = (Min + Max) * 0.5f;
		FVector Extents = (Max - Min) * 0.5f;
		Extents.X = std::max(Extents.X, MinExtent);
		Extents.Y = std::max(Extents.Y, MinExtent);
		Extents.Z = std::max(Extents.Z, MinExtent);
		
		// MinBoneSize 필터 : 가장 긴 축 기준.
		const float LongestDim = bHasEnoughVerts
			? std::max(Extents.X, std::max(Extents.Y, Extents.Z)) * 2.0f
			: BoneSegmentLength;

		if (LongestDim < EffectiveMinBoneSize && !Params.bCreateBodyForAllBones && !bForcedBody)
		{
			continue;
		}
		
		UBodySetup* Body = Asset.CreateBodySetup(FName(Mesh.Bones[BoneIndex].Name));
		if (!Body)
		{
			continue;
		}

		if (bUseHumanoidHeadPrimitive)
		{
			AddHumanoidHeadPrimitive(*Body, Params.PrimitiveType, ParentAwayAxis, ParentSegmentLength, ModelSize, MinExtent);
			continue;
		}
		
		switch (Params.PrimitiveType)
		{
		case EPhysicsAssetPrimitiveType::Box:
			if (bHasEnoughVerts)
			{
				Body->AddBox(Center, FQuat::Identity, Extents);
			}
			else
			{
				AddBoneSegmentPrimitive(*Body, Params.PrimitiveType, BoneAxis, BoneSegmentLength, MinExtent);
			}
			break;
			
		case EPhysicsAssetPrimitiveType::Sphere:
			if (bHasEnoughVerts)
			{
				Body->AddSphere(Center, Max3(Extents.X, Extents.Y, Extents.Z));
			}
			else
			{
				AddBoneSegmentPrimitive(*Body, Params.PrimitiveType, BoneAxis, BoneSegmentLength, MinExtent);
			}
			break;
			
		case EPhysicsAssetPrimitiveType::Capsule:
		default:
		{
			if (!bHasEnoughVerts && bHasUsableBoneSegment)
			{
				AddBoneSegmentPrimitive(*Body, Params.PrimitiveType, BoneAxis, BoneSegmentLength, MinExtent);
			}
			else if (bHasUsableBoneSegment)
			{
				AddBoneOrientedCapsule(*Body, Pts, BoneAxis, BoneSegmentLength, MinExtent);
			}
			else if (DirectChildCount > 1)
			{
				Body->AddBox(Center, FQuat::Identity, Extents);
			}
			else
			{
				AddAxisAlignedCapsule(*Body, Center, Extents, MinExtent);
			}
			break;
		}
		}
	}

	if (!Params.bCreateBodyForAllBones && Asset.GetBodySetups().empty())
	{
		std::vector<uint8> bFallbackMakeBone(static_cast<size_t>(NumBones), 0);

		auto MeasureBodyCandidate = [&](int32 BoneIndex, FVector* OutBoneAxis, float* OutBoneSegmentLength, int32* OutDirectChildCount) -> float
		{
			const TArray<FVector>& Pts = BodyLocalVerts[BoneIndex];
			float Score = 0.0f;

			if (Pts.size() >= 4)
			{
				FVector Min = Pts[0];
				FVector Max = Pts[0];
				for (const FVector& P : Pts)
				{
					Min.X = std::min(Min.X, P.X); Min.Y = std::min(Min.Y, P.Y); Min.Z = std::min(Min.Z, P.Z);
					Max.X = std::max(Max.X, P.X); Max.Y = std::max(Max.Y, P.Y); Max.Z = std::max(Max.Z, P.Z);
				}
				const FVector Extents = (Max - Min) * 0.5f;
				Score = Max3(Extents.X, Extents.Y, Extents.Z) * 2.0f;
			}

			FVector BoneAxis = FVector::ZAxisVector;
			float BoneSegmentLength = 0.0f;
			const int32 DirectChildCount = FindChildSegmentLocal(
				Mesh,
				ReferenceGlobals,
				BoneIndex,
				Params.bWalkPastSmallBones,
				MinExtent * 2.0f,
				BoneAxis,
				BoneSegmentLength);
			Score = std::max(Score, BoneSegmentLength);

			if (OutBoneAxis)
			{
				*OutBoneAxis = BoneAxis;
			}
			if (OutBoneSegmentLength)
			{
				*OutBoneSegmentLength = BoneSegmentLength;
			}
			if (OutDirectChildCount)
			{
				*OutDirectChildCount = DirectChildCount;
			}
			return Score;
		};

		auto MarkFallbackCandidates = [&](float MinBodySize, bool bKeepHumanoidBodyFilter, bool bAllowDetailBones, bool bForceBestCandidate) -> int32
		{
			int32 MarkedCount = 0;
			int32 BestBoneIndex = -1;
			float BestScore = 0.0f;

			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				if (IsLeadingSingleChildHelperBone(Mesh, BoneIndex))
				{
					continue;
				}

				const FString NormalizedBoneName = NormalizeBoneName(Mesh.Bones[BoneIndex].Name);
				if (bUseHumanoidBodyFilter && !bAllowDetailBones && IsHumanoidDetailBoneName(NormalizedBoneName))
				{
					continue;
				}
				if (bKeepHumanoidBodyFilter && bUseHumanoidBodyFilter && !IsHumanoidBodyBoneName(Mesh.Bones[BoneIndex].Name))
				{
					continue;
				}

				const float Score = MeasureBodyCandidate(BoneIndex, nullptr, nullptr, nullptr);

				if (Score > BestScore)
				{
					BestScore = Score;
					BestBoneIndex = BoneIndex;
				}

				if (MinBodySize > 0.0f && Score >= MinBodySize)
				{
					bFallbackMakeBone[static_cast<size_t>(BoneIndex)] = 1;
					++MarkedCount;
				}
			}

			if (MarkedCount == 0 && bForceBestCandidate && BestBoneIndex >= 0 && BestScore > 1.e-4f)
			{
				bFallbackMakeBone[static_cast<size_t>(BestBoneIndex)] = 1;
				MarkedCount = 1;
			}

			return MarkedCount;
		};

		auto MarkForcedRootBody = [&]()
		{
			int32 FirstParentBoneIndex = -1;
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				if (!bFallbackMakeBone[static_cast<size_t>(BoneIndex)])
				{
					continue;
				}

				const int32 ParentBoneIndex = Mesh.Bones[BoneIndex].ParentIndex;
				if (ParentBoneIndex < 0)
				{
					return;
				}

				if (FirstParentBoneIndex < 0)
				{
					FirstParentBoneIndex = ParentBoneIndex;
					continue;
				}

				if (ParentBoneIndex == FirstParentBoneIndex
					&& ParentBoneIndex < NumBones
					&& !IsLeadingSingleChildHelperBone(Mesh, ParentBoneIndex))
				{
					bFallbackMakeBone[static_cast<size_t>(ParentBoneIndex)] = 1;
					return;
				}
			}
		};

		const float RetryMinBoneSize = std::max(MinExtent * 2.0f, ModelSize * 0.01f);

		auto CreateFallbackBody = [&](int32 BoneIndex)
		{
			if (Asset.FindBodySetup(FName(Mesh.Bones[BoneIndex].Name)) != nullptr)
			{
				return;
			}

			const TArray<FVector>& Pts = BodyLocalVerts[BoneIndex];
			FVector BoneAxis = FVector::ZAxisVector;
			float BoneSegmentLength = 0.0f;
			const int32 DirectChildCount = Params.bAutoOrientToBone
				? FindChildSegmentLocal(Mesh, ReferenceGlobals, BoneIndex, Params.bWalkPastSmallBones, RetryMinBoneSize, BoneAxis, BoneSegmentLength)
				: 0;
			const bool bHasEnoughVerts = Pts.size() >= 4;
			const bool bHasUsableBoneSegment = DirectChildCount == 1 && BoneSegmentLength > MinExtent * 2.0f;

			if (!bHasEnoughVerts && !bHasUsableBoneSegment)
			{
				return;
			}

			FVector Min;
			FVector Max;
			if (bHasEnoughVerts)
			{
				Min = Pts[0];
				Max = Pts[0];
				for (const FVector& P : Pts)
				{
					Min.X = std::min(Min.X, P.X); Min.Y = std::min(Min.Y, P.Y); Min.Z = std::min(Min.Z, P.Z);
					Max.X = std::max(Max.X, P.X); Max.Y = std::max(Max.Y, P.Y); Max.Z = std::max(Max.Z, P.Z);
				}
			}
			else
			{
				Min = FVector(-MinExtent, -MinExtent, -MinExtent);
				Max = BoneAxis * BoneSegmentLength + FVector(MinExtent, MinExtent, MinExtent);
			}

			const FVector Center = (Min + Max) * 0.5f;
			FVector Extents = (Max - Min) * 0.5f;
			Extents.X = std::max(Extents.X, MinExtent);
			Extents.Y = std::max(Extents.Y, MinExtent);
			Extents.Z = std::max(Extents.Z, MinExtent);

			UBodySetup* Body = Asset.CreateBodySetup(FName(Mesh.Bones[BoneIndex].Name));
			if (!Body)
			{
				return;
			}

			FVector ParentAwayAxis = FVector::ZAxisVector;
			float ParentSegmentLength = 0.0f;
			if (IsHumanoidHeadBoneName(Mesh.Bones[BoneIndex].Name)
				&& FindParentSegmentLocal(Mesh, ReferenceGlobals, BoneIndex, ParentAwayAxis, ParentSegmentLength))
			{
				AddHumanoidHeadPrimitive(*Body, Params.PrimitiveType, ParentAwayAxis, ParentSegmentLength, ModelSize, MinExtent);
				return;
			}

			switch (Params.PrimitiveType)
			{
			case EPhysicsAssetPrimitiveType::Box:
				if (bHasEnoughVerts)
				{
					Body->AddBox(Center, FQuat::Identity, Extents);
				}
				else
				{
					AddBoneSegmentPrimitive(*Body, Params.PrimitiveType, BoneAxis, BoneSegmentLength, MinExtent);
				}
				break;

			case EPhysicsAssetPrimitiveType::Sphere:
				if (bHasEnoughVerts)
				{
					Body->AddSphere(Center, Max3(Extents.X, Extents.Y, Extents.Z));
				}
				else
				{
					AddBoneSegmentPrimitive(*Body, Params.PrimitiveType, BoneAxis, BoneSegmentLength, MinExtent);
				}
				break;

			case EPhysicsAssetPrimitiveType::Capsule:
			default:
				if (!bHasEnoughVerts && bHasUsableBoneSegment)
				{
					AddBoneSegmentPrimitive(*Body, Params.PrimitiveType, BoneAxis, BoneSegmentLength, MinExtent);
				}
				else if (bHasUsableBoneSegment)
				{
					AddBoneOrientedCapsule(*Body, Pts, BoneAxis, BoneSegmentLength, MinExtent);
				}
				else if (DirectChildCount > 1)
				{
					Body->AddBox(Center, FQuat::Identity, Extents);
				}
				else
				{
					AddAxisAlignedCapsule(*Body, Center, Extents, MinExtent);
				}
				break;
			}
		};

		int32 MarkedCount = MarkFallbackCandidates(RetryMinBoneSize, true, false, false);
		if (MarkedCount == 0)
		{
			MarkedCount = MarkFallbackCandidates(RetryMinBoneSize, false, false, false);
		}
		if (MarkedCount == 0)
		{
			MarkedCount = MarkFallbackCandidates(0.0f, false, true, true);
		}

		MarkForcedRootBody();

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			if (bFallbackMakeBone[static_cast<size_t>(BoneIndex)])
			{
				CreateFallbackBody(BoneIndex);
			}
		}
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
		const int32 ChildIdx = FindBoneIndexByName(Mesh, ChildBoneName);
		if (ChildIdx < 0)
		{
			continue;
		}
		
		int32 ParentIdx = Mesh.Bones[ChildIdx].ParentIndex;
		while (ParentIdx >= 0)
		{
			const FName ParentBoneName(Mesh.Bones[ParentIdx].Name);
			if (Asset.FindBodySetup(ParentBoneName) != nullptr)
			{
				break; // 바디 있는 조상 발견
			}
			ParentIdx = Mesh.Bones[ParentIdx].ParentIndex;
		}
		if (ParentIdx < 0)
		{
			continue; // 조상 바디 없음.
		}
		
		const FMatrix ChildGlobal = GetReferenceGlobalMatrix(ReferenceGlobals, Mesh, ChildIdx);
		const FMatrix ParentGlobal = GetReferenceGlobalMatrix(ReferenceGlobals, Mesh, ParentIdx);
		
		const FTransform FrameA(ChildGlobal * ParentGlobal.GetInverse()); // 부모 로컬에서 본 조인트
		const FTransform FrameB; // 자식 로컬 원점 = 조인트 -> Identity
		
		Asset.CreateConstraint(FName(Mesh.Bones[ParentIdx].Name), ChildBoneName, FrameA, FrameB, Mode);
	}
}
