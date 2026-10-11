#include "Mesh/Importer/Fbx/FbxStaticMeshImporter.h"
#include "Mesh/Importer/Fbx/FbxSceneQuery.h"
#include "Mesh/Importer/Fbx/FbxTransformUtils.h"
#include "Mesh/Importer/Fbx/FbxMaterialImporter.h"
#include "Mesh/Importer/Fbx/FbxTangentBuilder.h"
#include "Mesh/Importer/MeshImportOptions.h"
#include "Materials/MaterialManager.h"
#include "Core/Logging/Log.h"

#include <algorithm>
#include <cmath>

struct FFbxStaticVertexKey
{
	int32 ControlPointIndex = -1;
	float NormalX = 0.0f;
	float NormalY = 0.0f;
	float NormalZ = 0.0f;
	float UVX = 0.0f;
	float UVY = 0.0f;

	bool operator==(const FFbxStaticVertexKey& Other) const
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
struct hash<FFbxStaticVertexKey>
{
	size_t operator()(const FFbxStaticVertexKey& Key) const noexcept
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

bool FFbxStaticMeshImporter::Import(FbxScene* Scene, const FString& SourcePath, const FImportOptions* Options, FFbxImportContext& Context, FFbxStaticMeshImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxStaticMeshImportResult();
	Context.SourcePath = SourcePath;
	Context.AllNodes.clear();
	Context.MeshNodes.clear();

	FbxNode* RootNode = Scene ? Scene->GetRootNode() : nullptr;
	if (!RootNode)
	{
		if (OutMessage) *OutMessage = "FBX static mesh import failed: root node not found.";
		return false;
	}

	FFbxSceneQuery::CollectAllNodes(RootNode, Context.AllNodes);
	FFbxSceneQuery::CollectMeshNodes(RootNode, Context.MeshNodes);
	FFbxMaterialImporter::CollectMaterials(Scene, Context);
	FFbxMaterialImporter::BuildStaticMaterials(Context, OutResult.Materials);

	TArray<FVector> StaticTangentSums;
	TArray<FVector> StaticBitangentSums;
	bool bNeedsNoneSlot = OutResult.Materials.empty();

	for (FbxNode* Node : Context.AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		FbxMesh* Mesh = Node->GetMesh();
		if (!Mesh)
		{
			continue;
		}

		const int32                       SkinCount         = Mesh->GetDeformerCount(FbxDeformer::eSkin);
		FbxSkin*                          Skin              = SkinCount > 0 ? static_cast<FbxSkin*>(Mesh->GetDeformer(0, FbxDeformer::eSkin)) : nullptr;
		const bool                        bHasSkin          = Skin && Skin->GetClusterCount() > 0;
		const FImportOptions              EffectiveOptions  = Options ? *Options : FImportOptions::Default();
		const EStaticFbxSkinnedMeshPolicy SkinnedMeshPolicy = EffectiveOptions.StaticFbxSkinnedMeshPolicy;

		FbxAMatrix NodeGeometryTransform = FFbxTransformUtils::GetGeometryTransform(Node);
		FMatrix MeshToWorld = FFbxTransformUtils::ToEngineMatrix(Node->EvaluateGlobalTransform() * NodeGeometryTransform);

		if (bHasSkin)
		{
			switch (SkinnedMeshPolicy)
			{
			case EStaticFbxSkinnedMeshPolicy::Skip:
				continue;
			case EStaticFbxSkinnedMeshPolicy::ImportBindPoseAsStatic:
			{
				FbxAMatrix MeshBindMatrix;
				Skin->GetCluster(0)->GetTransformMatrix(MeshBindMatrix);
				MeshToWorld = FFbxTransformUtils::ToEngineMatrix(MeshBindMatrix);
				break;
			}
			}
		}

		FbxStringList UVSetNames;
		Mesh->GetUVSetNames(UVSetNames);
		const char* UVName = (UVSetNames.GetCount() > 0) ? UVSetNames.GetStringAt(0) : nullptr;
		const bool bReverseWinding = FFbxTransformUtils::HasNegativeBasisDeterminant(MeshToWorld);

		TArray<int32> LocalToGlobalMaterialIndex;
		LocalToGlobalMaterialIndex.resize(Node->GetMaterialCount());
		for (int32 LocalIndex = 0; LocalIndex < Node->GetMaterialCount(); ++LocalIndex)
		{
			FbxSurfaceMaterial* Material = Node->GetMaterial(LocalIndex);
			auto It = Context.MaterialToSlotIndex.find(Material);
			LocalToGlobalMaterialIndex[LocalIndex] = (It != Context.MaterialToSlotIndex.end()) ? It->second : -1;
		}

		TMap<int32, TArray<uint32>> SectionIndicesMap;
		TMap<FFbxStaticVertexKey, uint32> VertexMap;

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
				FNormalVertex Vertex;
				const int32 CPIndex = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);
				if (!FFbxSceneQuery::IsValidControlPointIndex(Mesh, CPIndex))
				{
					bValidTriangle = false;
					break;
				}

				FbxVector4 CP = Mesh->GetControlPointAt(CPIndex);
				Vertex.pos = MeshToWorld.TransformPositionWithW(FVector(static_cast<float>(CP[0]), static_cast<float>(CP[1]), static_cast<float>(CP[2])));

				FbxVector4 Normal;
				Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, Normal);
				Normal.Normalize();
				Vertex.normal = MeshToWorld.TransformVector(FVector(static_cast<float>(Normal[0]), static_cast<float>(Normal[1]), static_cast<float>(Normal[2])));
				if (!Vertex.normal.IsNearlyZero())
				{
					Vertex.normal.Normalize();
				}

