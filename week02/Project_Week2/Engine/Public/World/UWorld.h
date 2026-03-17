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
#include "Actor/APrimitiveActor.h"
#include "World/ESpawnMeshType.h"

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

    void SpawnMeshActor(
        ESpawnMeshType Type, 
        const FVector& Location = { 0.f, 0.f, 0.f },
        const FVector& Rotation = { 0.f, 0.f, 0.f },
        const FVector& Scale = { 1.f, 1.f, 1.f }
    );

    void BeginPlay();
    void Tick(float DeltaTime);

    void BuildScene(FScene& OutScene) const;

    const TArray<AActor*>& GetActors() const;
public:
    TArray<AActor*> EditorOnly;
    TArray<AActor*> Actors;
    bool bHasBegunPlay;
};
