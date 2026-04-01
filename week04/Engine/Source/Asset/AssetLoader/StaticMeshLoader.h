#pragma once
#include "Core/CoreMinimal.h"
#include "AssetLoader.h"
#include "Asset/AssetManager.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"

class FD3D11RHI;
struct FMeshVertexPNCT;

class ENGINE_API FStaticMeshLoader : public IAssetLoader
{
  public:
    explicit FStaticMeshLoader(FD3D11RHI* InRHI, UAssetManager* InAssetManager);
    ~FStaticMeshLoader() = default;

    bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
    EAssetType GetAssetType() const override;
    UAsset*    LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;

  private:
    bool ParseObjText(const FSourceRecord& Source, FStaticMesh& OutMesh,
                      TArray<FMeshVertexPNCT>& OutVertices) const;
    bool CreateBuffers(const TArray<FMeshVertexPNCT>& InVertices, FStaticMesh& OutMesh) const;

  private:
    FD3D11RHI*     RHI = nullptr;
    UAssetManager* AssetManager = nullptr;
};
