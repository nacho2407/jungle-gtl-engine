#include "Scene.h"

#include "Engine/Game/Actor.h"

FScene::~FScene() { Clear(); }

bool FScene::RemoveActor(AActor* InActor)
{
    if (InActor == nullptr)
    {
        return false;
    }

    for (auto Iterator = Actors.begin(); Iterator != Actors.end(); ++Iterator)
    {
        if (*Iterator != InActor)
        {
            continue;
        }

        delete InActor;
        Actors.erase(Iterator);
        return true;
    }

    return false;
}

void FScene::Clear()
{
    for (AActor* Actor : Actors)
    {
        delete Actor;
    }
    Actors.clear();
}

void FScene::BuildRenderData(FSceneRenderData& OutRenderData) const
{
    // TODO
    OutRenderData.Primitives.clear();

    for (AActor* Actor : Actors)
    {
        if (Actor == nullptr)
        {
            continue;
        }

        if (!Actor->IsRenderable())
        {
            continue;
        }

        FPrimitiveRenderItem Item;
        Item.World = Actor->GetWorldMatrix();
        Item.Color = Actor->GetColor();
        Item.MeshType = Actor->GetMeshType();
        Item.ObjectId = Actor->GetObjectId();

        Item.bVisible = Actor->IsVisible();
        Item.bPickable = Actor->IsPickable();
        Item.bSelected = Actor->IsSelected();
        Item.bHovered = Actor->IsHovered();

        OutRenderData.Primitives.push_back(Item);
    }
}
