#include "FbxImporter.h"
#include "Platform/Paths.h"
#include "Core/Log.h"

#include <filesystem>
#include <fstream>

TArray<FBone> FFbxImporter::Bones;
TArray<FVertexPNCTBW> FFbxImporter::Vertices;
TArray<uint32> FFbxImporter::Indices;
TArray<FSkeletalMeshSection> FFbxImporter::Sections;
TArray<FSkeletalMeshRange> FFbxImporter::MeshRanges;
TArray<FFbxImporter::FMaterialInfo> FFbxImporter::MtlInfos;
TArray<FSkeletalMaterial> FFbxImporter::SkeletalMaterials;
TArray<FVector> FFbxImporter::TangentSums;
TArray<FVector> FFbxImporter::BitangentSums;
TMap<FbxSurfaceMaterial*, int32> FFbxImporter::MaterialToSlotIndex;


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

bool FFbxImporter::Import(const FString& FilePath)
{
	FbxManager* SdkManager = FbxManager::Create();

	FbxIOSettings* ios = FbxIOSettings::Create(SdkManager, IOSROOT);
	SdkManager->SetIOSettings(ios);

	FbxScene* Scene = FbxScene::Create(SdkManager, "My Scene");

	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");

	FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(FilePath)));

	if (!Importer->Initialize(FullPath.c_str(), -1, SdkManager->GetIOSettings()))
	{
		return false;
	}

	Importer->Import(Scene);
	Importer->Destroy();

	// 임의로 m 변환. UE는 cm 단위
	FbxSystemUnit::m.ConvertScene(Scene);

	FbxAxisSystem UnrealAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
	UnrealAxisSystem.DeepConvertScene(Scene);

	TriangulateScene(Scene);

	if (!Parse(Scene))
	{
		return false;
	}

	if (!Convert())
	{
		return false;
	}

	return true;
}

bool FFbxImporter::Parse(FbxScene* Scene)
{
	FbxNode* RootNode = Scene->GetRootNode();

	if (!RootNode)
	{
		return false;
	}

	TArray<FbxNode*> Nodes;
	TMap<FbxNode*, int32> NodeToIndex;

	CollectNodes(RootNode, 0, Nodes);
	CollectMaterials(Scene);

	ParseBone(Nodes, NodeToIndex);
	ParseSkin(Nodes, NodeToIndex);
	
	return true;
}

bool FFbxImporter::Convert()
{
	SkeletalMaterials.clear();

	for (const FMaterialInfo& MatInfo : MtlInfos)
	{
		FString MaterialPath = ConvertToMat(&MatInfo);
		UMaterial* MaterialObject = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);

		FSkeletalMaterial NewMaterial;
		NewMaterial.MaterialInterface = MaterialObject;
		NewMaterial.MaterialSlotName = MatInfo.Name;
		SkeletalMaterials.push_back(NewMaterial);
	}

	return true;
}

void FFbxImporter::CollectNodes(FbxNode* Node, int32 depth, TArray<FbxNode*>& OutNodes)
{
	OutNodes.push_back(Node);

	for (int i = 0; i < Node->GetChildCount(); ++i)
	{
		CollectNodes(Node->GetChild(i), depth + 1, OutNodes);
	}
}

