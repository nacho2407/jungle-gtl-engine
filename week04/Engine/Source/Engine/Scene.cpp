#include "Scene.h"

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Component/Text/AtlasTextComponent.h"
#include "Engine/Game/Actor.h"
#include "Renderer/Types/RenderItem.h"

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
    OutRenderData.Primitives.clear();
    OutRenderData.Texts.clear();

    for (AActor* Actor : Actors)
    {
        if (Actor == nullptr)
        {
            continue;
        }

        if (!Actor->IsRenderable())
        {
            return;
        }

        const uint32 ObjectId = Actor->GetObjectId();

#pragma region __PRIMITIVE__
        FPrimitiveRenderItem PrimitiveItem = {};
        PrimitiveItem.World = Actor->GetWorldMatrix();
        PrimitiveItem.Color = Actor->GetColor();
        PrimitiveItem.MeshType = Actor->GetMeshType();

        if (auto* PrimitiveComponent =
                Cast<Engine::Component::UPrimitiveComponent>(Actor->GetRootComponent()))
        {
            PrimitiveItem.WorldAABB = PrimitiveComponent->GetWorldAABB();
            PrimitiveItem.bHasWorldAABB = true;
        }

        PrimitiveItem.State.ObjectId = ObjectId;
        PrimitiveItem.State.bShowBounds = Actor->IsShowBounds();
        PrimitiveItem.State.SetVisible(Actor->IsVisible());
        PrimitiveItem.State.SetPickable(Actor->IsPickable());
        PrimitiveItem.State.SetSelected(Actor->IsSelected());
        PrimitiveItem.State.SetHovered(Actor->IsHovered());

        OutRenderData.Primitives.push_back(PrimitiveItem);
#pragma endregion

#pragma region __ATLAS_TEXT__
        const TArray<Engine::Component::USceneComponent*>& OwnedComponents =
            Actor->GetOwnedComponents();

        for (Engine::Component::USceneComponent* Component : OwnedComponents)
        {
            if (Component == nullptr)
            {
                continue;
            }

            auto* TextComponent = dynamic_cast<Engine::Component::UAtlasTextComponent*>(Component);
            if (TextComponent == nullptr)
            {
                continue;
            }

            if (TextComponent->GetText().empty())
            {
                continue;
            }

            if (TextComponent->GetFontResource() == nullptr)
            {
                continue;
            }

            FTextRenderItem TextItem = {};
            TextItem.FontResource = TextComponent->GetFontResource();
            TextItem.Text = TextComponent->GetText();
            TextItem.Color = TextComponent->GetColor();
            TextItem.Placement.Mode =
                TextComponent->GetBillboard() ? ERenderPlacementMode::WorldBillboard
                                              : ERenderPlacementMode::World;
            TextItem.Placement.World = TextComponent->GetRenderPlacementWorld(*Actor);
            TextItem.Placement.WorldOffset = TextComponent->GetRenderPlacementOffset(*Actor);
            TextItem.TextScale = TextComponent->GetTextScale();
            TextItem.LetterSpacing = TextComponent->GetLetterSpacing();
            TextItem.LineSpacing = TextComponent->GetLineSpacing();

            TextItem.State.ObjectId = ObjectId;
            TextItem.State.bShowBounds = Actor->IsShowBounds();
            TextItem.State.SetVisible(Actor->IsVisible());
            TextItem.State.SetPickable(false);
            TextItem.State.SetSelected(Actor->IsSelected());
            TextItem.State.SetHovered(Actor->IsHovered());

            OutRenderData.Texts.push_back(TextItem);
        }
#pragma endregion
    }
}
