#pragma once
#include "AssetLoader.h"
#include "AssetManager.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"
#include <unordered_map>
#include <string>

class FD3D11RHI;
struct FVertex; // 엔진에 정의된 버텍스 구조체 (Position, UV, Normal 등 포함)

class ENGINE_API FStaticMeshLoader : public IAssetLoader
{
  public:
    explicit FStaticMeshLoader(FD3D11RHI* InRHI);

    bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
    EAssetType GetAssetType() const override;
    uint64     MakeBuildSignature(const FAssetLoadParams& Params) const override;
    UAsset*    LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;

  private:
    bool ParseObjText(const FSourceRecord& Source, FStaticMeshResource& OutMesh,
                      TArray<FVertex>& OutVertices) const;
    bool CreateBuffers(const TArray<FVertex>& InVertices, FStaticMeshResource& OutMesh) const;

    // 문자열 유틸리티
    FString WidePathToUtf8(const FWString& Path) const;

  private:
    FD3D11RHI* RHI = nullptr;
};