void FFbxImporter::CollectMaterials(FbxScene* Scene)
{
	MtlInfos.clear();
	MaterialToSlotIndex.clear();

	int32 MaterialCount = Scene->GetMaterialCount();

	for (int32 i = 0; i < MaterialCount; ++i)
	{
		FbxSurfaceMaterial* Material = Scene->GetMaterial(i);
		if (!Material) continue;

		FMaterialInfo MatInfo;
		MatInfo.Name = Material->GetName();
		MatInfo.DiffuseColor = { 1.0f, 1.0f, 1.0f };

		FbxProperty DiffuseProp = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
		if (DiffuseProp.IsValid())
		{
			FbxDouble3 Color = DiffuseProp.Get<FbxDouble3>();
			MatInfo.DiffuseColor = { (float)Color[0], (float)Color[1], (float)Color[2] };

			int32 TextureCount = DiffuseProp.GetSrcObjectCount<FbxTexture>();
			if (TextureCount > 0)
			{
				FbxFileTexture* Texture = DiffuseProp.GetSrcObject<FbxFileTexture>(0);
				if (Texture)
				{
					MatInfo.TexturePath = Texture->GetFileName();
				}
			}
		}

		int32 GlobalIndex = (int32)MtlInfos.size();
		MtlInfos.push_back(MatInfo);
		MaterialToSlotIndex[Material] = GlobalIndex;
	}
}

static FMatrix ConvertFbxMatrix(const FbxMatrix& FbxMat)
{
	FMatrix Mat;
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			Mat.M[i][j] = (float)FbxMat.Get(i, j);
		}
	}
	return Mat;
}

static FbxAMatrix GetGeometryTransform(FbxNode* Node)
{
	FbxAMatrix GeometryTransform;
	if (!Node)
	{
		return GeometryTransform;
	}

	GeometryTransform.SetT(Node->GetGeometricTranslation(FbxNode::eSourcePivot));
	GeometryTransform.SetR(Node->GetGeometricRotation(FbxNode::eSourcePivot));
	GeometryTransform.SetS(Node->GetGeometricScaling(FbxNode::eSourcePivot));
	return GeometryTransform;
}

void FFbxImporter::ParseBone(TArray<FbxNode*>& Nodes, TMap<FbxNode*, int32>& OutNodeToIndex)
{
	Bones.clear();
	OutNodeToIndex.clear();

	for (int32 i = 0; i < Nodes.size(); ++i)
	{
		FbxNode* Node = Nodes[i];

		FbxNodeAttribute* Attr = Node->GetNodeAttribute();
		if (Attr && Attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			FBone Bone;
			Bone.Name = Node->GetName();

			FbxNode* ParentNode = Node->GetParent();
			Bone.ParentIndex = FindNearestParentBoneIndex(Node, OutNodeToIndex);

			FbxMatrix LocalMatrix = Node->EvaluateLocalTransform();
			FbxMatrix GlobalMatrix = Node->EvaluateGlobalTransform();
			Bone.LocalTransform = FTransform(ConvertFbxMatrix(LocalMatrix));
			Bone.GlobalTransform = FTransform(ConvertFbxMatrix(GlobalMatrix));

			int32 NewBoneIndex = (int32)Bones.size();
			Bones.push_back(Bone);
			OutNodeToIndex[Node] = NewBoneIndex;
		}
	}
}

static void NormalizeWeights(float* Weights, int32 Count)
{
	float TotalWeight = 0.0f;
	for (int32 i = 0; i < Count; ++i)
	{
		TotalWeight += Weights[i];
	}

	if (TotalWeight > 0.0f)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			Weights[i] /= TotalWeight;
		}
	}
}

