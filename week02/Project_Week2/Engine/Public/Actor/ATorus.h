#pragma once

#include "Actor/APrimitiveActor.h"

class UStaticMesh;
class UStaticMeshComponent;

class ATorus : public APrimitiveActor
{
public:
    DECLARE_ROOT_UClass(ATorus)

    ATorus();
    ATorus(const FUObjectInitializer& ObjectInitializer);
    virtual ~ATorus() override;

public:
    void SetStaticMesh(UStaticMesh* InStaticMesh);
    UStaticMesh* GetStaticMesh() const;

    UStaticMeshComponent* GetStaticMeshComponent() const;

private:
    void InitializeActor();

private:
    UStaticMeshComponent* StaticMeshComponent = nullptr;
};
