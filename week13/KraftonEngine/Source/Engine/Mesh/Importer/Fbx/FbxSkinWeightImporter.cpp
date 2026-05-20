#include "Mesh/Importer/Fbx/FbxSkinWeightImporter.h"
#include "Mesh/Importer/Fbx/FbxSceneQuery.h"
#include "Mesh/Importer/Fbx/FbxTransformUtils.h"
#include "Mesh/Importer/Fbx/FbxMaterialImporter.h"
#include "Mesh/Importer/Fbx/FbxTangentBuilder.h"
#include "Core/Log.h"

#include <algorithm>
#include <cmath>
#include <utility>

struct FFbxSkeletalVertexKey
{
	int32 ControlPointIndex = -1;
	float NormalX = 0.0f;
	float NormalY = 0.0f;
	float NormalZ = 0.0f;
	float UVX = 0.0f;
	float UVY = 0.0f;

	bool operator==(const FFbxSkeletalVertexKey& Other) const
	{
		return ControlPointIndex == Other.ControlPointIndex
			&& NormalX == Other.NormalX
			&& NormalY == Other.NormalY
			&& NormalZ == Other.NormalZ
			&& UVX == Other.UVX
			&& UVY == Other.UVY;
	}
};

namespace std
{
template<>
struct hash<FFbxSkeletalVertexKey>
{
	size_t operator()(const FFbxSkeletalVertexKey& Key) const noexcept
	{
		size_t Result = std::hash<int32>()(Key.ControlPointIndex);
		auto Combine = [&Result](size_t Value)
		{
			Result ^= Value + 0x9e3779b9 + (Result << 6) + (Result >> 2);
		};

		Combine(std::hash<float>()(Key.NormalX));
		Combine(std::hash<float>()(Key.NormalY));
		Combine(std::hash<float>()(Key.NormalZ));
		Combine(std::hash<float>()(Key.UVX));
		Combine(std::hash<float>()(Key.UVY));
		return Result;
	}
};
}

namespace
{
	static void SetBoneInverseBindPose(FBone& Bone, const FMatrix& InverseBindPose)
	{
		Bone.InverseBindPoseMatrix = InverseBindPose;
		Bone.SkinBindGlobalPose = InverseBindPose.GetInverse();
		Bone.GlobalMatrix = Bone.SkinBindGlobalPose;
	}

