#pragma once

#include "AtlasComponent.h"

struct FSubUVAtlasResource;
class UAssetManager;

namespace Engine::Component
{
    class ENGINE_API USubUVComponent : public UAtlasComponent
    {
        DECLARE_RTTI(USubUVComponent, UAtlasComponent)
      public:
        USubUVComponent() = default;
        ~USubUVComponent() override = default;

        const FString& GetSubUVAtlasPath() const { return SubUVAtlasPath; }
        void           SetSubUVAtlasPath(const FString& InPath);

        const FSubUVAtlasResource* GetSubUVAtlasResource() const { return SubUVResource; }
        FSubUVAtlasResource*       GetSubUVAtlasResource() { return SubUVResource; }
        void                       SetSubUVAtlasResource(FSubUVAtlasResource* InResource);

        int32 GetFrameIndex() const { return FrameIndex; }
        void  SetFrameIndex(int32 InFrameIndex);
        void DescribeProperties(FComponentPropertyBuilder& Builder) override;
        void ResolveAssetReferences(UAssetManager* InAssetManager) override;

        int32 GetFrameCount() const;

      protected:
        FSubUVAtlasResource* SubUVResource = nullptr;
        FString SubUVAtlasPath = {};
        int32 FrameIndex = 0;
    };
} // namespace Engine::Component