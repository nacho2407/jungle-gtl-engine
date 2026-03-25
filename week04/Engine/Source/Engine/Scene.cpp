#include "Scene.h"

#include <algorithm>

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Component/Sprite/SpriteComponent.h"
#include "Engine/Component/Sprite/SubUVComponent.h"
#include "Engine/Component/Text/AtlasTextComponent.h"
#include "Engine/Game/Actor.h"
#include "Renderer/RenderAsset/SubUVAtlasResource.h"
#include "Renderer/Types/RenderItem.h"

namespace
{
    void ResolveSpriteUVs(const Engine::Component::USpriteComponent& SpriteComponent,
                          FVector2& OutUVMin, FVector2& OutUVMax)
    {
        OutUVMin = FVector2(0.0f, 0.0f);
        OutUVMax = FVector2(1.0f, 1.0f);

        const auto* SubUVComponent =
            dynamic_cast<const Engine::Component::USubUVComponent*>(&SpriteComponent);
        if (SubUVComponent == nullptr)
        {
            return;
        }

        if (const FSubUVAtlasResource* AtlasResource = SubUVComponent->GetSubUVAtlasResource())
        {
            const int32 FrameCount = std::max(SubUVComponent->GetFrameCount(), 1);
            const int32 FrameIndex = std::clamp(SubUVComponent->GetFrameIndex(), 0, FrameCount - 1);
            const FSubUVFrame* Frame = AtlasResource->FindFrame(static_cast<uint32>(FrameIndex));

            const float AtlasWidth = static_cast<float>(
                std::max(AtlasResource->Common.ScaleW, AtlasResource->Atlas.Width));
            const float AtlasHeight = static_cast<float>(
                std::max(AtlasResource->Common.ScaleH, AtlasResource->Atlas.Height));
            if (Frame != nullptr && AtlasWidth > 0.0f && AtlasHeight > 0.0f)
            {
                OutUVMin = FVector2(static_cast<float>(Frame->X) / AtlasWidth,
                                    static_cast<float>(Frame->Y) / AtlasHeight);
                OutUVMax = FVector2(static_cast<float>(Frame->X + Frame->Width) / AtlasWidth,
                                    static_cast<float>(Frame->Y + Frame->Height) / AtlasHeight);
                return;
            }
        }

        const int32 Columns = std::max(SubUVComponent->GetAtlasColumns(), 1);
        const int32 Rows = std::max(SubUVComponent->GetAtlasRows(), 1);
        const int32 FrameCount = std::max(Columns * Rows, 1);
        const int32 FrameIndex = std::clamp(SubUVComponent->GetFrameIndex(), 0, FrameCount - 1);

        const int32 ColumnIndex = FrameIndex % Columns;
        const int32 RowIndex = FrameIndex / Columns;

        const float InvColumns = 1.0f / static_cast<float>(Columns);
        const float InvRows = 1.0f / static_cast<float>(Rows);

        OutUVMin = FVector2(ColumnIndex * InvColumns, RowIndex * InvRows);
        OutUVMax = FVector2((ColumnIndex + 1) * InvColumns, (RowIndex + 1) * InvRows);
    }
} // namespace

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

void FScene::Tick(float DeltaTime)
{
    for (auto& actor : Actors)
    {
	    if (actor)
	    {
            actor->Tick(DeltaTime);
	    }
    }
}

void FScene::BuildRenderData(FSceneRenderData& OutRenderData) const
{
    OutRenderData.Primitives.clear();
    OutRenderData.Sprites.clear();
    OutRenderData.Texts.clear();

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

        const uint32                                       ObjectId = Actor->GetObjectId();
        const TArray<Engine::Component::USceneComponent*>& OwnedComponents =
            Actor->GetOwnedComponents();

        for (Engine::Component::USceneComponent* Component : OwnedComponents)
        {
            if (Component == nullptr)
            {
                continue;
            }

#pragma region __SPRITE__
            if (auto* SpriteComponent = Cast<Engine::Component::USpriteComponent>(Component))
            {
                if (Cast<Engine::Component::UAtlasTextComponent>(SpriteComponent) != nullptr)
                {
                    continue;
                }

                FSpriteRenderItem SpriteItem = {};
                SpriteItem.TextureResource = SpriteComponent->GetTextureResource();
                SpriteItem.Color = SpriteComponent->GetColor();
                ResolveSpriteUVs(*SpriteComponent, SpriteItem.UVMin, SpriteItem.UVMax);
                SpriteItem.Placement.Mode = SpriteComponent->GetBillboard()
                                                ? ERenderPlacementMode::WorldBillboard
                                                : ERenderPlacementMode::World;
                SpriteItem.Placement.World = Actor->GetWorldMatrix();
                SpriteItem.Placement.WorldOffset = SpriteComponent->GetBillboardOffset();

                SpriteItem.State.ObjectId = ObjectId;
                SpriteItem.State.bShowBounds = Actor->IsShowBounds();
                SpriteItem.State.SetVisible(Actor->IsVisible());
                SpriteItem.State.SetPickable(Actor->IsPickable());
                SpriteItem.State.SetSelected(Actor->IsSelected());
                SpriteItem.State.SetHovered(Actor->IsHovered());

                OutRenderData.Sprites.push_back(SpriteItem);
            }
#pragma endregion

#pragma region __ATLAS_TEXT__
            else if (auto* TextComponent = Cast<Engine::Component::UAtlasTextComponent>(Component))
            {
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
                TextItem.Placement.Mode = TextComponent->GetBillboard()
                                              ? ERenderPlacementMode::WorldBillboard
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

#pragma region __PRIMITIVE__
            else if (auto* PrimitiveComponent =
                         Cast<Engine::Component::UPrimitiveComponent>(Component))
            {
                FPrimitiveRenderItem PrimitiveItem = {};
                PrimitiveItem.World = Actor->GetWorldMatrix();
                PrimitiveItem.Color = Actor->GetColor();
                PrimitiveItem.MeshType = Actor->GetMeshType();
                PrimitiveItem.WorldAABB = PrimitiveComponent->GetWorldAABB();
                PrimitiveItem.bHasWorldAABB = true;

                PrimitiveItem.State.ObjectId = ObjectId;
                PrimitiveItem.State.bShowBounds = Actor->IsShowBounds();
                PrimitiveItem.State.SetVisible(Actor->IsVisible());
                PrimitiveItem.State.SetPickable(Actor->IsPickable());
                PrimitiveItem.State.SetSelected(Actor->IsSelected());
                PrimitiveItem.State.SetHovered(Actor->IsHovered());

                OutRenderData.Primitives.push_back(PrimitiveItem);
            }
#pragma endregion
        }
    }
}