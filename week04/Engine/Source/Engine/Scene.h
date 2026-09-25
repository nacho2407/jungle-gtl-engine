#pragma once

#include "Core/CoreMinimal.h"

#include "Renderer/SceneFrameRenderData.h"
#include "Renderer/Types/SceneShowFlags.h"

class AActor;

class ENGINE_API FScene
{
  public:
    ~FScene();

    void                   AddActor(AActor* InActor) { Actors.push_back(InActor); }
    bool                   RemoveActor(AActor* InActor);
    const TArray<AActor*>& GetActors() const { return Actors; }

    void Tick(float DeltaTime);

    void BuildRenderData(FSceneFrameRenderData& OutRenderData, ESceneShowFlags InShowFlags) const;
    void Clear();

    TArray<AActor*>* GetActors() { return &Actors; }

  private:
    TArray<AActor*> Actors;
};
