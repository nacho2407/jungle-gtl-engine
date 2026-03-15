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

    const std::vector<AActor*>& GetActors() const;

private:
    std::vector<AActor*> Actors;
    bool bHasBegunPlay;
};