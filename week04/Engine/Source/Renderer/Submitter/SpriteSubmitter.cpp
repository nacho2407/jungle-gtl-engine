#include "Renderer/Submitter/SpriteSubmitter.h"

#include "Renderer/D3D11/D3D11LineBatchRenderer.h"
#include "Renderer/D3D11/D3D11SpriteBatchRenderer.h"
#include "Renderer/SceneRenderData.h"
#include "Renderer/SceneView.h"
#include "Renderer/Types/RenderDebugColors.h"

namespace
{
    void SubmitQuadOutline(FD3D11LineBatchRenderer& InLineRenderer, const FVector& InBottomLeft,
                           const FVector& InRight, const FVector& InUp, const FColor& InColor)
    {
        const FVector P0 = InBottomLeft;
        const FVector P1 = InBottomLeft + InUp;
        const FVector P2 = InBottomLeft + InRight + InUp;
        const FVector P3 = InBottomLeft + InRight;

        InLineRenderer.AddLine(P0, P1, InColor);
        InLineRenderer.AddLine(P1, P2, InColor);
        InLineRenderer.AddLine(P2, P3, InColor);
        InLineRenderer.AddLine(P3, P0, InColor);
    }

    bool ShouldSubmitSprites(const FSceneRenderData& InSceneRenderData)
    {
        return InSceneRenderData.SceneView != nullptr &&
               IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_Sprites);
    }
}

void FSpriteSubmitter::Submit(FD3D11SpriteBatchRenderer& InSpriteRenderer,
                              const FSceneRenderData&    InSceneRenderData) const
{
    if (!ShouldSubmitSprites(InSceneRenderData))
    {
        return;
    }

    InSpriteRenderer.AddSprites(InSceneRenderData.Sprites);
}

void FSpriteSubmitter::Submit(FD3D11LineBatchRenderer& InLineRenderer,
                              const FSceneRenderData&  InSceneRenderData) const
{
    if (!ShouldSubmitSprites(InSceneRenderData))
    {
        return;
    }

    const FSceneView* SceneView = InSceneRenderData.SceneView;
    if (SceneView == nullptr)
    {
        return;
    }

    const FMatrix CameraWorld = SceneView->GetViewMatrix().GetInverse();

    for (const FSpriteRenderItem& Item : InSceneRenderData.Sprites)
    {
        if (!Item.State.IsVisible())
        {
            continue;
        }

        const FMatrix& PlacementWorld = Item.Placement.World;
        const FVector  SpriteOrigin = PlacementWorld.GetOrigin() + Item.Placement.WorldOffset;

        FVector RightAxis;
        FVector UpAxis;

        if (Item.Placement.IsBillboard())
        {
            RightAxis = CameraWorld.GetRightVector();
            UpAxis = CameraWorld.GetUpVector();

            const FVector WorldScale = PlacementWorld.GetScaleVector();
            RightAxis = RightAxis * WorldScale.X;
            UpAxis = UpAxis * WorldScale.Z;
        }
        else
        {
            RightAxis = PlacementWorld.GetForwardVector();
            UpAxis = PlacementWorld.GetUpVector();
        }

        const FVector BottomLeft = SpriteOrigin - RightAxis - UpAxis;
        const FColor  OutlineColor =
            (Item.TextureResource == nullptr) ? RenderDebugColors::MissingGlyph : Item.Color;

        SubmitQuadOutline(InLineRenderer, BottomLeft, RightAxis * 2.0f, UpAxis * 2.0f,
                          OutlineColor);
    }
}
