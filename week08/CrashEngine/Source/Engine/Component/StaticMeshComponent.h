// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Component/MeshComponent.h"
#include "Core/PropertyTypes.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"

class UMaterial;
class FPrimitiveProxy;

namespace json
{
class JSON;
}

// UStaticMeshComponent 컴포넌트이다.
class UStaticMeshComponent : public UMeshComponent
{
public:
    DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)

    UStaticMeshComponent() = default;
    ~UStaticMeshComponent() override = default;

    FMeshBuffer* GetMeshBuffer() const override;
    FMeshDataView GetMeshDataView() const override;
    bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;
    bool LineTraceStaticMeshFast(const FRay& Ray, const FMatrix& WorldMatrix, const FMatrix& WorldInverse, FHitResult& OutHitResult);
    void UpdateWorldAABB() const override;

    // 구체 프록시 생성 (FStaticMeshSceneProxy)
    FPrimitiveProxy* CreateSceneProxy() override;

    void SetStaticMesh(UStaticMesh* InMesh);
    UStaticMesh* GetStaticMesh() const;

    void SetMaterial(int32 ElementIndex, UMaterial* InMaterial) override;
    UMaterial* GetMaterial(int32 ElementIndex) const override;
    const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }

    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    // Property Editor 지원
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    const FString& GetStaticMeshPath() const { return StaticMeshPath; }

private:
    void CacheLocalBounds();

    UStaticMesh* StaticMesh = nullptr;
    FString StaticMeshPath = "None";
    TArray<UMaterial*> OverrideMaterials;
    TArray<FMaterialSlot> MaterialSlots; // 경로 + UVScroll 묶음

    FVector CachedLocalCenter = { 0, 0, 0 };
    FVector CachedLocalExtent = { 0.5f, 0.5f, 0.5f };
    bool bHasValidBounds = false;
};

