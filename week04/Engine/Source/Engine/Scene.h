#pragma once

#include "Core/Containers/Array.h"
#include "Renderer/SceneRenderData.h"

class AActor;

class ENGINE_API FScene
{
  public:
    ~FScene();

    void AddActor(AActor* InActor) { Actors.push_back(InActor); }

    void BuildRenderData(FSceneRenderData& OutRenderData) const;
    void Clear();

  private:
    TArray<AActor*> Actors;
};
