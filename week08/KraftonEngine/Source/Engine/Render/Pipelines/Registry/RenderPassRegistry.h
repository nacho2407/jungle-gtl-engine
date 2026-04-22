#pragma once

#include "Core/CoreTypes.h"

#include "Render/Passes/Base/PassRenderState.h"
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
class FPresentPass;
class FSelectionMaskPass;
class FOutlinePass;
class FDebugLinePass;
class FGizmoPass;
class FOverlayBillboardPass;
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
    PresentPass,
    SelectionMaskPass,
    OutlinePass,
    DebugLinePass,
    OverlayBillboardPass,
    GizmoPass,
    OverlayTextPass,
	LightHitMapPass,
};

/*
    패스 노드 타입과 실제 패스 객체, 패스별 기본 상태 표를 함께 관리하는 레지스트리입니다.
*/
class FRenderPassRegistry
{
public:
    FRenderPassRegistry() = default;
    ~FRenderPassRegistry();

    void Initialize();
    void Release();

    FRenderPass* FindPass(ERenderPassNodeType Type) const;
    const FPassRenderStateDesc& GetPassStateDesc(ERenderPass Pass) const;
    const FPassRenderStateDesc* GetPassStateDescs() const;

private:
    TMap<int32, FRenderPass*> Passes;
    FPassRenderStateDesc PassStateDescs[(uint32)ERenderPass::MAX] = {};
};
