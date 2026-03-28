#pragma once

#include "Asset/StreamableRenderAsset.h"

struct FStaticMesh;

/**
 * @brief 정적 메시(Static Mesh) 리소스를 관리하는 에셋 클래스입니다.
 */
class ENGINE_API UStaticMesh : public UStreamableRenderAsset
{
  public:
    DECLARE_RTTI(UStaticMesh, UStreamableRenderAsset)

    /** 이 에셋이 Bake된 .mesh 파일에서 로드되었는지 여부 */
    bool bIsBaked = false;

    UStaticMesh();
    virtual ~UStaticMesh() override;

    /** 메시 데이터 설정 및 반환 */
    void         SetStaticMeshAsset(FStaticMesh* InStaticMesh) { StaticMeshAsset = InStaticMesh; }
    FStaticMesh* GetStaticMeshAsset() const { return StaticMeshAsset; }

    /** 에셋 파일 경로 반환 */
    const FString& GetAssetPathFileName() const { return GetAssetName(); }

    /** 이진 데이터 직렬화 (Bake 연동) */
    virtual void Serialize(class FArchive& Ar) override;

  private:
    FStaticMesh* StaticMeshAsset = nullptr;
};
