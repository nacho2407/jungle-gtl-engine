#pragma once
#include <vector>
#include "UObject.h"


class AActor;
class UActorComponent;
class UPrimitiveComponent;
class FScene;
class APrimitiveActor;

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

    const TArray<AActor*>& GetActors() const;

private:
    TArray<AActor*> Actors;
    bool bHasBegunPlay;
};