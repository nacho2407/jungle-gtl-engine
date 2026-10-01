#pragma once

#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Materials/MaterialCore.h"
#include <memory>

class UStaticMeshComponent;

// FStaticMeshSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FStaticMeshSceneProxy : public FPrimitiveProxy
{
public:
    static constexpr uint32 MAX_LOD = 4;

    FStaticMeshSceneProxy(UStaticMeshComponent* InComponent);

    void UpdateMaterial() override;
    void UpdateMesh() override;
    void UpdateLOD(uint32 LODLevel) override;

private:
    UStaticMeshComponent* GetStaticMeshComponent() const;

    // 모든 LOD의 SectionRenderData 재구축
    void RebuildSectionRenderData();

    // FLODDrawData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
    struct FLODDrawData
    {
        FMeshBuffer*                                     MeshBuffer = nullptr;
        TArray<FMeshSectionRenderData>                   SectionRenderData;
        TArray<std::unique_ptr<FMaterialConstantBuffer>> OwnedMaterialCBs;
    };

    FLODDrawData                                     LODData[MAX_LOD];
    TArray<std::unique_ptr<FMaterialConstantBuffer>> ActiveOwnedMaterialCBs;
    uint32                                           LODCount = 1;
};
