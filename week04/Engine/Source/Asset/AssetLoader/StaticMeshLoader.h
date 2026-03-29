#pragma once
#include "Core/CoreMinimal.h"
#include "AssetLoader.h"
#include "Asset/AssetManager.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"

class FD3D11RHI;
struct FMeshVertexNormal;

class ENGINE_API FStaticMeshLoader : public IAssetLoader
{
  public:
    explicit FStaticMeshLoader(FD3D11RHI* InRHI, UAssetManager* InAssetManager);

    bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
    EAssetType GetAssetType() const override;
    uint64     MakeBuildSignature(const FAssetLoadParams& Params) const override;
    UAsset*    LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;

  private:
    bool ParseObjText(const FSourceRecord& Source, FStaticMeshResource& OutMesh,
                      TArray<FMeshVertexNormal>& OutVertices) const;
    bool CreateBuffers(const TArray<FMeshVertexNormal>& InVertices,
                       FStaticMeshResource&             OutMesh) const;

    // 문자열 유틸리티
    FString WidePathToUtf8(const FWString& Path) const;

  private:
    FD3D11RHI* RHI = nullptr;
    UAssetManager* AssetManager = nullptr;
};
