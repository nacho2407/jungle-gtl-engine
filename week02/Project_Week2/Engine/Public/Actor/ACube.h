#pragma once

#include "Actor/APrimitiveActor.h"

class UStaticMesh;
class UStaticMeshComponent;

class ACube : public APrimitiveActor
{
public:
    DECLARE_ROOT_UClass(ACube)
    ACube();
    //ACube(const FUObjectInitializer& ObjectInitializer);
    virtual ~ACube() override;

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
