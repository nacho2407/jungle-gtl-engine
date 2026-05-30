#include "PhysicsAssetGenerator.h"

#include "AssetFactory.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsConstraintTemplate.h"
#include "Math/Matrix.h"
#include "Core/Logging/Log.h"

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

	int32 LogCount = 0; // 진단용: 처음 몇 개 본만 로그

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
		const float LongestDim = std::max(Extents.X, std::max(Extents.Y, Extents.Z)) * 2.0f;

		if (LogCount < 6 && bHasEnoughVerts)
		{
			const FVector RawSample = Pts[0]; // 본로컬 좌표 한 점(스케일 감 잡기용)
			const FVector InvScale  = Mesh.Bones[BoneIndex].InverseBindPoseMatrix.GetScale();
			UE_LOG("[PhysGen] [1]Bone=%s verts=%llu | [2]InvBindScale=(%.3f,%.3f,%.3f) | [3]localMin=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f) sample=(%.2f,%.2f,%.2f) | [4]Extents=(%.2f,%.2f,%.2f) Longest=%.2f",
				Mesh.Bones[BoneIndex].Name.c_str(),
				static_cast<unsigned long long>(Pts.size()),
				InvScale.X, InvScale.Y, InvScale.Z,
				Min.X, Min.Y, Min.Z, Max.X, Max.Y, Max.Z,
				RawSample.X, RawSample.Y, RawSample.Z,
				Extents.X, Extents.Y, Extents.Z, LongestDim);
			++LogCount;
		}

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
			// 캡슐 축은 항상 가장 긴 AABB 축으로 잡는다(본 길이에 맞춰 눕힘).
				// 축을 Z로 강제하면 길쭉한 본(다리/팔)이 SegHalf<0 이 되어 sphere로 퇴화한다.
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
					Body -> AddSphere(Center, std::max(Radius, HalfLen));
				}
				else
				{
					const float Length = SegHalf * 2.0f;
					
					FVector AxisDir = FVector::ZAxisVector;
					if (Axis == 0) AxisDir = FVector::XAxisVector;
					else if (Axis == 1) AxisDir = FVector::YAxisVector;
					
					const FQuat Rotation = MakeQuatFromZToAxis(AxisDir);
					Body->AddSphyl(Center, Rotation, Radius, Length);
				}
			break;
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
		
		const FMatrix ChildGlobal = Mesh.Bones[ChildIdx].GetReferenceGlobalPose();
		const FMatrix ParentGlobal = Mesh.Bones[ParentIdx].GetReferenceGlobalPose();
		
		const FTransform FrameA(ChildGlobal * ParentGlobal.GetInverse()); // 부모 로컬에서 본 조인트
		const FTransform FrameB; // 자식 로컬 원점 = 조인트 -> Identity
		
		Asset.CreateConstraint(FName(Mesh.Bones[ParentIdx].Name), ChildBoneName, FrameA, FrameB, Mode);
	}
}
