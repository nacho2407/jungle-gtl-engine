#include "FbxImporter.h"
#include "Platform/Paths.h"
#include "Core/Log.h"

#include <fbxsdk.h>

static void ProcessMesh(FbxNode* Node)
{
	FbxMesh* Mesh = Node->GetMesh();
	FbxVector4* Vertices = Mesh->GetControlPoints();

	for (int i = 0; i < Mesh->GetControlPointsCount(); ++i)
	{
		float x = (float)Vertices[i][0];
		float y = (float)Vertices[i][1];
		float z = (float)Vertices[i][2];

		UE_LOG("Mesh Vertex: (%f, %f, %f)", x, y, z);
	}
}

static void ProcessNode(FbxNode* Node)
{
	const char* NodeName = Node->GetName();

	FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
	if (Attribute)
	{
		if (Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			ProcessMesh(Node);
		}
	}

	for (int i = 0; i < Node->GetChildCount(); ++i)
	{
		ProcessNode(Node->GetChild(i));
	}
}

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

	ProcessNode(Scene->GetRootNode());
}