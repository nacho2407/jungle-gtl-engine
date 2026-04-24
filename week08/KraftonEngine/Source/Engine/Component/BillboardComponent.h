// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Render/Resources/Meshes/PrimitiveMeshTypes.h"
#include "PrimitiveComponent.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Core/ResourceTypes.h"
#include "Object/FName.h"

class FPrimitiveSceneProxy;

// UBillboardComponent 컴포넌트이다.
class UBillboardComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
    FPrimitiveSceneProxy* CreateSceneProxy() override;

    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void SetBillboardEnabled(bool bEnable)
    {
        bIsBillboard = bEnable;
        MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
        MarkWorldBoundsDirty();
    }
    bool IsBillboardEnabled() const { return bIsBillboard; }

    /*
        빌보드에 사용할 머티리얼을 설정합니다.
    */
    void SetMaterial(class UMaterial* InMaterial);
    // UMaterial는 머티리얼 파라미터와 렌더 리소스를 다룹니다.
    class UMaterial* GetMaterial() const { return Material; }

    /*
        월드 공간 기준 스프라이트 크기를 설정합니다.
    */
    void SetSpriteSize(float InWidth, float InHeight)
    {
        Width = InWidth;
        Height = InHeight;
    }
    float GetWidth() const { return Width; }
    float GetHeight() const { return Height; }

    /*
        주어진 카메라 방향을 바라보는 빌보드 월드 행렬을 계산합니다.
    */
    FMatrix ComputeBillboardMatrix(const FVector& CameraForward) const;

    FMeshBuffer* GetMeshBuffer() const override { return &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::Quad); }
    FMeshDataView GetMeshDataView() const override { return FMeshDataView::FromMeshData(FMeshBufferManager::Get().GetMeshData(EMeshShape::Quad)); }
    void UpdateWorldAABB() const override;
    bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

protected:
    bool bIsBillboard = true;

    FMaterialSlot MaterialSlot;
    UMaterial* Material = nullptr;

    float Width = 1.0f;
    float Height = 1.0f;
};
