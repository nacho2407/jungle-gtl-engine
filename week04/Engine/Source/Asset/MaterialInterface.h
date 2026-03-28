#pragma once

#include "Asset/Asset.h"
#include "Renderer/RenderAsset/MaterialResource.h"
#include <memory>

namespace Engine::Asset
{
    /**
     * @brief 메시(Mesh)에 적용될 수 있는 모든 시각적 질감의 추상 기반 클래스입니다.
     */
    class ENGINE_API UMaterialInterface : public UAsset
    {
      public:
        DECLARE_RTTI(UMaterialInterface, UAsset)

        UMaterialInterface();
        virtual ~UMaterialInterface() override;

        /** 에셋 로더로부터 전체 머티리얼 리소스를 주입받습니다. */
        virtual void Initialize(std::shared_ptr<FMaterialResource> InResource) { SharedResource = InResource; }

        /** 공통 리소스 접근 */
        const FMaterialResource* GetSharedResource() const { return SharedResource.get(); }

        virtual void Serialize(class FArchive& Ar);

      protected:
        std::shared_ptr<FMaterialResource> SharedResource;
    };
} // namespace Engine::Asset
