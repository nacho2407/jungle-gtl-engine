#pragma once
#include "MeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Render/Resource/Material.h"

#include "StaticMeshComponent.generated.h"

UCLASS()
class UStaticMeshComponent : public UMeshComponent
{
    GENERATED_BODY_UStaticMeshComponent()

public:
    DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)
    UStaticMeshComponent();

    virtual void PostDuplicate(UObject* Original) override;

    virtual void Serialize(FArchive& Ar) override;

    void SetStaticMesh(UStaticMesh* InStaticMesh);
    UStaticMesh* GetStaticMesh() const;
    bool HasValidMesh() const;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_StaticMesh; }

    const FAABB& GetWorldAABB() const override;

    bool ConsumeRenderStateDirty();

    void GetMeshData(TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices) const;

private:
    void MarkBoundsDirty();
    void MarkRenderStateDirty();
    void EnsureBoundsUpdated() const;

private:
    UPROPERTY(EditAnywhere)
    UStaticMesh* StaticMeshAsset = nullptr;

    UPROPERTY(EditAnywhere)
    FString StaticMeshAssetPath;

    mutable bool bBoundsDirty = true;
    bool bRenderStateDirty = true;
};
