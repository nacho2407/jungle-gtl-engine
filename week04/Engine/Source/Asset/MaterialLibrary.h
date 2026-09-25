#pragma once

#include "Asset/Asset.h"
#include "Asset/AssetManager.h"

namespace Engine::Asset
{
    /**
     * @brief .mtl 파일의 메타데이터를 보관하는 라이브러리 에셋.
     * 실제 머티리얼 데이터는 개별 UMaterial이 소유합니다.
     */
    class ENGINE_API UMaterialLibrary : public UAsset
    {
        DECLARE_RTTI(UMaterialLibrary, UAsset)
      public:
        UMaterialLibrary() = default;
        virtual ~UMaterialLibrary() override = default;

        void Initialize(const FSourceRecord& InSource, const FString& InLibraryName,
                        TMap<FString, FAssetId> InMaterialRefs);
        const TMap<FString, FAssetId>& GetMaterialRefs() const { return MaterialRef; }

      private:
        FString MaterialLibraryName{"Default"};
        TMap<FString, FAssetId> MaterialRef;
    };
} // namespace Engine::Asset
