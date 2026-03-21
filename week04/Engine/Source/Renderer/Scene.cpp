#include "Scene.h"

#include "Engine/Game/Actor.h"

FScene::~FScene()
{
    for (auto& Actor : Actors)
    {
        delete Actor;
    }
    Actors.clear();
}
