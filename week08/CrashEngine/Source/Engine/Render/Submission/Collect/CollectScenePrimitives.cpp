// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Submission/Collect/DrawCollector.h"

#include "GameFramework/World.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "Profiling/Stats.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Scene/Scene.h"
#include "Render/Visibility/Occlusion/GPUOcclusionCulling.h"

#include <algorithm>

#define LOD_STATS_ENABLE 1
#if LOD_STATS_ENABLE
thread_local uint64 GLOD0 = 0;
thread_local uint64 GLOD1 = 0;
thread_local uint64 GLOD2 = 0;
#define DRAW_COLLECTOR_LOD_STATS_RESET() \
    do                                   \
    {                                    \
        GLOD0 = GLOD1 = GLOD2 = 0;       \
    } while (0)
#define DRAW_COLLECTOR_LOD_STATS_RECORD(L) \
    do                                     \
    {                                      \
        if ((L) == 0)                      \
            ++GLOD0;                       \
        else if ((L) == 1)                 \
            ++GLOD1;                       \
        else                               \
            ++GLOD2;                       \
    } while (0)
#else
#define DRAW_COLLECTOR_LOD_STATS_RESET() \
    do                                   \
    {                                    \
    } while (0)
#define DRAW_COLLECTOR_LOD_STATS_RECORD(L) \
    do                                     \
    {                                      \
    } while (0)
#endif

namespace
{
bool ContainsProxy(const TArray<FPrimitiveProxy*>& Proxies, const FPrimitiveProxy* Target)
{
    return std::find(Proxies.begin(), Proxies.end(), Target) != Proxies.end();
}

void AppendSceneRegistryFrustumProxies(FScene& Scene, const FConvexVolume& Frustum, TArray<FPrimitiveProxy*>& OutProxies)
{
    for (FPrimitiveProxy* Proxy : Scene.GetPrimitiveProxies())
    {
        if (!Proxy || Proxy->bNeverCull || ContainsProxy(OutProxies, Proxy))
        {
            continue;
        }

        if (Frustum.IntersectAABB(Proxy->CachedBounds))
        {
            OutProxies.push_back(Proxy);
        }
    }
}
} // namespace

// ==================== Scene Helpers ====================

uint32 FDrawCollector::SelectLOD(uint32 CurrentLOD, float DistSq)
{
    (void)CurrentLOD;

    constexpr float LOD1Distance = 500.0f;
    constexpr float LOD2Distance = 1500.0f;

    if (DistSq >= LOD2Distance * LOD2Distance)
        return 2;
    if (DistSq >= LOD1Distance * LOD1Distance)
        return 1;
    return 0;
}


// ==================== Public API ====================

void FDrawCollector::CollectWorld(UWorld* World, FRenderCollectContext& CollectContext)
{
    CollectScenePrimitives(World, CollectContext);
    CollectSceneLights(World, CollectContext.Scene, CollectContext.SceneView);
    CollectShadowCasters(World);

    if (World && CollectContext.SceneView)
    {
        CollectEditorHelpers(World, *CollectContext.SceneView, CollectedOverlayData);
    }
}

