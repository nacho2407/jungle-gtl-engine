#pragma once

#include "Core/CoreTypes.h"
#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Passes/Base/RenderPass.h"

class FDepthPrePass;
class FBaseDrawPass;
class FDecalPass;
class FLightingPass;
class FAdditiveDecalPass;
class FAlphaBlendPass;
class FHeightFogPass;
class FViewModeResolvePass;
class FFXAAPass;
class FSelectionMaskPass;
class FOutlinePass;
class FDebugLinePass;
class FGizmoPass;
class FOverlayTextPass;

/*
    레지스트리에서 구분하는 패스 노드 종류입니다.
    실제 드로우 커맨드의 ERenderPass와는 별개로, 파이프라인 그래프 노드를 식별할 때 사용합니다.
*/
enum class ERenderPassNodeType
{
    GridPass,
    DepthPrePass,
    LightCullingPass,
    BaseDrawPass,
    DecalPass,
    LightingPass,
    AdditiveDecalPass,
    AlphaBlendPass,
    ViewModeResolvePass,
    HeightFogPass,
    FXAAPass,
    SelectionMaskPass,
    OutlinePass,
    DebugLinePass,
    GizmoPass,
    OverlayTextPass,
};

/*
    패스 노드 타입과 실제 패스 객체를 연결해 주는 레지스트리입니다.
    파이프라인 실행기는 이 레지스트리에서 패스를 찾아 순서대로 실행합니다.
*/
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
