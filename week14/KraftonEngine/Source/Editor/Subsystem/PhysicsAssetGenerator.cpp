#include "PhysicsAssetGenerator.h"

#include "AssetFactory.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsAsset.h"

namespace
{
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
}

void GeneratePhysicsAssetBodies(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, const FPhysicsAssetCreationParams& Params)
{
	const int32 NumBones = static_cast<int32>(Mesh.Bones.size());
	if (NumBones == 0 || Mesh.Vertices.empty())
	{
		return;
	}
	
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
	const float MinExtent = 0.5f;
	
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const TArray<FVector>& Pts = BoneLocalVerts[BoneIndex];
		
		const bool bHasEnoughVerts = Pts.size() >= 4;
		if (!bHasEnoughVerts && !Params.bCreateBodyForAllBones)
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
		else // 버텍스 부족 + bCreateBodyForAllBones : 작은 기본 박스
		{
			Min = FVector(-2.0f, -2.0f, -2.0f);
			Max = FVector( 2.0f,  2.0f,  2.0f);
		}
		
		const FVector Center = (Min + Max) * 0.5f;
		FVector Extents = (Max - Min) * 0.5f;
		Extents.X = std::max(Extents.X, MinExtent);
		Extents.Y = std::max(Extents.Y, MinExtent);
		Extents.Z = std::max(Extents.Z, MinExtent);
		
		// MinBoneSize 필터 : 가장 긴 축 기준.
		const float LongestDim = std::max(Extents.X, std::max(Extents.Y, Extents.Z)) * 2.0f;
		if (LongestDim < Params.MinBoneSize && !Params.bCreateBodyForAllBones)
		{
			continue;
		}
		
		UBodySetup* Body = Asset.CreateBodySetup(FName(Mesh.Bones[BoneIndex].Name));
		if (!Body)
		{
			continue;
		}
		
		switch (Params.PrimitiveType)
		{
		case EPhysicsAssetPrimitiveType::Box:
			Body->AddBox(Center, FQuat::Identity, Extents);
			break;
			
		case EPhysicsAssetPrimitiveType::Sphere:
			Body->AddSphere(Center, std::max(Extents.X, std::max(Extents.Y, Extents.Z)));
			break;
			
		case EPhysicsAssetPrimitiveType::Capsule:
		default:
		{
			int32 Axis = 2;
				if (Params.bAutoOrientToBone)
				{
					Axis = 0;
					if (Extents.Y > Extents.Data[Axis]) Axis = 1;
					if (Extents.Z > Extents.Data[Axis]) Axis = 2;
				}
				
				const float HalfLen = Extents.Data[Axis];
				const float Radius = (Axis == 0) ? std::max(Extents.Y, Extents.Z)
					: (Axis == 1) ? std::max(Extents.X, Extents.Z)
						: std::max(Extents.X, Extents.Y);
			
				const float SegHalf = HalfLen - Radius;
				if (SegHalf <= MinExtent)
				{
					Body -> AddSphere(Center, std::max(Radius, HalfLen));
				}
				else
				{
					const float Length = SegHalf * 2.0f;
					
					FVector AxisDir = FVector::ZAxisVector;
					if (Axis = 0) AxisDir = FVector::XAxisVector;
					else if (Axis == 1) AxisDir = FVector::YAxisVector;
					
					const FQuat Rotation = MakeQuatFromZToAxis(AxisDir);
					Body->AddSphyl(Center, Rotation, Radius, Length);
				}
			break;
		}
		}
	}
}
