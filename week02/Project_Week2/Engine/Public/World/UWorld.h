#pragma once

#include <vector>
#include "UObject.h"
#include "Actor/ACamera.h"
#include "FUObjectFactory.h"
#include "Component/UStaticMeshComponent.h"
#include "Resource/BuiltInMeshFactory.h"

class UWorld : public UObject
{
public:
    UWorld();
    ~UWorld();

public:
    void AddActor(AActor* InActor);

    void BeginPlay();
    void Tick(float DeltaTime);

    void BuildScene(FScene& OutScene) const;

    ACamera* GetCameraActor();
    //AActor* GetWorldAxisActor();
    //AActor* GetGridActor();
    //AActor* GetGizmoActor();

       
    const TArray<AActor*>& GetActors() const;
private:
    ACamera* Camera;
    AActor* WorldAxisActor;
    AActor* GridActor;
    AActor* GizmoActor;
    TArray<AActor*> Actors;
    bool bHasBegunPlay;
};