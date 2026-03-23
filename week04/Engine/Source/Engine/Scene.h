#pragma once

#include "Core/Containers/Array.h"
#include "Renderer/SceneRenderData.h"

class AActor;

class ENGINE_API FScene
{
  public:
    ~FScene();

    void AddActor(AActor* InActor) { Actors.push_back(InActor); }
    const TArray<AActor*>& GetActors() const { return Actors; }

    void BuildRenderData(FSceneRenderData& OutRenderData) const;
    void Clear();

  private:
    TArray<AActor*> Actors;
};
