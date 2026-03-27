#pragma once

#include "Asset/Asset.h"

struct FStaticMesh;

/**
 * @brief 정적 메시(Static Mesh) 리소스를 관리하는 에셋 클래스입니다.
 * .obj(원본) 또는 .mesh(Bake 가공본) 데이터를 모두 수용합니다.
 */
class ENGINE_API UStaticMesh : public UAsset
{
  public:
    DECLARE_RTTI(UStaticMesh, UAsset)

    UStaticMesh();
    virtual ~UStaticMesh() override;

    /** 메시 데이터 유효성 확인 */
    bool IsValid() const { return StaticMeshAsset != nullptr; }

    /** 메시 데이터 설정 및 반환 */
    void         SetStaticMeshAsset(FStaticMesh* InStaticMesh) { StaticMeshAsset = InStaticMesh; }
    FStaticMesh* GetStaticMeshAsset() const { return StaticMeshAsset; }

    /** 에셋 파일 경로 반환 (발제 요구사항) */
    const FString& GetAssetPathFileName() const { return GetAssetName(); }

  public:
    /** 이 에셋이 Bake된 .mesh 파일에서 로드되었는지 여부 */
    bool bIsBaked = false;

  private:
    /** 실제 메시 데이터를 담고 있는 포인터 */
    FStaticMesh* StaticMeshAsset = nullptr;
};
