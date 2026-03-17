#pragma once

#include "Component/UPrimitiveComponent.h"

class UStaticMesh;

class UStaticMeshComponent : public UPrimitiveComponent
{
public:
    DECLARE_ROOT_UClass(UStaticMeshComponent)

    UStaticMeshComponent();
    UStaticMeshComponent(const FUObjectInitializer& ObjectInitializer);
    virtual ~UStaticMeshComponent();

public:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;

public:
    void SetStaticMesh(UStaticMesh* InMesh);
    UStaticMesh* GetStaticMesh() const;

    virtual FRenderItem CreateRenderItem() const override;

private:
    UStaticMesh* StaticMesh = nullptr;
};