	static int32 FindMappedBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& BoneNodeToIndex)
	{
		auto It = BoneNodeToIndex.find(Node);
		return It != BoneNodeToIndex.end() ? It->second : -1;
	}

	static int32 FindFirstDescendantBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& BoneNodeToIndex)
	{
		if (!Node)
		{
			return -1;
		}

		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			FbxNode* Child = Node->GetChild(ChildIndex);
			const int32 DirectBoneIndex = FindMappedBoneIndex(Child, BoneNodeToIndex);
			if (DirectBoneIndex >= 0)
			{
				return DirectBoneIndex;
			}

			const int32 DescendantBoneIndex = FindFirstDescendantBoneIndex(Child, BoneNodeToIndex);
			if (DescendantBoneIndex >= 0)
			{
				return DescendantBoneIndex;
			}
		}

		return -1;
	}

	static int32 FindFirstAncestorBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& BoneNodeToIndex)
	{
		for (FbxNode* Parent = Node ? Node->GetParent() : nullptr; Parent; Parent = Parent->GetParent())
		{
			const int32 BoneIndex = FindMappedBoneIndex(Parent, BoneNodeToIndex);
			if (BoneIndex >= 0)
			{
				return BoneIndex;
			}
		}

		return -1;
	}

	static int32 ResolveRigidAttachmentBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& BoneNodeToIndex)
	{
		const int32 DirectBoneIndex = FindMappedBoneIndex(Node, BoneNodeToIndex);
		if (DirectBoneIndex >= 0)
		{
			return DirectBoneIndex;
		}

		const int32 DescendantBoneIndex = FindFirstDescendantBoneIndex(Node, BoneNodeToIndex);
		if (DescendantBoneIndex >= 0)
		{
			return DescendantBoneIndex;
		}

		return FindFirstAncestorBoneIndex(Node, BoneNodeToIndex);
	}

	static FString GetFbxObjectName(const FbxObject* Object, const FString& Fallback)
	{
		const char* RawName = Object ? Object->GetName() : nullptr;
		return (RawName && RawName[0] != '\0') ? FString(RawName) : Fallback;
	}

	static FMorphTarget& FindOrAddMorphTarget(FFbxImportContext& Context, const FString& TargetName)
	{
		for (FMorphTarget& Target : Context.SkeletalMorphTargets)
		{
			if (Target.Name == TargetName)
			{
				return Target;
			}
		}

		FMorphTarget NewTarget;
		NewTarget.Name = TargetName.empty() ? FString("MorphTarget") : TargetName;
		Context.SkeletalMorphTargets.push_back(std::move(NewTarget));
		return Context.SkeletalMorphTargets.back();
	}

	static void AddOrReplaceMorphDelta(FMorphTarget& Target, uint32 VertexIndex, const FVector& PositionDelta)
	{
		if (PositionDelta.IsNearlyZero())
		{
			return;
		}

		for (FMorphTargetDelta& Delta : Target.Deltas)
		{
			if (Delta.VertexIndex == VertexIndex)
			{
				Delta.PositionDelta = PositionDelta;
				return;
			}
		}

		FMorphTargetDelta Delta;
		Delta.VertexIndex   = VertexIndex;
		Delta.PositionDelta = PositionDelta;
		Target.Deltas.push_back(Delta);
	}

	static void ImportMorphTargetsForMesh(
		FbxMesh*           Mesh,
		FFbxImportContext& Context,
		uint32             VertexStart,
		uint32             VertexEnd
		)
	{
		if (!Mesh || VertexStart >= VertexEnd)
		{
			return;
		}

		const int32 BlendShapeCount = Mesh->GetDeformerCount(FbxDeformer::eBlendShape);
		if (BlendShapeCount <= 0)
		{
			return;
		}

		VertexEnd = std::min<uint32>(VertexEnd, static_cast<uint32>(Context.SkeletalVertices.size()));

		for (int32 BlendShapeIndex = 0; BlendShapeIndex < BlendShapeCount; ++BlendShapeIndex)
		{
			FbxBlendShape* BlendShape = static_cast<FbxBlendShape*>(Mesh->GetDeformer(
				BlendShapeIndex,
				FbxDeformer::eBlendShape
			));
			if (!BlendShape)
			{
				continue;
			}

			const int32 ChannelCount = BlendShape->GetBlendShapeChannelCount();
			for (int32 ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
			{
				FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);
				if (!Channel || Channel->GetTargetShapeCount() <= 0)
				{
					continue;
				}

				FbxShape* Shape = Channel->GetTargetShape(Channel->GetTargetShapeCount() - 1);
				if (!Shape || Shape->GetControlPointsCount() != Mesh->GetControlPointsCount())
				{
					continue;
				}

				const FString TargetName = GetFbxObjectName(Channel, GetFbxObjectName(Shape, "MorphTarget"));
				FMorphTarget& Target     = FindOrAddMorphTarget(Context, TargetName);

				for (uint32 VertexIndex = VertexStart; VertexIndex < VertexEnd; ++VertexIndex)
				{
					if (VertexIndex >= Context.SkeletalMorphVertexSources.size())
					{
						continue;
					}

					const FFbxMorphVertexSource& Source = Context.SkeletalMorphVertexSources[VertexIndex];
					if (Source.Mesh != Mesh || !FFbxSceneQuery::IsValidControlPointIndex(
						Mesh,
						Source.ControlPointIndex
					))
					{
						continue;
					}

					const FbxVector4 ShapeCP = Shape->GetControlPointAt(Source.ControlPointIndex);
					FVector          TargetPosition(
						static_cast<float>(ShapeCP[0]),
						static_cast<float>(ShapeCP[1]),
						static_cast<float>(ShapeCP[2])
					);
					TargetPosition = Source.MeshBindGlobal.TransformPositionWithW(TargetPosition);
					if (Source.bUseRigidBindCorrection)
					{
						TargetPosition = Source.RigidBindCorrection.TransformPositionWithW(TargetPosition);
					}

					const FVector PositionDelta = TargetPosition - Context.SkeletalVertices[VertexIndex].Position;
					AddOrReplaceMorphDelta(Target, VertexIndex, PositionDelta);
				}
			}
		}
	}

	static void ImportClusterInverseBindPoses(FFbxImportContext& Context)
	{
		for (FbxNode* Node : Context.AllNodes)
		{
			FbxMesh* Mesh = Node ? Node->GetMesh() : nullptr;
			if (!Mesh)
			{
				continue;
			}

			const int32 DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
			for (int32 DeformerIndex = 0; DeformerIndex < DeformerCount; ++DeformerIndex)
			{
				FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(DeformerIndex, FbxDeformer::eSkin));
				if (!Skin)
				{
					continue;
				}

				for (int32 ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ++ClusterIndex)
				{
					FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
					FbxNode* LinkNode = Cluster ? Cluster->GetLink() : nullptr;
					auto BoneIt = Context.BoneNodeToIndex.find(LinkNode);
					if (BoneIt == Context.BoneNodeToIndex.end())
					{
						continue;
					}

					FbxAMatrix LinkBindMatrix;
					Cluster->GetTransformLinkMatrix(LinkBindMatrix);
					SetBoneInverseBindPose(Context.Bones[BoneIt->second], FFbxTransformUtils::ToEngineInverseMatrix(LinkBindMatrix));
				}
			}
		}
	}

	static FMatrix BuildRigidBindCorrection(int32 RigidBoneIndex, const TArray<FBone>& Bones)
	{
		if (RigidBoneIndex < 0 || RigidBoneIndex >= static_cast<int32>(Bones.size()))
		{
			return FMatrix::Identity;
		}

		// Rigid child meshes still pass through the bone skin matrix at runtime.
		// Store them in pre-skinned space so reference-pose skinning preserves their FBX mesh transform.
		const FMatrix ReferenceSkinMatrix =
			Bones[RigidBoneIndex].GetInverseBindPose() * Bones[RigidBoneIndex].GetReferenceGlobalPose();
		return ReferenceSkinMatrix.IsIdentity() ? FMatrix::Identity : ReferenceSkinMatrix.GetInverse();
	}

	static void NormalizeWeights(float* Weights, int32 Count)
	{
		float TotalWeight = 0.0f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			TotalWeight += Weights[Index];
		}

		if (TotalWeight > 0.0f)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Weights[Index] /= TotalWeight;
			}
		}
	}
}

