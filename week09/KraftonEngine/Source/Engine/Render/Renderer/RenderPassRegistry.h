#pragma once

#include "Core/CoreTypes.h"
#include "Render/Core/RenderPass.h"

class FDepthPrePass;
class FBaseDrawPass;
class FDecalPass;
class FLightingPass;
class FAdditiveDecalPass;
class FAlphaBlendPass;
class FHeightFogPass;
class FFXAAPass;
class FSelectionMaskPass;
class FOutlinePass;
class FDebugLinesPass;
class FGizmoRenderPass;
class FOverlayFontRenderPass;

enum class ERenderPassNodeType
{
    DepthPrePass,
    BaseDrawPass,
    DecalPass,
    LightingPass,
    AdditiveDecalPass,
    AlphaBlendPass,
    HeightFogPass,
    FXAAPass,
    SelectionMaskPass,
    OutlinePass,
    DebugLinesPass,
    GizmoRenderPass,
    OverlayFontRenderPass,
};

class FRenderPassRegistry
{
public:
    FRenderPassRegistry() = default;
    ~FRenderPassRegistry();

    void Initialize();
    void Release();

    FRenderPass* FindPass(ERenderPassNodeType Type) const;

private:
    TMap<int32, FRenderPass*> Passes;
};
