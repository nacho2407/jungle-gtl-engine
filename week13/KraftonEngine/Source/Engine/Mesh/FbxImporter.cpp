#include "FbxImporter.h"
#include "Platform/Paths.h"
#include "Core/Log.h"

TArray<FBone> FFbxImporter::Bones;
TArray<FVertexPNCTBW> FFbxImporter::Vertices;
TArray<uint32> FFbxImporter::Indices;

bool FFbxImporter::Import(const FString& FilePath)
{
	FbxManager* SdkManager = FbxManager::Create();

	FbxIOSettings* ios = FbxIOSettings::Create(SdkManager, IOSROOT);
	SdkManager->SetIOSettings(ios);

	FbxScene* Scene = FbxScene::Create(SdkManager, "My Scene");

	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");

	FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), FPaths::ToWide(FilePath)));

	if (!Importer->Initialize(FullPath.c_str(), -1, SdkManager->GetIOSettings()))
	{
		return false;
	}

	Importer->Import(Scene);
	Importer->Destroy();

	if (!Parse(Scene->GetRootNode()))
	{
		return false;
	}

	return true;
}

bool FFbxImporter::Parse(FbxNode* RootNode)
{
	if (!RootNode)
	{
		return false;
	}

	TArray<FbxNode*> Nodes;
	TMap<FbxNode*, int32> NodeToIndex;

	CollectNodes(RootNode, 0, Nodes);
	ParseBone(Nodes, NodeToIndex);
	ParseSkin(Nodes, NodeToIndex);
	
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
			Bone.LocalMatrix = ConvertFbxMatrix(LocalMatrix);

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

		for (int32 i = 0; i < Mesh->GetPolygonCount(); ++i)
		{
			// Triangulation을 가정
			for (int32 j = 0; j < 3; ++j)
			{
				FVertexPNCTBW Vertex;
				int32 CPIndex = Mesh->GetPolygonVertex(i, j);

				FbxVector4 CP = Mesh->GetControlPointAt(CPIndex);
				Vertex.Position = FVector((float)CP[0], (float)CP[1], (float)CP[2]);

				for (int32 k = 0; k < (int32)TempWeights[CPIndex].size() && k < 4; ++k)
				{
					Vertex.BoneIndices[k] = TempWeights[CPIndex][k].BoneIndex;
					Vertex.BoneWeights[k] = TempWeights[CPIndex][k].Weight;
				}
				NormalizeWeights(Vertex.BoneWeights, 4);

				FbxVector4 Normal;
				Mesh->GetPolygonVertexNormal(i, j, Normal);
				Vertex.Normal = { (float)Normal[0], (float)Normal[1], (float)Normal[2] };

				FbxVector2 UV;
				bool bMapped;
				Mesh->GetPolygonVertexUV(i, j, UVName, UV, bMapped);
				Vertex.UV = { (float)UV[0], (float)UV[1] };

				Vertices.push_back(Vertex);
				Indices.push_back((uint32)Vertices.size() - 1);
			}
		}
	}
}
