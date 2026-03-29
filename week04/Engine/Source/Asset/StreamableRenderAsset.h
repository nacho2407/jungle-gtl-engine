#pragma once

#include "Asset/Asset.h"

namespace Engine::Asset
{
    /**
     * @brief LOD 및 스트리밍 관리가 필요한 모든 렌더링 에셋의 최상위 클래스입니다. (UE 표준)
     */
    class ENGINE_API UStreamableRenderAsset : public UAsset
    {
      public:
        DECLARE_RTTI(UStreamableRenderAsset, UAsset)

        UStreamableRenderAsset();
        virtual ~UStreamableRenderAsset() override;

        /** 이진 파일 직렬화 인터페이스 */
        virtual void Serialize(class FArchive& Ar);

      protected:
        /** 스트리밍 및 LOD 제어용 변수들 */
        int32 NumLODs = 1;
        bool  bIsStreamable = true;
    };
}
