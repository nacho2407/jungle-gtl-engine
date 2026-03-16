#pragma once

#include "Actor/APrimitiveActor.h"

class UStaticMesh;
class UStaticMeshComponent;

class ATriangle : public APrimitiveActor
{
public:
    ATriangle();
    ATriangle(const FUObjectInitializer& ObjectInitializer);
    virtual ~ATriangle() override;

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