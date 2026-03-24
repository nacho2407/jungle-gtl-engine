#include "Scene.h"

#include "Engine/Game/Actor.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Component/Text/TextComponent.h"
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

        const uint32 ObjectId = Actor->GetObjectId();

        // Primitive
        if (Actor->IsRenderable())
        {
            FPrimitiveRenderItem Item;
            Item.World = Actor->GetWorldMatrix();
            Item.Color = Actor->GetColor();
            Item.MeshType = Actor->GetMeshType();

            Item.State.ObjectId = ObjectId;
            Item.State.SetVisible(Actor->IsVisible());
            Item.State.SetPickable(Actor->IsPickable());
            Item.State.SetSelected(Actor->IsSelected());
            Item.State.SetHovered(Actor->IsHovered());

            OutRenderData.Primitives.push_back(Item);
        }

        // Text components
        const TArray<Engine::Component::USceneComponent*>& OwnedComponents =
            Actor->GetOwnedComponents();

        for (Engine::Component::USceneComponent* Component : OwnedComponents)
        {
            if (Component == nullptr)
            {
                continue;
            }

            auto* TextComponent = dynamic_cast<Engine::Component::UTextComponent*>(Component);
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

            FTextRenderItem TextItem;
            TextItem.World = Actor->GetWorldMatrix();
            TextItem.Color = TextComponent->GetColor();
            TextItem.FontResource = TextComponent->GetFontResource();
            TextItem.Text = TextComponent->GetText();
            TextItem.TextScale = TextComponent->GetTextScale();
            TextItem.LetterSpacing = TextComponent->GetLetterSpacing();
            TextItem.LineSpacing = TextComponent->GetLineSpacing();
            TextItem.bBillboard = TextComponent->GetBillboard();
            TextItem.BillboardOffset = TextComponent->GetBillboardOffset();

            TextItem.State.ObjectId = ObjectId;
            TextItem.State.SetVisible(Actor->IsVisible());
            TextItem.State.SetPickable(false);
            TextItem.State.SetSelected(Actor->IsSelected());
            TextItem.State.SetHovered(Actor->IsHovered());

            OutRenderData.Texts.push_back(TextItem);
        }
    }
}