#pragma once

#include "Asset/MaterialInterface.h"
#include "Renderer/RenderAsset/MaterialResource.h"
#include "Asset/Texture2DAsset.h"

#include <functional>

struct FSourceRecord;

namespace Engine::Asset
{
    //@brief Material library 안의 특정 material을 직접 가리키는 에셋.
    class ENGINE_API UMaterial : public UMaterialInterface
    {
        DECLARE_RTTI(UMaterial, UMaterialInterface)
      public:
        UMaterial() = default;
        virtual ~UMaterial() override = default;

        void Initialize(const FSourceRecord& InSource, const FMaterial& InMaterial,
                        const FString& InMaterialName);

        virtual const FMaterial* GetMaterialData() const override { return &MaterialData; }
        FMaterial*               GetMaterialDataMutable() { return &MaterialData; }
        const FString&           GetMaterialName() const { return MaterialName; }

        // Texture 종속성 관련 함수
        enum class EMaterialTextureSlot : uint8
        {
            Diffuse,
            Ambient,
            Specular,
            Normal
        };

        struct FMaterialTextureSlotHasher
        {
            size_t operator()(EMaterialTextureSlot Slot) const noexcept
            {
                return std::hash<uint8>{}(static_cast<uint8>(Slot));
            }
        };

        void SetTextureDependency(EMaterialTextureSlot Slot, UTexture2DAsset* InTexture);
        UTexture2DAsset* GetTextureDependency(EMaterialTextureSlot Slot) const;
        bool HasTextureDependency(EMaterialTextureSlot Slot) const;
        void ClearTextureDependencies() { ReferencedTextures.clear(); }
        TMap<EMaterialTextureSlot, UTexture2DAsset*, FMaterialTextureSlotHasher>&
        GetReferencedTextures()
        {
            return ReferencedTextures;
        }
        const TMap<EMaterialTextureSlot, UTexture2DAsset*, FMaterialTextureSlotHasher>&
        GetReferencedTextures() const
        {
            return ReferencedTextures;
        }

      private:
        FMaterial                MaterialData;
        FString                  MaterialName;
        TMap<EMaterialTextureSlot, UTexture2DAsset*, FMaterialTextureSlotHasher>
            ReferencedTextures;
    };
} // namespace Engine::Asset
