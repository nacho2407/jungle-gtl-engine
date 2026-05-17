#include "Mesh/Fbx/FbxSkinWeightImporter.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxTransformUtils.h"
#include "Mesh/Fbx/FbxMaterialImporter.h"
#include "Mesh/Fbx/FbxSkeletonImporter.h"
#include "Mesh/Fbx/FbxTangentBuilder.h"
#include "Core/Log.h"

#include <algorithm>
#include <cmath>

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
	Context.TangentSums.clear();
	Context.BitangentSums.clear();

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
		const int32 RigidBoneIndex = bHasSkin ? -1 : FFbxSkeletonImporter::FindNearestParentBoneIndex(Node, Context.BoneNodeToIndex);

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
			Context.Bones[BoneIndex].InverseBindPoseMatrix = FFbxTransformUtils::ToEngineMatrix(LinkBindMatrix).GetInverse();

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
			if (Weights.empty() && RigidBoneIndex >= 0)
			{
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
				Vertex.Position = FVector(static_cast<float>(CP[0]), static_cast<float>(CP[1]), static_cast<float>(CP[2]));

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
		MeshRange.MeshBindGlobal = MeshBindGlobal;
		if (MeshRange.VertexStart < MeshRange.VertexEnd && MeshRange.IndexCount > 0)
		{
			Context.SkeletalMeshRanges.push_back(MeshRange);
		}
	}

	const bool bImportedAnyGeometry = !Context.SkeletalVertices.empty() && !Context.SkeletalIndices.empty();
	if (!bImportedAnyGeometry && OutMessage)
	{
		*OutMessage = "FBX skeletal import failed: no skinned geometry imported.";
	}
	return bImportedAnyGeometry;
}
