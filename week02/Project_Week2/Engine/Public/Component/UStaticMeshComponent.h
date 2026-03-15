#pragma once

#include <string>
#include "Component/UPrimitiveComponent.h"

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
    void SetMeshName(const std::string& InMeshName);
    const std::string& GetMeshName() const;

    virtual FRenderItem CreateRenderItem() const override;

private:
    std::string MeshName;
};