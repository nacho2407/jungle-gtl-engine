#pragma once

#include "Asset/Asset.h"
#include "Renderer/RenderAsset/MaterialResource.h" // FMaterialData 구조체 사용을 위해 포함

namespace Engine::Asset
{
    /**
     * @brief 메시(Mesh)에 적용될 수 있는 모든 시각적 질감의 추상 기반 클래스입니다.
     * 원본 에셋(UMaterialAsset)과 동적 인스턴스(UMaterialInstance)의 공통 부모가 됩니다.
     */
    class ENGINE_API UMaterialInterface : public UAsset
    {
      public:
        DECLARE_RTTI(UMaterialInterface, UAsset)

        UMaterialInterface() = default;

        // 가상 소멸자 (LNK 에러 방지를 위해 = default 명시)
        virtual ~UMaterialInterface() = default;

        // ------------------------------------------------------------------------
        // [다형성 핵심 인터페이스]
        // 렌더러가 특정 이름(usemtl)의 재질 데이터를 요구할 때 호출합니다.
        // - UMaterialAsset: 자신이 들고 있는 Resource 맵에서 찾아 반환
        // - UMaterialInstance: 오버라이드된 데이터가 있으면 덮어씌워서 반환, 없으면 부모에게 요청
        // ------------------------------------------------------------------------
        virtual const FMaterialData* GetMaterialData(const FString& SubMaterialName) const = 0;

        // 공통 직렬화 인터페이스 (LNK 에러 방지를 위해 빈 중괄호 구현)
        virtual void Serialize(class FArchive& Ar) {}
    };
} // namespace Engine::Asset
