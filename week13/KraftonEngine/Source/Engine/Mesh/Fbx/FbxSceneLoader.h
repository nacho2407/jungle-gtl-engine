#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>

struct FFbxSceneHandle
{
	FbxManager* Manager = nullptr;
	FbxScene* Scene = nullptr;

	~FFbxSceneHandle();

	FFbxSceneHandle() = default;
	FFbxSceneHandle(const FFbxSceneHandle&) = delete;
	FFbxSceneHandle& operator=(const FFbxSceneHandle&) = delete;
};

struct FFbxSceneLoadOptions
{
	bool bImportMaterials      = true;
	bool bImportTextures       = true;
	bool bImportLinks          = true;
	bool bImportShapes         = true;
	bool bImportGobos          = true;
	bool bImportAnimations     = true;
	bool bImportGlobalSettings = true;

	// Empty means keep the FBX SDK default, which imports every animation stack.
	TSet<int32> SelectedAnimationStackIndices;
};

class FFbxSceneLoader
{
public:
	static bool Load(const FString& SourcePath, FFbxSceneHandle& OutScene, FString* OutMessage = nullptr);
	static bool Load(
		const FString&              SourcePath,
		const FFbxSceneLoadOptions& Options,
		FFbxSceneHandle&            OutScene,
		FString*                    OutMessage = nullptr
		);
	static void NormalizeScene(FbxScene* Scene);
	static void Triangulate(FbxManager* Manager, FbxScene* Scene);
};
