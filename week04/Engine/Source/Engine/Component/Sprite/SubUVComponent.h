#pragma once

#include "AtlasComponent.h"
#include "Core/CoreMinimal.h"

// 전방 선언 (Forward Declarations)
class UAssetManager;
struct FSubUVAtlasResource;

namespace Engine::Component
{
    class ENGINE_API USubUVComponent : public UAtlasComponent
    {
        DECLARE_RTTI(USubUVComponent, UAtlasComponent)

      public:
        USubUVComponent() = default;
        ~USubUVComponent() override = default;

        // SubUV Atlas Path
        const FString& GetSubUVAtlasPath() const { return SubUVAtlasPath; }
        void           SetSubUVAtlasPath(const FString& InPath);

        // SubUV Resource
        FSubUVAtlasResource* GetSubUVAtlasResource() const { return SubUVResource; }
        void                 SetSubUVAtlasResource(FSubUVAtlasResource* InResource);

        // Frame Index
        int32 GetFrameIndex() const { return FrameIndex; }
        void  SetFrameIndex(int32 InFrameIndex);

        // Frame Count 계산
        int32 GetFrameCount() const;

        // UAtlasComponent 오버라이드
        void DescribeProperties(FComponentPropertyBuilder& Builder) override;
        void ResolveAssetReferences(UAssetManager* InAssetManager) override;

      protected:
        FString              SubUVAtlasPath;
        FSubUVAtlasResource* SubUVResource = nullptr;
        int32                FrameIndex = 0;
    };
} // namespace Engine::Component
