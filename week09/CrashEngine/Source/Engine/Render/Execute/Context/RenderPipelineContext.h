// Defines context structs passed across render pipeline execution.
#pragma once

#include "Core/CoreTypes.h"

#include "Render/Execute/Registry/RenderPassPresets.h"
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Submission/Collect/CollectedOverlayData.h"
#include "Render/Submission/Collect/CollectedSceneData.h"

struct FSceneView;
struct FViewportRenderTargets;
class FScene;
class FD3DDevice;
struct FFrameResources;
class FViewModePassRegistry;
class FViewModeSurfaces;
class FGPUOcclusionCulling;
class FRenderer;
struct FLODUpdateContext;
class FPrimitiveProxy;
class FDecalSceneProxy;
class FDrawCommandList;
class FTileBasedLightCulling;
struct FDrawBindStateCache;

// FViewModeExecutionContext groups view-mode specific execution references.
struct FViewModeExecutionContext
{
    const FViewModePassRegistry* Registry       = nullptr;
    FViewModeSurfaces*           Surfaces       = nullptr;
    EViewMode                    ActiveViewMode = {};
};

// FRenderSubmissionContext groups collected submission data for pass execution.
struct FRenderSubmissionContext
{
    const FCollectedSceneData*   SceneData   = nullptr;
    const FCollectedOverlayData* OverlayData = nullptr;
};

// FRenderPipelineContext groups renderer-owned state and execution references for a pipeline run.
struct FRenderPipelineContext
{
    const FSceneView*             SceneView = nullptr;
    const FViewportRenderTargets* Targets   = nullptr;
    FScene*                       Scene     = nullptr;

    FRenderer*           Renderer = nullptr;
    FD3DDevice*          Device   = nullptr;
    ID3D11DeviceContext* Context  = nullptr;

    FFrameResources*         Resources         = nullptr;
    FDrawBindStateCache*     StateCache        = nullptr;
    FDrawCommandList*        DrawCommandList   = nullptr;
    const FRenderPassPreset* RenderPassPresets = nullptr;

    FGPUOcclusionCulling*    Occlusion    = nullptr;
    FTileBasedLightCulling*  LightCulling = nullptr;
    const FLODUpdateContext* LODContext   = nullptr;

    FViewModeExecutionContext ViewMode;
    FRenderSubmissionContext  Submission;

    const FRenderPassPreset&     GetRenderPassPreset(ERenderPass Pass) const;
    const FRenderPassDrawPreset& GetRenderPassDrawPreset(ERenderPass Pass) const;
    ID3D11RenderTargetView*      GetViewportRTV() const;
    ID3D11DepthStencilView*      GetViewportDSV() const;
};

