#pragma once
#include "Source/Renderer/RenderAsset/MaterialResource.h"
#include "AssetLoader.h"

namespace Engine::Asset
{
    /**
     * @brief .mtl 파일을 읽어 UMaterial 에셋을 생성하는 로더입니다.
     */
    class ENGINE_API FMaterialLoader : public IAssetLoader
    {
      public:
        FMaterialLoader() = default;
        virtual ~FMaterialLoader() override = default;

        virtual bool       CanLoad(const FWString& Path, const FAssetLoadParams& Params) const override;
        virtual EAssetType GetAssetType() const override { return EAssetType::Material; }

        virtual uint64     MakeBuildSignature(const FAssetLoadParams& Params) const override;
        virtual UAsset*    LoadAsset(const FSourceRecord& Source, const FAssetLoadParams& Params) override;

      private:
        bool ParseMtlText(const FSourceRecord& Source, struct ::FMaterialResource& OutResource);
    };
} // namespace Engine::Asset