				Vertex.color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
				Vertex.tex = FVector2(0.0f, 0.0f);
				if (UVName)
				{
					FbxVector2 UV;
					bool bUnmappedUV = false;
					const bool bSuccess = Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVName, UV, bUnmappedUV);
					if (bSuccess && !bUnmappedUV)
					{
						Vertex.tex = FVector2(static_cast<float>(UV[0]), 1.0f - static_cast<float>(UV[1]));
					}
				}

				FFbxStaticVertexKey Key;
				Key.ControlPointIndex = CPIndex;
				Key.NormalX = Vertex.normal.X;
				Key.NormalY = Vertex.normal.Y;
				Key.NormalZ = Vertex.normal.Z;
				Key.UVX = Vertex.tex.X;
				Key.UVY = Vertex.tex.Y;

				uint32 VertexIndex = 0;
				auto It = VertexMap.find(Key);
				if (It != VertexMap.end())
				{
					VertexIndex = It->second;
				}
				else
				{
					VertexIndex = static_cast<uint32>(OutResult.Mesh.Vertices.size());
					OutResult.Mesh.Vertices.push_back(Vertex);
					StaticTangentSums.push_back(FVector::ZeroVector);
					StaticBitangentSums.push_back(FVector::ZeroVector);
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

			FFbxTangentBuilder::AccumulateStaticTriangle(OutResult.Mesh, TriIndices, StaticTangentSums, StaticBitangentSums);
		}

		uint32 CurrentBaseIndex = static_cast<uint32>(OutResult.Mesh.Indices.size());
		for (auto& Pair : SectionIndicesMap)
		{
			FStaticMeshSection Section;
			const int32 MatIndex = Pair.first;
			if (MatIndex >= 0 && MatIndex < static_cast<int32>(Context.Materials.size()))
			{
				Section.MaterialSlotName = Context.Materials[MatIndex].Name;
				Section.MaterialIndex = MatIndex;
			}
			else
			{
				Section.MaterialSlotName = "None";
				Section.MaterialIndex = -1;
				bNeedsNoneSlot = true;
			}

			Section.FirstIndex = CurrentBaseIndex;
			Section.NumTriangles = static_cast<uint32>(Pair.second.size() / 3);
			CurrentBaseIndex += static_cast<uint32>(Pair.second.size());
			OutResult.Mesh.Indices.insert(OutResult.Mesh.Indices.end(), Pair.second.begin(), Pair.second.end());
			OutResult.Mesh.Sections.push_back(Section);
		}
	}

	if (bNeedsNoneSlot)
	{
		FStaticMaterial DefaultMaterial;
		DefaultMaterial.MaterialSlotName = "None";
		DefaultMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial("None");
		OutResult.Materials.push_back(DefaultMaterial);
		const int32 NoneMaterialIndex = static_cast<int32>(OutResult.Materials.size()) - 1;
		for (FStaticMeshSection& Section : OutResult.Mesh.Sections)
		{
			if (Section.MaterialSlotName == "None")
			{
				Section.MaterialIndex = NoneMaterialIndex;
			}
		}
	}

	FFbxTangentBuilder::FinalizeStaticTangents(OutResult.Mesh, StaticTangentSums, StaticBitangentSums);
	OutResult.Mesh.PathFileName = SourcePath;
	OutResult.SourceMaterials = Context.Materials;

	const bool bImportedAnyGeometry = !OutResult.Mesh.Vertices.empty() && !OutResult.Mesh.Indices.empty();
	if (!bImportedAnyGeometry && OutMessage)
	{
		*OutMessage = "FBX static mesh import failed: no geometry imported.";
	}
	return bImportedAnyGeometry;
}
