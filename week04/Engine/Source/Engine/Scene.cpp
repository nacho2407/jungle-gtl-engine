#include "Scene.h"

#include "Engine/Game/Actor.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Component/Text/AtlasTextComponent.h"
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

            // TODO
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
            
            TextItem.Placement.Mode = ERenderPlacementMode::WorldBillboard;         //  일단 기본적으로 WorldBillboard
            TextItem.Placement.World = Actor->GetWorldMatrix();
            TextItem.Placement.WorldOffset = TextComponent->GetRelativeLocation();  //  이거 맞나
            
            TextItem.TextScale = TextComponent->GetTextScale();
            TextItem.LetterSpacing = TextComponent->GetLetterSpacing();
            TextItem.LineSpacing = TextComponent->GetLineSpacing();

            TextItem.State.ObjectId = ObjectId;
            TextItem.State.bShowBounds = Actor->IsShowBounds();                     //  나중에 False로 바꿔도 될 것 같긴 함
            TextItem.State.SetVisible(Actor->IsVisible());
            TextItem.State.SetPickable(false);
            TextItem.State.SetSelected(Actor->IsSelected());
            TextItem.State.SetHovered(Actor->IsHovered());

            OutRenderData.Texts.push_back(TextItem);
        }
#pragma endregion
        
#pragma region __SPRITE__
        // for (Engine::
        //        //     if (ComponComponent::USceneComponent* Component : OwnedComponents)
        // {ent == nullptr)
        //     {
        //         continue;
        //     }
        //
        //     // TODO
        //     auto* SpriteComponent = dynamic_cast<Engine::Component::USpriteComponent*>(Component);
        //     if (SpriteComponent == nullptr)
        //     {
        //         continue;
        //     }
        //     
        //     FSpriteRenderItem SpriteItem = {};
        //     SpriteItem.TextureResource = SpriteComponent->GetTextureResource();
        //     SpriteItem.Color = SpriteComponent->GetColor();
        // SpriteItem.UVMax = SpriteComponent->
        //     
        //     
        //     OutRenderData.Sprites.push_back(SpriteItem);
        // }
        
#pragma endregion
        
    }
}