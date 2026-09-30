#pragma once

#include "Engine/Core/CoreTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Collision/BVH/EditorHelperBVH.h"
#include "Collision/BVH/ScenePrimitiveBVH.h"

class AActor;

class FEditorPickingBVH
{
public:
    void MarkDirty();
    void BuildNow(const TArray<AActor*>& Actors);
    void EnsureBuilt(const TArray<AActor*>& Actors);
    bool Raycast(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const;

    const FScenePrimitiveBVH& GetScenePrimitiveBVH() const { return ScenePrimitiveBVH; }
    const FEditorHelperBVH& GetEditorHelperBVH() const { return EditorHelperBVH; }

private:
    mutable FScenePrimitiveBVH ScenePrimitiveBVH;
    mutable FEditorHelperBVH EditorHelperBVH;
};
