#pragma once

#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"

struct FFrameContext;
struct FViewportRenderTargets;
#include "Render/Scene/Core/Scene.h"
class FD3DDevice;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct FFrameSharedResources;
class FViewModePassRegistry;
class FViewModeSurfaceSet;
class FGPUOcclusionCulling;
class FRenderer;
struct FLODUpdateContext;
class FPrimitiveSceneProxy;
class FDecalSceneProxy;
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

    const TArray<FPrimitiveSceneProxy*>* VisibleProxies = nullptr;
    const TArray<FDecalSceneProxy*>* VisibleDecals = nullptr;
    const TArray<FScene::FDebugLine>* DebugLines = nullptr;
    const TArray<FScene::FOverlayText>* OverlayTexts = nullptr;

    FGPUOcclusionCulling* Occlusion = nullptr;
    const FLODUpdateContext* LODContext = nullptr;

    const FPassRenderState& GetPassState(ERenderPass Pass) const;
    ID3D11RenderTargetView* GetViewportRTV() const;
    ID3D11DepthStencilView* GetViewportDSV() const;
};
