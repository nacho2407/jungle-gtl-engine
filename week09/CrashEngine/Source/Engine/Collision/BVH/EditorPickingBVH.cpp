#include "Collision/BVH/EditorPickingBVH.h"

#include "GameFramework/AActor.h"

void FEditorPickingBVH::MarkDirty()
{
    ScenePrimitiveBVH.MarkDirty();
    EditorHelperBVH.MarkDirty();
}

void FEditorPickingBVH::BuildNow(const TArray<AActor*>& Actors)
{
    ScenePrimitiveBVH.BuildNow(Actors);
    EditorHelperBVH.BuildNow(Actors);
}

void FEditorPickingBVH::EnsureBuilt(const TArray<AActor*>& Actors)
{
    ScenePrimitiveBVH.EnsureBuilt(Actors);
    EditorHelperBVH.EnsureBuilt(Actors);
}

bool FEditorPickingBVH::Raycast(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
    FHitResult WorldHit{};
    AActor* WorldActor = nullptr;
    const bool bHitScenePrimitive = ScenePrimitiveBVH.Raycast(Ray, WorldHit, WorldActor);

    FHitResult EditorHit{};
    AActor* EditorActor = nullptr;
    const bool bHitEditorHelper = EditorHelperBVH.Raycast(Ray, EditorHit, EditorActor);

    if (bHitEditorHelper && (!bHitScenePrimitive || EditorHit.Distance < WorldHit.Distance))
    {
        OutHitResult = EditorHit;
        OutActor = EditorActor;
        return true;
    }

    if (bHitScenePrimitive)
    {
        OutHitResult = WorldHit;
        OutActor = WorldActor;
        return true;
    }

    OutHitResult = FHitResult{};
    OutActor = nullptr;
    return false;
}
