#pragma once

#include "Core/Containers/Array.h"

class AActor;

class FScene
{
  public:
    ~FScene();
    TArray<AActor *> Actors;

    void AddActor(AActor *InActor) { Actors.push_back(InActor); }
};
