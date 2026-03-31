#include "Scene.h"

#include <algorithm>

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Component/Text/UUIDComponent.h"
#include "Engine/Component/Sprite/SpriteComponent.h"
#include "Engine/Component/Sprite/SubUVComponent.h"
#include "Engine/Component/Text/AtlasTextComponent.h"
#include "Engine/Component/Mesh/StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Engine/Game/Actor.h"
#include "Renderer/RenderAsset/SubUVAtlasResource.h"
#include "Renderer/Types/RenderItem.h"
#include "Core/Misc/BitMaskEnum.h"

namespace
{
    void ResolveSpriteUVs(const Engine::Component::USpriteComponent& SpriteComponent,
                          FVector2& OutUVMin, FVector2& OutUVMax)
    {
        OutUVMin = FVector2(0.0f, 0.0f);
        OutUVMax = FVector2(1.0f, 1.0f);

        /*const auto* SubUVComponent =
            Cast<Engine::Component::USubUVComponent>(SpriteComponent.GetClass());*/
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

void FScene::BuildRenderData(FSceneFrameRenderData& OutRenderData, ESceneShowFlags InShowFlags) const
{
    OutRenderData.StaticMeshes.clear();
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

#pragma region __ATLAS_TEXT__
            if (auto* TextComponent = Cast<Engine::Component::UAtlasTextComponent>(Component))
            {
                if (TextComponent->GetText().empty())
                {
                    continue;
                }

                if (!IsFlagSet(InShowFlags, ESceneShowFlags::SF_BillboardText) &&
                    TextComponent->GetBillboard())
                {
                    continue;
                }

                auto* UUIDTextComponent = Cast<Engine::Component::UUUIDComponent>(TextComponent);
                if (!IsFlagSet(InShowFlags, ESceneShowFlags::SF_UUIDText) && UUIDTextComponent)
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

#pragma region __STATIC_MESH__
            else if (auto* StaticMeshComp =
                         Cast<Engine::Component::UStaticMeshComponent>(Component))
            {
                // (선택 사항) ShowFlags에 SF_StaticMeshes 같은 플래그가 있다면 여기서 체크
                // if (!IsFlagSet(InShowFlags, ESceneShowFlags::SF_StaticMeshes)) continue;

                Engine::Asset::UStaticMesh* StaticMeshAsset = StaticMeshComp->GetStaticMesh();
                if (StaticMeshAsset == nullptr || StaticMeshAsset->GetRenderResource() == nullptr)
                {
                    continue;
                }

                FStaticMeshRenderItem MeshItem = {};

                // 1. 트랜스폼 데이터 세팅
                MeshItem.World =
                    StaticMeshComp->GetWorldMatrix(); // (또는 컴포넌트의 월드 매트릭스)

                // 2. VBO/IBO가 들어있는 렌더 리소스 포인터 전달
                MeshItem.RenderResource = StaticMeshAsset->GetRenderResource();

                // 3. 서브 메시 개수만큼 매핑된 머티리얼 포인터 수집
                uint32 NumSubMeshes =
                    static_cast<uint32>(MeshItem.RenderResource->SubMeshes.size());
                for (uint32 i = 0; i < NumSubMeshes; ++i)
                {
                    FStaticMeshMaterialBinding Binding = {};
                    Binding.Material = StaticMeshComp->GetMaterial(i);
                    Binding.SubMaterialName = StaticMeshComp->GetSubMaterialName(i);
                    MeshItem.MaterialBindings.push_back(Binding);
                }

                // 4. 상태 및 피킹 데이터
                MeshItem.WorldAABB = StaticMeshComp->GetWorldAABB();
                MeshItem.State.ObjectId = ObjectId;
                MeshItem.State.bShowBounds = StaticMeshComp->IsShowBounds();
                MeshItem.State.SetVisible(Actor->IsVisible());
                MeshItem.State.SetPickable(Actor->IsPickable());
                MeshItem.State.SetSelected(Actor->IsSelected());
                MeshItem.State.SetHovered(Actor->IsHovered());

                // 5. 렌더 데이터 큐에 삽입
                OutRenderData.StaticMeshes.push_back(MeshItem);
            }
#pragma endregion

#pragma region __SPRITE__
            else if (auto* SpriteComponent = Cast<Engine::Component::USpriteComponent>(Component))
            {
                if (!IsFlagSet(InShowFlags, ESceneShowFlags::SF_Sprites) ||
                    Cast<Engine::Component::UAtlasTextComponent>(SpriteComponent) != nullptr)
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

//#pragma region __PRIMITIVE__
//            else if (auto* PrimitiveComponent =
//                         Cast<Engine::Component::UPrimitiveComponent>(Component))
//            {
//                if (!IsFlagSet(InShowFlags, ESceneShowFlags::SF_Primitives))
//                {
//                    continue;
//                }
//
//                FPrimitiveRenderItem PrimitiveItem = {};
//                PrimitiveItem.World = PrimitiveComponent->GetWorldMatrix();
//                PrimitiveItem.Color = Actor->GetColor();
//                PrimitiveItem.MeshType = Actor->GetMeshType();
//                PrimitiveItem.WorldAABB = PrimitiveComponent->GetWorldAABB();
//                PrimitiveItem.bHasWorldAABB = true;
//
//                PrimitiveItem.State.ObjectId = ObjectId;
//                PrimitiveItem.State.bShowBounds = PrimitiveComponent->IsShowBounds();
//                PrimitiveItem.State.SetVisible(Actor->IsVisible());
//                PrimitiveItem.State.SetPickable(Actor->IsPickable());
//                PrimitiveItem.State.SetSelected(Actor->IsSelected());
//                PrimitiveItem.State.SetHovered(Actor->IsHovered());
//
//                OutRenderData.Primitives.push_back(PrimitiveItem);
//            }
//#pragma endregion
        }
    }
}
