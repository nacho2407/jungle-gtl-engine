#pragma once

#include "Core/CoreTypes.h"
#include "Render/Passes/RenderPass.h"

class FDepthPrePass;
class FBaseDrawPass;
class FDecalPass;
class FLightingPass;
class FAdditiveDecalPass;
class FAlphaBlendPass;
class FHeightFogPass;
class FViewModePostProcessPass;
class FFXAAPass;
class FSelectionMaskPass;
class FOutlinePass;
class FDebugLinePass;
class FGizmoPass;
class FOverlayTextPass;

enum class ERenderPassNodeType
{
    DepthPrePass,
    BaseDrawPass,
    DecalPass,
    LightingPass,
    AdditiveDecalPass,
    AlphaBlendPass,
    ViewModePostProcessPass,
    HeightFogPass,
    FXAAPass,
    SelectionMaskPass,
    OutlinePass,
    DebugLinePass,
    GizmoPass,
    OverlayTextPass,
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