void FFbxImporter::ParseSkin(TArray<FbxNode*>& Nodes, TMap<FbxNode*, int32>& NodeToIndex)
{
	Vertices.clear();
	Indices.clear();
	Sections.clear();
	MeshRanges.clear();
	TangentSums.clear();

	for (FbxNode* Node : Nodes)
	{
		FbxMesh* Mesh = Node->GetMesh();
		if (!Mesh) continue;

		int32 DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
		FbxSkin* Skin = DeformerCount > 0 ? (FbxSkin*)Mesh->GetDeformer(0, FbxDeformer::eSkin) : nullptr;
		int32 ClusterCount = Skin ? Skin->GetClusterCount() : 0;
		const bool bHasSkin = Skin && ClusterCount > 0;
		const int32 RigidBoneIndex = bHasSkin ? -1 : FindNearestParentBoneIndex(Node, NodeToIndex);

		struct WeightData { int32 BoneIndex; float Weight; };
		TArray<TArray<WeightData>> TempWeights(Mesh->GetControlPointsCount());
		FbxAMatrix NodeGeometryTransform = GetGeometryTransform(Node);
		FMatrix MeshBindGlobal = ConvertFbxMatrix(Node->EvaluateGlobalTransform() * NodeGeometryTransform);
		bool bHasClusterMeshBindGlobal = false;

		for (int32 i = 0; i < ClusterCount; ++i)
		{
			FbxCluster* Cluster = Skin->GetCluster(i);
			FbxNode* LinkNode = Cluster->GetLink();

			auto It = NodeToIndex.find(LinkNode);
			if (It == NodeToIndex.end()) continue;

			FbxAMatrix LinkBindMatrix;
			Cluster->GetTransformLinkMatrix(LinkBindMatrix);

			int32 BoneIndex = It->second;
			Bones[BoneIndex].InverseBindPoseMatrix = ConvertFbxMatrix(LinkBindMatrix).GetInverse();

			if (!bHasClusterMeshBindGlobal)
			{
				FbxAMatrix MeshBindMatrix;
				Cluster->GetTransformMatrix(MeshBindMatrix);
				MeshBindGlobal = ConvertFbxMatrix(MeshBindMatrix);
				bHasClusterMeshBindGlobal = true;
			}

			int32* ControlPointIndices = Cluster->GetControlPointIndices();
			double* ControlPointWeights = Cluster->GetControlPointWeights();
			int32 NumIndices = Cluster->GetControlPointIndicesCount();

			for (int32 j = 0; j < NumIndices; ++j)
			{
				int32 CPIndex = ControlPointIndices[j];
				float Weight = (float)ControlPointWeights[j];
				TempWeights[CPIndex].push_back({ BoneIndex, Weight });
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

			auto It = MaterialToSlotIndex.find(Material);
			LocalToGlobalMaterialIndex[LocalIndex] = (It != MaterialToSlotIndex.end()) ? It->second : -1;
		}

		TMap<int32, TArray<uint32>> SectionIndicesMap;
		TMap<FFbxSkeletalVertexKey, uint32> VertexMap;
		const uint32 VertexStart = (uint32)Vertices.size();
		const uint32 FirstIndex = (uint32)Indices.size();

		for (int32 i = 0; i < Mesh->GetPolygonCount(); ++i)
		{
			int32 LocalMaterialIndex = GetMaterialIndex(Mesh, i);
			int32 GlobalMaterialIndex = -1;

			FbxSurfaceMaterial* Material = nullptr;
			if (LocalMaterialIndex >= 0 && LocalMaterialIndex < (int32)LocalToGlobalMaterialIndex.size())
			{
				GlobalMaterialIndex = LocalToGlobalMaterialIndex[LocalMaterialIndex];
			}
			uint32 TriIndices[3] = {};
			for (int32 j = 0; j < 3; ++j)
			{
				FVertexPNCTBW Vertex;
				int32 CPIndex = Mesh->GetPolygonVertex(i, j);

				FbxVector4 CP = Mesh->GetControlPointAt(CPIndex);
				Vertex.Position = FVector((float)CP[0], (float)CP[1], (float)CP[2]);

				auto& Weights = TempWeights[CPIndex];
				std::sort(Weights.begin(), Weights.end(), [](const WeightData& A, const WeightData& B)
				{
					return A.Weight > B.Weight;
				});

				if (Weights.empty() && RigidBoneIndex >= 0)
				{
					Weights.push_back({ RigidBoneIndex, 1.0f });
				}

				for (int32 k = 0; k < (int32)Weights.size() && k < 4; ++k)
				{
					Vertex.BoneIndices[k] = Weights[k].BoneIndex;
					Vertex.BoneWeights[k] = Weights[k].Weight;
				}

				NormalizeWeights(Vertex.BoneWeights, 4);

				FbxVector4 Normal;
				Mesh->GetPolygonVertexNormal(i, j, Normal);
				Normal.Normalize();
				FVector N = FVector((float)Normal[0], (float)Normal[1], (float)Normal[2]);
				N.Normalize();

				Vertex.Normal = N;

				Vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
				Vertex.UV = FVector2(0.0f, 0.0f);

				if (UVName)
				{
					FbxVector2 UV;
					bool bUnmappedUV = false;
					const bool bSuccess = Mesh->GetPolygonVertexUV(i, j, UVName, UV, bUnmappedUV);
					if (bSuccess && !bUnmappedUV)
					{
						Vertex.UV = FVector2((float)UV[0], 1.0f - (float)UV[1]);
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
				auto It = VertexMap.find(Key);
				if (It != VertexMap.end())
				{
					VertexIndex = It->second;
				}
				else
				{
					VertexIndex = (uint32)Vertices.size();
					Vertices.push_back(Vertex);
					TangentSums.push_back(FVector::ZeroVector);
					BitangentSums.push_back(FVector::ZeroVector);
					VertexMap[Key] = VertexIndex;
				}
				TriIndices[j] = VertexIndex;
				SectionIndicesMap[GlobalMaterialIndex].push_back(VertexIndex);
			}

			//Tangent 연산
			GenerateTangents(TriIndices);
		}

		BuildTangentsForVertexRange(VertexStart);

		uint32 CurrentBaseIndex = (uint32)Indices.size();

		for (auto& Pair : SectionIndicesMap)
		{
			FSkeletalMeshSection Section;

			int32 MatIndex = Pair.first;
			if (MtlInfos.size() > MatIndex)
			{
				Section.MaterialSlotName = MtlInfos[MatIndex].Name;
			}
			else
			{
				UE_LOG("Warning: Material index %d out of range. Assigning to Default slot.", Pair.first);
				Section.MaterialSlotName = "None";
			}
			Section.MaterialIndex = Pair.first;
			Section.FirstIndex = CurrentBaseIndex;
			Section.IndexCount = (uint32)Pair.second.size();

			CurrentBaseIndex += Section.IndexCount;
			
			Indices.insert(Indices.end(), Pair.second.begin(), Pair.second.end());
			Sections.push_back(Section);
		}

		FSkeletalMeshRange MeshRange;
		MeshRange.VertexStart = VertexStart;
		MeshRange.VertexEnd = (uint32)Vertices.size();
		MeshRange.FirstIndex = FirstIndex;
		MeshRange.IndexCount = (uint32)Indices.size() - FirstIndex;
		MeshRange.MeshBindGlobal = MeshBindGlobal;
		if (MeshRange.VertexStart < MeshRange.VertexEnd && MeshRange.IndexCount > 0)
		{
			MeshRanges.push_back(MeshRange);
		}
	}
}

void FFbxImporter::GenerateTangents(uint32 TriIndices[])
{
	const FVertexPNCTBW& V0 = Vertices[TriIndices[0]];
	const FVertexPNCTBW& V1 = Vertices[TriIndices[1]];
	const FVertexPNCTBW& V2 = Vertices[TriIndices[2]];

	FVector Edge1 = V1.Position - V0.Position;
	FVector Edge2 = V2.Position - V0.Position;

	FVector2 DeltaUV1 = V1.UV - V0.UV;
	FVector2 DeltaUV2 = V2.UV - V0.UV;

	float Det = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;
	if (std::abs(Det) >= 1e-8f)
	{
		float InvDet = 1.0f / Det;

		FVector Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDet;
		FVector Bitangent = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * InvDet;

		TangentSums[TriIndices[0]] += Tangent;
		TangentSums[TriIndices[1]] += Tangent;
		TangentSums[TriIndices[2]] += Tangent;

		BitangentSums[TriIndices[0]] += Bitangent;
		BitangentSums[TriIndices[1]] += Bitangent;
		BitangentSums[TriIndices[2]] += Bitangent;
	}
}

void FFbxImporter::BuildTangentsForVertexRange(const uint32 VertexStart)
{
	for (uint32 i = VertexStart; i < (uint32)Vertices.size(); ++i)
	{
		FVector N = Vertices[i].Normal;
		FVector T = TangentSums[i];

		T = T - N * N.Dot(T);
		if (T.Length() < FMath::Epsilon)
		{
			FVector Axis = std::abs(N.Z) < 0.999f ? FVector(0.0f, 0.0f, 1.0f) : FVector(0.0f, 1.0f, 0.0f);
			T = Axis.Cross(N).Normalized();
		}
		else
		{
			T.Normalize();
		}


		FVector B = BitangentSums[i];
		float Handedness = N.Cross(T).Dot(B) < 0.0f ? -1.0f : 1.0f;

		Vertices[i].Tangent = FVector4(T, Handedness);
	}
}

int32 FFbxImporter::GetMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex)
{
	FbxLayerElementMaterial* LayerElementMaterial = Mesh->GetElementMaterial();
	if (!LayerElementMaterial) return -1;

	FbxLayerElementArrayTemplate<int32>& MaterialIndices = LayerElementMaterial->GetIndexArray();

	switch (LayerElementMaterial->GetMappingMode())
	{
	case FbxLayerElement::eAllSame: return MaterialIndices[0];
	case FbxLayerElement::eByPolygon: return MaterialIndices[PolygonIndex];
	}

	return 0;
}

int32 FFbxImporter::FindNearestParentBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& NodeToIndex)
{
	FbxNode* Parent = Node ? Node->GetParent() : nullptr;

	while (Parent)
	{
		auto It = NodeToIndex.find(Parent);
		if (It != NodeToIndex.end())
		{
			return It->second;
		}

		Parent = Parent->GetParent();
	}

	return -1;
}

void FFbxImporter::TriangulateScene(FbxScene* Scene)
{
	FbxGeometryConverter Converter(Scene->GetFbxManager());

	Converter.Triangulate(Scene, true);
}

FString FFbxImporter::ConvertToMat(const FMaterialInfo* MaterialInfo)
{
	FString MatPath = "Asset/Materials/Auto/" + MaterialInfo->Name + ".mat";

	if (std::filesystem::exists(FPaths::ToWide(MatPath)))
	{
		return MatPath;
	}

	std::filesystem::create_directories(FPaths::ToWide("Asset/Materials/Auto"));

	json::JSON JsonData;
	JsonData["PathFileName"] = MatPath;
	JsonData["Origin"] = "FbxImport";
	JsonData["ShaderPath"] = "Shaders/Geometry/UberLit.hlsl";
	JsonData["RenderPass"] = "Opaque";

	if (!MaterialInfo->TexturePath.empty())
	{
		JsonData["Textures"]["DiffuseTexture"] = MaterialInfo->TexturePath;

		JsonData["Parameters"]["SectionColor"][0] = 1.0f;
		JsonData["Parameters"]["SectionColor"][1] = 1.0f;
		JsonData["Parameters"]["SectionColor"][2] = 1.0f;
		JsonData["Parameters"]["SectionColor"][3] = 1.0f;
	}
	else
	{
		JsonData["Parameters"]["SectionColor"][0] = MaterialInfo->DiffuseColor.X;
		JsonData["Parameters"]["SectionColor"][1] = MaterialInfo->DiffuseColor.Y;
		JsonData["Parameters"]["SectionColor"][2] = MaterialInfo->DiffuseColor.Z;
		JsonData["Parameters"]["SectionColor"][3] = 1.0f;
	}

	std::ofstream File(FPaths::ToWide(MatPath));
	File << JsonData.dump();

	return MatPath;
}