bool FFbxSkinWeightImporter::ImportSkin(FbxScene* Scene, FFbxImportContext& Context, FString* OutMessage)
{
	(void)Scene;
	Context.SkeletalVertices.clear();
	Context.SkeletalIndices.clear();
	Context.SkeletalSections.clear();
	Context.SkeletalMeshRanges.clear();
	Context.SkeletalMorphTargets.clear();
	Context.SkeletalMorphVertexSources.clear();
	Context.TangentSums.clear();
	Context.BitangentSums.clear();

	ImportClusterInverseBindPoses(Context);

	for (FbxNode* Node : Context.AllNodes)
	{
		FbxMesh* Mesh = Node ? Node->GetMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}

		const int32 DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
		FbxSkin* Skin = DeformerCount > 0 ? static_cast<FbxSkin*>(Mesh->GetDeformer(0, FbxDeformer::eSkin)) : nullptr;
		const int32 ClusterCount = Skin ? Skin->GetClusterCount() : 0;
		const bool bHasSkin = Skin && ClusterCount > 0;
		const int32 RigidBoneIndex = bHasSkin ? -1 : ResolveRigidAttachmentBoneIndex(Node, Context.BoneNodeToIndex);
		const FMatrix RigidBindCorrection = (!bHasSkin && RigidBoneIndex >= 0)
			? BuildRigidBindCorrection(RigidBoneIndex, Context.Bones)
			: FMatrix::Identity;

		struct WeightData
		{
			int32 BoneIndex;
			float Weight;
		};

		TArray<TArray<WeightData>> TempWeights(Mesh->GetControlPointsCount());
		FbxAMatrix NodeGeometryTransform = FFbxTransformUtils::GetGeometryTransform(Node);
		FMatrix MeshBindGlobal = FFbxTransformUtils::ToEngineMatrix(Node->EvaluateGlobalTransform() * NodeGeometryTransform);
		bool bHasClusterMeshBindGlobal = false;

		for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
		{
			FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
			if (!Cluster)
			{
				continue;
			}

			FbxNode* LinkNode = Cluster->GetLink();
			if (!LinkNode)
			{
				continue;
			}

			auto BoneIt = Context.BoneNodeToIndex.find(LinkNode);
			if (BoneIt == Context.BoneNodeToIndex.end())
			{
				continue;
			}

			FbxAMatrix LinkBindMatrix;
			Cluster->GetTransformLinkMatrix(LinkBindMatrix);

			const int32 BoneIndex = BoneIt->second;
			SetBoneInverseBindPose(Context.Bones[BoneIndex], FFbxTransformUtils::ToEngineInverseMatrix(LinkBindMatrix));

			if (!bHasClusterMeshBindGlobal)
			{
				FbxAMatrix MeshBindMatrix;
				Cluster->GetTransformMatrix(MeshBindMatrix);
				MeshBindGlobal = FFbxTransformUtils::ToEngineMatrix(MeshBindMatrix);
				bHasClusterMeshBindGlobal = true;
			}

			int32* ControlPointIndices = Cluster->GetControlPointIndices();
			double* ControlPointWeights = Cluster->GetControlPointWeights();
			const int32 NumIndices = Cluster->GetControlPointIndicesCount();
			if (!ControlPointIndices || !ControlPointWeights || NumIndices <= 0)
			{
				continue;
			}

			for (int32 ControlPointWeightIndex = 0; ControlPointWeightIndex < NumIndices; ++ControlPointWeightIndex)
			{
				const int32 ControlPointIndex = ControlPointIndices[ControlPointWeightIndex];
				if (!FFbxSceneQuery::IsValidControlPointIndex(Mesh, ControlPointIndex))
				{
					continue;
				}

				const float Weight = static_cast<float>(ControlPointWeights[ControlPointWeightIndex]);
				if (Weight <= 0.0f)
				{
					continue;
				}

				TempWeights[ControlPointIndex].push_back({ BoneIndex, Weight });
			}
		}

		for (TArray<WeightData>& Weights : TempWeights)
		{
			if (!bHasSkin && RigidBoneIndex >= 0)
			{
				Weights.clear();
				Weights.push_back({ RigidBoneIndex, 1.0f });
			}

			if (Weights.empty())
			{
				continue;
			}

			std::sort(Weights.begin(), Weights.end(), [](const WeightData& A, const WeightData& B)
			{
				return A.Weight > B.Weight;
			});

			if (Weights.size() > 4)
			{
				Weights.resize(4);
			}
		}

		FbxStringList UVSetNames;
		Mesh->GetUVSetNames(UVSetNames);
		const char* UVName = (UVSetNames.GetCount() > 0) ? UVSetNames.GetStringAt(0) : nullptr;
		const bool bReverseWinding = FFbxTransformUtils::HasNegativeBasisDeterminant(MeshBindGlobal);

		TArray<int32> LocalToGlobalMaterialIndex;
		LocalToGlobalMaterialIndex.resize(Node->GetMaterialCount());
		for (int32 LocalIndex = 0; LocalIndex < Node->GetMaterialCount(); ++LocalIndex)
		{
			FbxSurfaceMaterial* Material = Node->GetMaterial(LocalIndex);
			auto MaterialIt = Context.MaterialToSlotIndex.find(Material);
			LocalToGlobalMaterialIndex[LocalIndex] = (MaterialIt != Context.MaterialToSlotIndex.end()) ? MaterialIt->second : -1;
		}

		TMap<int32, TArray<uint32>> SectionIndicesMap;
		TMap<FFbxSkeletalVertexKey, uint32> VertexMap;
		const uint32 VertexStart = static_cast<uint32>(Context.SkeletalVertices.size());
		const uint32 FirstIndex = static_cast<uint32>(Context.SkeletalIndices.size());

		for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
		{
			if (Mesh->GetPolygonSize(PolygonIndex) != 3)
			{
				continue;
			}

			const int32 LocalMaterialIndex = FFbxMaterialImporter::GetMaterialIndex(Mesh, PolygonIndex);
			int32 GlobalMaterialIndex = -1;
			if (LocalMaterialIndex >= 0 && LocalMaterialIndex < static_cast<int32>(LocalToGlobalMaterialIndex.size()))
			{
				GlobalMaterialIndex = LocalToGlobalMaterialIndex[LocalMaterialIndex];
			}

			uint32 TriIndices[3] = {};
			uint32 PendingSectionIndices[3] = {};
			bool bValidTriangle = true;

			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				FVertexPNCTBW Vertex;
				const int32 CPIndex = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);
				if (!FFbxSceneQuery::IsValidControlPointIndex(Mesh, CPIndex))
				{
					bValidTriangle = false;
					break;
				}

				FbxVector4 CP = Mesh->GetControlPointAt(CPIndex);
				const FVector LocalPosition(static_cast<float>(CP[0]), static_cast<float>(CP[1]), static_cast<float>(CP[2]));
				Vertex.Position = MeshBindGlobal.TransformPositionWithW(LocalPosition);
				if (!bHasSkin && RigidBoneIndex >= 0)
				{
					Vertex.Position = RigidBindCorrection.TransformPositionWithW(Vertex.Position);
				}

				const TArray<WeightData>& Weights = TempWeights[CPIndex];

				for (int32 WeightIndex = 0; WeightIndex < static_cast<int32>(Weights.size()) && WeightIndex < 4; ++WeightIndex)
				{
					Vertex.BoneIndices[WeightIndex] = Weights[WeightIndex].BoneIndex;
					Vertex.BoneWeights[WeightIndex] = Weights[WeightIndex].Weight;
				}

				NormalizeWeights(Vertex.BoneWeights, 4);

				FbxVector4 Normal;
				Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, Normal);
				Normal.Normalize();
				FVector N = FVector(static_cast<float>(Normal[0]), static_cast<float>(Normal[1]), static_cast<float>(Normal[2]));
				N = MeshBindGlobal.TransformVector(N);
				if (!bHasSkin && RigidBoneIndex >= 0)
				{
					N = RigidBindCorrection.TransformVector(N);
				}
				N.Normalize();
				Vertex.Normal = N;

				Vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
				Vertex.UV = FVector2(0.0f, 0.0f);
				if (UVName)
				{
					FbxVector2 UV;
					bool bUnmappedUV = false;
					const bool bSuccess = Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVName, UV, bUnmappedUV);
					if (bSuccess && !bUnmappedUV)
					{
						Vertex.UV = FVector2(static_cast<float>(UV[0]), 1.0f - static_cast<float>(UV[1]));
					}
				}

				FFbxSkeletalVertexKey Key;
				Key.ControlPointIndex = CPIndex;
				Key.NormalX = Vertex.Normal.X;
				Key.NormalY = Vertex.Normal.Y;
				Key.NormalZ = Vertex.Normal.Z;
				Key.UVX = Vertex.UV.X;
				Key.UVY = Vertex.UV.Y;

				uint32 VertexIndex = 0;
				auto VertexIt = VertexMap.find(Key);
				if (VertexIt != VertexMap.end())
				{
					VertexIndex = VertexIt->second;
				}
				else
				{
					VertexIndex = static_cast<uint32>(Context.SkeletalVertices.size());
					Context.SkeletalVertices.push_back(Vertex);

					FFbxMorphVertexSource MorphSource;
					MorphSource.Mesh                    = Mesh;
					MorphSource.ControlPointIndex       = CPIndex;
					MorphSource.MeshBindGlobal          = MeshBindGlobal;
					MorphSource.RigidBindCorrection     = RigidBindCorrection;
					MorphSource.bUseRigidBindCorrection = (!bHasSkin && RigidBoneIndex >= 0);
					Context.SkeletalMorphVertexSources.push_back(MorphSource);

					Context.TangentSums.push_back(FVector::ZeroVector);
					Context.BitangentSums.push_back(FVector::ZeroVector);
					VertexMap[Key] = VertexIndex;
				}

				TriIndices[CornerIndex] = VertexIndex;
				PendingSectionIndices[CornerIndex] = VertexIndex;
			}

			if (!bValidTriangle)
			{
				continue;
			}

			if (bReverseWinding)
			{
				std::swap(TriIndices[1], TriIndices[2]);
				std::swap(PendingSectionIndices[1], PendingSectionIndices[2]);
			}

			for (uint32 VertexIndex : PendingSectionIndices)
			{
				SectionIndicesMap[GlobalMaterialIndex].push_back(VertexIndex);
			}

			FFbxTangentBuilder::AccumulateSkeletalTriangle(Context, TriIndices);
		}

		FFbxTangentBuilder::BuildSkeletalTangentsForVertexRange(Context, VertexStart);

		uint32 CurrentBaseIndex = static_cast<uint32>(Context.SkeletalIndices.size());
		for (auto& Pair : SectionIndicesMap)
		{
			FSkeletalMeshSection Section;
			const int32 MatIndex = Pair.first;
			if (MatIndex >= 0 && MatIndex < static_cast<int32>(Context.Materials.size()))
			{
				Section.MaterialSlotName = Context.Materials[MatIndex].Name;
				Section.MaterialIndex = Pair.first;
			}
			else
			{
				UE_LOG("Warning: Material index %d out of range. Assigning to Default slot.", Pair.first);
				Section.MaterialSlotName = "None";
				Section.MaterialIndex = -1;
			}

			Section.FirstIndex = CurrentBaseIndex;
			Section.IndexCount = static_cast<uint32>(Pair.second.size());
			CurrentBaseIndex += Section.IndexCount;
			Context.SkeletalIndices.insert(Context.SkeletalIndices.end(), Pair.second.begin(), Pair.second.end());
			Context.SkeletalSections.push_back(Section);
		}

		FSkeletalMeshRange MeshRange;
		MeshRange.VertexStart = VertexStart;
		MeshRange.VertexEnd = static_cast<uint32>(Context.SkeletalVertices.size());
		MeshRange.FirstIndex = FirstIndex;
		MeshRange.IndexCount = static_cast<uint32>(Context.SkeletalIndices.size()) - FirstIndex;
		MeshRange.MeshBindGlobal = FMatrix::Identity;
		if (MeshRange.VertexStart < MeshRange.VertexEnd && MeshRange.IndexCount > 0)
		{
			Context.SkeletalMeshRanges.push_back(MeshRange);
			ImportMorphTargetsForMesh(Mesh, Context, MeshRange.VertexStart, MeshRange.VertexEnd);
		}
	}

	Context.SkeletalMorphTargets.erase(
		std::remove_if(
			Context.SkeletalMorphTargets.begin(),
			Context.SkeletalMorphTargets.end(),
			[](const FMorphTarget& Target)
			{
				return Target.Deltas.empty();
			}
		),
		Context.SkeletalMorphTargets.end()
	);

	const bool bImportedAnyGeometry = !Context.SkeletalVertices.empty() && !Context.SkeletalIndices.empty();
	if (!bImportedAnyGeometry && OutMessage)
	{
		*OutMessage = "FBX skeletal import failed: no skinned geometry imported.";
	}
	return bImportedAnyGeometry;
}
