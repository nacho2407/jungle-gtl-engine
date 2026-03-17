#pragma once

#include <vector>
#include "UObject.h"
#include "Actor/ACamera.h"
#include "FUObjectFactory.h"
#include "Component/UStaticMeshComponent.h"
#include "Resource/BuiltInMeshFactory.h"
#include "Actor/AAxisActor.h"
#include "Actor/AActor.h"
#include "Component/UActorComponent.h"
#include "Component/UPrimitiveComponent.h"
#include "World/FScene.h"
#include "Actor/AGizmoActor.h"
#include "Actor/AGridActor.h"



class UWorld : public UObject
{
DECLARE_UClass(UWorld, UObject)
public:
    UWorld();
    ~UWorld();

public:
    void Clear();
    void AddActor(AActor* InActor);
    void RemoveActor(AActor* InActor);

    void BeginPlay();
    void Tick(float DeltaTime);

    void BuildScene(FScene& OutScene) const;

    //ACamera* GetCameraActor();
    //AActor* GetWorldAxisActor();
    //AActor* GetGridActor();
    //AActor* GetGizmoActor();

      

    const TArray<AActor*>& GetActors() const;
public:
    TArray<AActor*> Actors;
    bool bHasBegunPlay;
};