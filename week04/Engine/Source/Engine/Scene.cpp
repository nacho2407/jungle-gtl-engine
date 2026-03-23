#include "Scene.h"

#include "Engine/Game/Actor.h"

FScene::~FScene() { Clear(); }

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
    
    OutRenderData.Primitives.push_back(
        FPrimitiveRenderItem{.World = FMatrix::MakeScale({10.f, 10.f, 10.f}),
                             .Color = FColor::Blue(),
                             .MeshType = EBasicMeshType::Cube,
                             .ObjectId = 0,
                             .bVisible = true,
                             .bPickable = false,
                             .bSelected = false,
                             .bHovered = false});
    // OutRenderData.ShowFlags = ESceneShowFlags::None;
}