void FDrawCollector::CollectScenePrimitives(UWorld* World, FRenderCollectContext& CollectContext)
{
    ResetCollectedPrimitives(CollectedSceneData.Primitives, false);

    if (!World || !CollectContext.SceneView || !CollectContext.Scene)
    {
        if (CollectContext.CollectedPrimitives)
        {
            *CollectContext.CollectedPrimitives = CollectedSceneData.Primitives;
        }
        return;
    }

    const FSceneView& SceneView = *CollectContext.SceneView;
    FScene&           Scene     = *CollectContext.Scene;

    Scene.UpdateDirtyProxies();
    Scene.UpdateDirtyLightProxies();

    {
        SCOPE_STAT_CAT("FrustumCulling", "3_Collect");
        const uint32 ExpectedCount = Scene.GetPrimitiveProxyCount() + static_cast<uint32>(Scene.GetNeverCullProxies().size());
        if (CollectedSceneData.Primitives.VisibleProxies.capacity() < ExpectedCount)
        {
            CollectedSceneData.Primitives.VisibleProxies.reserve(ExpectedCount);
        }

        for (FPrimitiveProxy* Proxy : Scene.GetNeverCullProxies())
        {
            if (Proxy)
            {
                CollectedSceneData.Primitives.VisibleProxies.push_back(Proxy);
            }
        }

        World->GetPartition().QueryFrustumAllProxies(SceneView.FrustumVolume, CollectedSceneData.Primitives.VisibleProxies);
        AppendSceneRegistryFrustumProxies(Scene, SceneView.FrustumVolume, CollectedSceneData.Primitives.VisibleProxies);
    }

    const TArray<FPrimitiveProxy*> CandidateProxies = CollectedSceneData.Primitives.VisibleProxies;
    CollectedSceneData.Primitives.VisibleProxies.clear();
    CollectedSceneData.Primitives.OpaqueProxies.clear();
    CollectedSceneData.Primitives.TransparentProxies.clear();

    const FGPUOcclusionCulling* Occlusion    = SceneView.OcclusionCulling;
    FGPUOcclusionCulling*       OcclusionMut = SceneView.OcclusionCulling;
    const FLODUpdateContext&    LODCtx       = SceneView.LODContext;

    if (OcclusionMut && OcclusionMut->IsInitialized())
    {
        OcclusionMut->BeginGatherAABB(static_cast<uint32>(CandidateProxies.size()));
    }

    DRAW_COLLECTOR_LOD_STATS_RESET();

    for (FPrimitiveProxy* Proxy : CandidateProxies)
    {
        if (!Proxy)
        {
            continue;
        }

        if (World->GetWorldType() == EWorldType::Editor)
        {
            const UPrimitiveComponent* PrimitiveOwner = Proxy->Owner;
            if (PrimitiveOwner && PrimitiveOwner->IsEditorHelper())
            {
                continue;
            }
        }

        if (LODCtx.bValid && LODCtx.ShouldRefreshLOD(Proxy->ProxyId, Proxy->LastLODUpdateFrame))
        {
            const FVector& ProxyPos = Proxy->CachedWorldPos;
            const float    dx       = LODCtx.CameraPos.X - ProxyPos.X;
            const float    dy       = LODCtx.CameraPos.Y - ProxyPos.Y;
            const float    dz       = LODCtx.CameraPos.Z - ProxyPos.Z;
            const float    DistSq   = dx * dx + dy * dy + dz * dz;
            Proxy->UpdateLOD(SelectLOD(Proxy->CurrentLOD, DistSq));
            Proxy->LastLODUpdateFrame = LODCtx.LODUpdateFrame;
        }

        DRAW_COLLECTOR_LOD_STATS_RECORD(Proxy->CurrentLOD);

        if (Proxy->bPerViewportUpdate)
        {
            Proxy->UpdatePerViewport(SceneView);
        }

        if (!Proxy->bVisible)
        {
            continue;
        }

        if (OcclusionMut)
        {
            OcclusionMut->GatherAABB(Proxy);
        }

        // Opaque proxies must still reach DepthPre/Opaque command generation even
        // when the previous GPU occlusion readback is stale or overly aggressive.
        if (Occlusion && !Proxy->bNeverCull && Proxy->Pass != ERenderPass::Opaque && Occlusion->IsOccluded(Proxy))
        {
            continue;
        }

        CollectedSceneData.Primitives.VisibleProxies.push_back(Proxy);

        if (Proxy->Blend == EBlendState::AlphaBlend)
        {
            CollectedSceneData.Primitives.TransparentProxies.push_back(Proxy);
        }
        else
        {
            CollectedSceneData.Primitives.OpaqueProxies.push_back(Proxy);
        }
    }

    if (CollectContext.CollectedPrimitives)
    {
        *CollectContext.CollectedPrimitives = CollectedSceneData.Primitives;
    }
}

