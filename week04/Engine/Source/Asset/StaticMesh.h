#pragma once

#include "Asset/StreamableRenderAsset.h"
#include "Renderer/RenderAsset/StaticMeshResource.h"
#include <memory>

/**
 * @brief 정적 메시(Static Mesh) 리소스를 관리하는 에셋 클래스입니다.
 * 언리얼 엔진의 UStaticMesh 구조를 모방하며, 실제 데이터는 FStaticMeshResource가 소유합니다.
 */
class ENGINE_API UStaticMesh : public UStreamableRenderAsset
{
  public:
    DECLARE_RTTI(UStaticMesh, UStreamableRenderAsset)

    /** 이 에셋이 Bake된 .mesh 파일에서 로드되었는지 여부 */
    bool bIsBaked = false;

    UStaticMesh();
    virtual ~UStaticMesh() override;

    /** 
     * 에셋 로더로부터 리소스를 주입받아 초기화합니다.
     */
    void Initialize(std::shared_ptr<FStaticMeshResource> InResource);

    /** 렌더링 리소스 접근 */
    const FStaticMeshResource* GetRenderResource() const { return RenderResource.get(); }
    FStaticMeshResource*       GetRenderResource() { return RenderResource.get(); }

    /** 에셋 파일 경로 반환 */
    const FString& GetAssetPathFileName() const { return GetAssetName(); }

    /** 이 메시가 가진 섹션(재질 구역)의 개수를 관리합니다. */
    uint32 GetNumSections() const { return NumSections; }
    void   SetNumSections(uint32 InNum) { NumSections = InNum; }

    /** 이진 데이터 직렬화 (Bake 연동) */
    virtual void Serialize(class FArchive& Ar);

  private:
    std::shared_ptr<FStaticMeshResource> RenderResource;
    uint32                               NumSections = 1; // 기본값은 1개 섹션
};
   