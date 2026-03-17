#pragma once

#include "Actor/APrimitiveActor.h"

class UStaticMesh;
class UStaticMeshComponent;

class ACube : public APrimitiveActor
{
DECLARE_ROOT_UClass(ACube)
public:
    
    ACube();
    virtual ~ACube() override;

public:
    void SetStaticMesh(UStaticMesh* InStaticMesh);
    UStaticMesh* GetStaticMesh() const;

    UStaticMeshComponent* GetStaticMeshComponent() const;

private:
    void InitializeActor();

private:
    UStaticMeshComponent* StaticMeshComponent = nullptr;
};
