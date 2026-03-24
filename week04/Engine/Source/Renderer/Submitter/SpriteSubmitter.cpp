#include "Renderer/Submitter/SpriteSubmitter.h"

#include "Renderer/D3D11/D3D11SpriteBatchRenderer.h"
#include "Renderer/SceneRenderData.h"

void FSpriteSubmitter::Submit(FD3D11SpriteBatchRenderer& InSpriteRenderer,
                              const FSceneRenderData&    InSceneRenderData) const
{
    if (InSceneRenderData.SceneView == nullptr ||
        !IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_Sprites))
    {
        return;
    }

    InSpriteRenderer.AddSprites(InSceneRenderData.Sprites);
}
