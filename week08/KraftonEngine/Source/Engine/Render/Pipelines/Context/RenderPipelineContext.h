#pragma once

#include "Core/CoreTypes.h"

#include "Render/Passes/Base/PassRenderState.h"
#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/Pipelines/Context/Scene/ViewTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Submission/Collect/CollectedOverlayData.h"

struct FSceneView;
struct FViewportRenderTargets;
class FScene;
class FD3DDevice;
struct FFrameRenderResources;
class FViewModePassRegistry;
class FSceneViewModeSurfaces;
class FGPUOcclusionCulling;
class FRenderer;
struct FLODUpdateContext;
class FPrimitiveSceneProxy;
class FDecalSceneProxy;
struct FCollectedPrimitives;
class FDrawCommandList;
class FTileBasedLightCulling;
struct FDrawSubmitStateCache;

/*
    렌더 파이프라인 실행에 필요한 문맥을 한 곳에 모아 둔 구조체입니다.
    현재 뷰, 타깃, 프레임 공용 자원, 수집된 프록시, 패스 상태를 함께 전달합니다.
*/
struct FRenderPipelineContext
{
    const FSceneView*             SceneView = nullptr;
    const FViewportRenderTargets* Targets   = nullptr;
    FScene*                       Scene     = nullptr;

    FRenderer*           Renderer = nullptr;
    FD3DDevice*          Device   = nullptr;
    ID3D11DeviceContext* Context  = nullptr;

    FFrameRenderResources*      Resources       = nullptr;
    FDrawSubmitStateCache*      StateCache      = nullptr;
    FDrawCommandList*           DrawCommandList = nullptr;
    const FPassRenderStateDesc* PassStateDescs  = nullptr;

    const FViewModePassRegistry* ViewModePassRegistry = nullptr;
    FSceneViewModeSurfaces*      ActiveViewSurfaces   = nullptr;
    EViewMode                    ActiveViewMode       = {};

    const FCollectedPrimitives*          CollectedPrimitives = nullptr;
    const TArray<FPrimitiveSceneProxy*>* VisibleProxies      = nullptr;
    const TArray<FDecalSceneProxy*>*     VisibleDecals       = nullptr;
    const FCollectedOverlayData*         OverlayData            = nullptr;
    const TArray<FSceneDebugLine>*       DebugLines             = nullptr;
    const TArray<FSceneOverlayText>*     OverlayTexts           = nullptr;
    const TArray<FPrimitiveSceneProxy*>* OverlayBillboardProxies = nullptr;
    const TArray<FPrimitiveSceneProxy*>* OverlayTextProxies      = nullptr;

    FGPUOcclusionCulling*    Occlusion  = nullptr;
    FTileBasedLightCulling*  LightCulling = nullptr;
    const FLODUpdateContext* LODContext = nullptr;

    const FPassRenderStateDesc& GetPassState(ERenderPass Pass) const;
    ID3D11RenderTargetView*     GetViewportRTV() const;
    ID3D11DepthStencilView*     GetViewportDSV() const;
};
