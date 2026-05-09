#include "FbxImporter.h"
#include "Platform/Paths.h"
#include "Core/Log.h"

#include <filesystem>
#include <fstream>

TArray<FBone> FFbxImporter::Bones;
TArray<FVertexPNCTBW> FFbxImporter::Vertices;
TArray<uint32> FFbxImporter::Indices;
TArray<FSkeletalMeshSection> FFbxImporter::Sections;
TArray<FFbxImporter::FMaterialInfo> FFbxImporter::MtlInfos;
TArray<FSkeletalMaterial> FFbxImporter::SkeletalMaterials;
TMap<FbxSurfaceMaterial*, int32> FFbxImporter::MaterialToSlotIndex;

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

	FbxAxisSystem UnrealAxisSystem(FbxAxisSystem::eZAxis, (FbxAxisSystem::EFrontVector) FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded);
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
			auto it = OutNodeToIndex.find(ParentNode);
			if (it != OutNodeToIndex.end())
			{
				Bone.ParentIndex = it->second;
			}
			else
			{
				Bone.ParentIndex = -1;
			}

			FbxMatrix LocalMatrix = Node->EvaluateLocalTransform();
			FbxMatrix GlobalMatrix = Node->EvaluateGlobalTransform();
			FbxMatrix InverseBindPoseMatrix = GlobalMatrix.Inverse();
			Bone.LocalMatrix = ConvertFbxMatrix(LocalMatrix);
			Bone.GlobalMatrix = ConvertFbxMatrix(GlobalMatrix);
			Bone.InverseBindPoseMatrix = ConvertFbxMatrix(InverseBindPoseMatrix);

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

	for (FbxNode* Node : Nodes)
	{
		FbxMesh* Mesh = Node->GetMesh();
		if (!Mesh) continue;

		int32 DeformerCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
		if (DeformerCount <= 0) continue;

		FbxSkin* Skin = (FbxSkin*)Mesh->GetDeformer(0, FbxDeformer::eSkin);
		int32 ClusterCount = Skin->GetClusterCount();

		struct WeightData { int32 BoneIndex; float Weight; };
		TArray<TArray<WeightData>> TempWeights(Mesh->GetControlPointsCount());

		for (int32 i = 0; i < ClusterCount; ++i)
		{
			FbxCluster* Cluster = Skin->GetCluster(i);
			FbxNode* LinkNode = Cluster->GetLink();

			int32 BoneIndex = NodeToIndex[LinkNode];

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

		FbxAMatrix NodeGlobalTransform = Node->EvaluateGlobalTransform();

		for (int32 i = 0; i < Mesh->GetPolygonCount(); ++i)
		{
			int32 LocalMaterialIndex = GetMaterialIndex(Mesh, i);
			int32 GlobalMaterialIndex = -1;

			FbxSurfaceMaterial* Material = nullptr;
			if (LocalMaterialIndex >= 0 && LocalMaterialIndex < (int32)LocalToGlobalMaterialIndex.size())
			{
				GlobalMaterialIndex = LocalToGlobalMaterialIndex[LocalMaterialIndex];
			}

			for (int32 j = 0; j < 3; ++j)
			{
				FVertexPNCTBW Vertex;
				int32 CPIndex = Mesh->GetPolygonVertex(i, j);

				FbxVector4 CP = Mesh->GetControlPointAt(CPIndex);
				FbxVector4 WorldCP = NodeGlobalTransform.MultT(CP);

				Vertex.Position = FVector((float)WorldCP[0], (float)WorldCP[1], (float)WorldCP[2]);

				for (int32 k = 0; k < (int32)TempWeights[CPIndex].size() && k < 4; ++k)
				{
					Vertex.BoneIndices[k] = TempWeights[CPIndex][k].BoneIndex;
					Vertex.BoneWeights[k] = TempWeights[CPIndex][k].Weight;
				}
				NormalizeWeights(Vertex.BoneWeights, 4);

				FbxVector4 Normal;
				Mesh->GetPolygonVertexNormal(i, j, Normal);

				FbxVector4 WorldNormal = NodeGlobalTransform.MultR(Normal);

				FVector N = FVector((float)WorldNormal[0], (float)WorldNormal[1], (float)WorldNormal[2]);
				N.Normalize();

				Vertex.Normal = N;

				Vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

				FbxVector2 UV;
				bool bMapped;
				Mesh->GetPolygonVertexUV(i, j, UVName, UV, bMapped);
				Vertex.UV = { (float)UV[0], 1.0f - (float)UV[1] };

				Vertices.push_back(Vertex);

				SectionIndicesMap[GlobalMaterialIndex].push_back((uint32)Vertices.size() - 1);
			}
		}

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
