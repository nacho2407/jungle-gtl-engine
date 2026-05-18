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

class FFbxSceneLoader
{
public:
	static bool Load(const FString& SourcePath, FFbxSceneHandle& OutScene, FString* OutMessage = nullptr);
	static void NormalizeScene(FbxScene* Scene);
	static void Triangulate(FbxManager* Manager, FbxScene* Scene);
};
