#pragma once

#include "Containers/Array.h"

class AActor;

class FScene
{
  public:
    ~FScene()
    {
        for (auto &Actor : Actors)
        {
            delete Actor;
        }
        Actors.clear();
    }
    TArray<AActor *> Actors;

    void AddActor(AActor *InActor) { Actors.push_back(InActor); }
};