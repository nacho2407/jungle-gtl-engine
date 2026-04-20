#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Types/ViewTypes.h"
#include "Render/Scene/DebugDraw/SceneDebugData.h"

struct FSceneView;
struct FViewportRenderTargets;
class FScene;
class FD3DDevice;
struct FFrameSharedResources;
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

/*
    렌더 파이프라인 실행에 필요한 문맥을 한 곳에 모아 둔 구조체입니다.
    현재 뷰, 타깃, 프레임 공용 자원, 수집된 프록시, 패스 상태를 함께 전달합니다.
*/
struct FRenderPipelineContext
{
    const FSceneView* SceneView = nullptr;
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
