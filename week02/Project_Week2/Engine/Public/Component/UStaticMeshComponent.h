#pragma once

#include "Component/UPrimitiveComponent.h"

class UStaticMesh;

class UStaticMeshComponent : public UPrimitiveComponent
{
public:
    UStaticMeshComponent();
    virtual ~UStaticMeshComponent();

public:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;
    virtual const char* GetObjClassName() const override;

public:
    void SetStaticMesh(UStaticMesh* InMesh);
    UStaticMesh* GetStaticMesh() const;

    virtual FRenderItem CreateRenderItem() const override;

private:
    UStaticMesh* StaticMesh;
};