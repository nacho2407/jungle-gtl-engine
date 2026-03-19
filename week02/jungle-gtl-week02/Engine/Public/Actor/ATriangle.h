#pragma once

#include "Core.h"
#include "Actor/APrimitiveActor.h"

class UStaticMesh;
class UStaticMeshComponent;

class ATriangle : public APrimitiveActor
{
    DECLARE_UClass(ATriangle, APrimitiveActor)

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