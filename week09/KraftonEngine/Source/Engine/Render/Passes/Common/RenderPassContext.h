#pragma once

#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"
#include "Render/Scene/DebugDraw/SceneDebugData.h"

struct FSceneView;
using FFrameContext = FSceneView;
struct FViewportRenderTargets;
class FScene;
class FD3DDevice;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct FFrameSharedResources;
using FFrameSharedResources = FFrameSharedResources;
class FViewModePassRegistry;
class FViewModeSurfaceSet;
class FGPUOcclusionCulling;
class FRenderer;
struct FLODUpdateContext;
class FPrimitiveSceneProxy;
class FDecalSceneProxy;
struct FCollectedPrimitives;
class FDrawCommandList;
struct FPassRenderState;
struct FStateCache;

struct FRenderPassContext
{
    const FFrameContext* Frame = nullptr;
    const FViewportRenderTargets* Targets = nullptr;
    FScene* Scene = nullptr;

    FRenderer* Renderer = nullptr;
    FD3DDevice* Device = nullptr;
    ID3D11DeviceContext* Context = nullptr;

    FFrameSharedResources* Resources = nullptr;
    FStateCache* StateCache = nullptr;
    FDrawCommandList* DrawCommandList = nullptr;
    const FPassRenderState* PassRenderStates = nullptr;

    const FViewModePassRegistry* ViewModePassRegistry = nullptr;
    FViewModeSurfaceSet* ActiveViewSurfaceSet = nullptr;
    EViewMode ActiveViewMode = EViewMode::Lit_Phong;

    const FCollectedPrimitives* CollectedPrimitives = nullptr;
    const TArray<FPrimitiveSceneProxy*>* VisibleProxies = nullptr;
    const TArray<FDecalSceneProxy*>* VisibleDecals = nullptr;
    const TArray<FSceneDebugLine>* DebugLines = nullptr;
    const TArray<FSceneOverlayText>* OverlayTexts = nullptr;

    FGPUOcclusionCulling* Occlusion = nullptr;
    const FLODUpdateContext* LODContext = nullptr;

    const FPassRenderState& GetPassState(ERenderPass Pass) const;
    ID3D11RenderTargetView* GetViewportRTV() const;
    ID3D11DepthStencilView* GetViewportDSV() const;
};
