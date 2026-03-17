#pragma once

#include "Actor/APrimitiveActor.h"

class UStaticMesh;
class UStaticMeshComponent;

class ATriangle : public APrimitiveActor
{
DECLARE_ROOT_UClass(ATriangle)

public:
    ATriangle();
    virtual ~ATriangle() override;

public:
    void SetStaticMesh(UStaticMesh* InStaticMesh);
    UStaticMesh* GetStaticMesh() const;

    UStaticMeshComponent* GetStaticMeshComponent() const;

private:
    void InitializeActor();

private:
    UStaticMeshComponent* StaticMeshComponent = nullptr;
};