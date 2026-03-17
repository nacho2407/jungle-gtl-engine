#pragma once

#include "Component/UPrimitiveComponent.h"

class UStaticMesh;

class UStaticMeshComponent : public UPrimitiveComponent
{
    DECLARE_UClass(UStaticMeshComponent, UPrimitiveComponent)
public:

    UStaticMeshComponent();
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
