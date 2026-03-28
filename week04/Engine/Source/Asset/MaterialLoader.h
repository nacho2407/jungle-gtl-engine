#pragma once

#include "AssetLoader.h"
#include "AssetManager.h"
#include "Renderer/RenderAsset/MaterialResource.h"

class ENGINE_API FMaterialLoader : public IAssetLoader
{
  public:
    FMaterialLoader(UAssetManager* InAssetManager) { AssetManager = InAssetManager; }
    ~FMaterialLoader() override = default;

    bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
    EAssetType GetAssetType() const override;
    uint64     MakeBuildSignature(const FAssetLoadParams& Params) const override;
    UAsset*    LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;

  private:
    bool ParseMtlText(const FSourceRecord& Source, FMaterialResource& OutResource) const;

    // map_Kd 경로를 .mtl 파일 위치 기준으로 절대 경로화 해주는 유틸리티
    FWString ResolveSiblingPath(const FWString& BaseFilePath, const FString& RelativePath) const;
    FString  WidePathToUtf8(const FWString& Path) const;

    UAssetManager* AssetManager = nullptr;
};
