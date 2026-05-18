#pragma once

#include "Core/CoreMinimal.h"

namespace fbxsdk
{
	class FbxManager;
	class FbxScene;
} // namespace fbxsdk

struct FFbxSceneImportContext
{
    fbxsdk::FbxManager* Manager = nullptr;
    fbxsdk::FbxScene* Scene = nullptr;
    FString SourcePath;
    ~FFbxSceneImportContext();
    bool Import(const FString& Path);
    void Destroy();
};
