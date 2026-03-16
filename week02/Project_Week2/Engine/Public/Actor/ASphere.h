#pragma once

#include "Actor/APrimitiveActor.h"

class UStaticMesh;
class UStaticMeshComponent;

class ASphere : public APrimitiveActor
{
public:
    ASphere();
    ASphere(const FUObjectInitializer& ObjectInitializer);
    virtual ~ASphere() override;

public:
    virtual const char* GetObjClassName() const override;

public:
    void SetStaticMesh(UStaticMesh* InStaticMesh);
    UStaticMesh* GetStaticMesh() const;

    UStaticMeshComponent* GetStaticMeshComponent() const;

private:
    void InitializeActor();

private:
    UStaticMeshComponent* StaticMeshComponent = nullptr;
};
