#include "Mesh/Fbx/FbxSceneLoader.h"
#include "Platform/Paths.h"
#include "Core/Log.h"

FFbxSceneHandle::~FFbxSceneHandle()
{
	if (Manager)
	{
		Manager->Destroy();
		Manager = nullptr;
		Scene = nullptr;
	}
}

bool FFbxSceneLoader::Load(const FString& SourcePath, FFbxSceneHandle& OutScene, FString* OutMessage)
{
	if (OutScene.Manager)
	{
		OutScene.Manager->Destroy();
	}
	OutScene.Manager = nullptr;
	OutScene.Scene = nullptr;

	FbxManager* SdkManager = FbxManager::Create();
	if (!SdkManager)
	{
		if (OutMessage) *OutMessage = "FBX SDK manager creation failed.";
		return false;
	}

	FbxIOSettings* IoSettings = FbxIOSettings::Create(SdkManager, IOSROOT);
	if (!IoSettings)
	{
		SdkManager->Destroy();
		if (OutMessage) *OutMessage = "FBX IO settings creation failed.";
		return false;
	}
	SdkManager->SetIOSettings(IoSettings);

	FbxScene* Scene = FbxScene::Create(SdkManager, "FBX Scene");
	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
	if (!Scene || !Importer)
	{
		if (Importer) Importer->Destroy();
		SdkManager->Destroy();
		if (OutMessage) *OutMessage = "FBX scene/importer creation failed.";
		return false;
	}

	const FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(SourcePath)));
	if (!Importer->Initialize(FullPath.c_str(), -1, SdkManager->GetIOSettings()))
	{
		Importer->Destroy();
		SdkManager->Destroy();
		if (OutMessage) *OutMessage = FString("FBX importer initialize failed: ") + SourcePath;
		return false;
	}

	if (!Importer->Import(Scene))
	{
		Importer->Destroy();
		SdkManager->Destroy();
		if (OutMessage) *OutMessage = FString("FBX scene import failed: ") + SourcePath;
		return false;
	}

	Importer->Destroy();
	OutScene.Manager = SdkManager;
	OutScene.Scene = Scene;
	return true;
}

void FFbxSceneLoader::NormalizeScene(FbxScene* Scene)
{
	if (!Scene)
	{
		return;
	}

	FbxSystemUnit::m.ConvertScene(Scene);

	FbxAxisSystem UnrealAxisSystem(
		FbxAxisSystem::eZAxis,
		FbxAxisSystem::eParityEven,
		FbxAxisSystem::eLeftHanded
	);
	UnrealAxisSystem.DeepConvertScene(Scene);
}

void FFbxSceneLoader::Triangulate(FbxManager* Manager, FbxScene* Scene)
{
	if (!Manager || !Scene)
	{
		return;
	}

	FbxGeometryConverter Converter(Manager);
	Converter.Triangulate(Scene, true);
}
