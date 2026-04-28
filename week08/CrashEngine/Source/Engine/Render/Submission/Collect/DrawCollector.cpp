// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Submission/Collect/DrawCollector.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Renderer.h"

// ==================== Public API ====================

void FDrawCollector::Reset()
{
    ResetCollectedPrimitives(CollectedSceneData.Primitives, true);
    ResetCollectedLights(CollectedSceneData.Lights);
    CollectedOverlayData.ClearTransientData();
}

// ==================== Reset Helpers ====================
// 그림자를 드리우는 라이트가 어떤 atlas rect를 쓰는지 CB 데이터에 다시 반영합니다.
void FDrawCollector::UpdateShadowDataInCBs()
{
    uint32 DirIdx = 0;
    uint32 LocalIdx = 0;
    for (FLightProxy* Proxy : CollectedSceneData.Lights.VisibleLightProxies)
    {
        if (!Proxy) continue;

        FLightProxyInfo& LC = Proxy->LightProxyInfo;
        if (LC.LightType == static_cast<uint32>(ELightType::Directional))
        {
            if (DirIdx < MAX_DIRECTIONAL_LIGHTS)
            {
                const FCascadeShadowMapData* CascadeShadowMapData = Proxy->GetCascadeShadowMapData();
                if (!CascadeShadowMapData)
                {
                    continue;
                }

                CollectedSceneData.Lights.GlobalLights.Directional[DirIdx].CascadeCount =
                    static_cast<int32>(CascadeShadowMapData->CascadeCount);
                for (uint32 CascadeIndex = 0; CascadeIndex < MAX_DIRECTIONAL_SHADOW_CASCADES; ++CascadeIndex)
                {
                    CollectedSceneData.Lights.GlobalLights.Directional[DirIdx].ShadowViewProj[CascadeIndex] =
                        CascadeShadowMapData->CascadeViewProj[CascadeIndex];
                    CollectedSceneData.Lights.GlobalLights.Directional[DirIdx].ShadowSamples[CascadeIndex] =
                        MakeSampleCBData(CascadeShadowMapData->Cascades[CascadeIndex]);
                }
                CollectedSceneData.Lights.GlobalLights.Directional[DirIdx].ShadowBias = LC.ShadowBias;
                CollectedSceneData.Lights.GlobalLights.Directional[DirIdx].ShadowSlopeBias = LC.ShadowSlopeBias;
                CollectedSceneData.Lights.GlobalLights.Directional[DirIdx].ShadowNormalBias = LC.ShadowNormalBias;
                DirIdx++;
            }
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Point) || LC.LightType == static_cast<uint32>(ELightType::Spot))
        {
            if (LocalIdx < CollectedSceneData.Lights.LocalLights.size())
            {
                if (LC.LightType == static_cast<uint32>(ELightType::Spot))
                {
                    const FShadowMapData* SpotShadowMapData = Proxy->GetSpotShadowMapData();
                    if (!SpotShadowMapData)
                    {
                        continue;
                    }

                    CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowSampleCount = 1;
                    CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowViewProj[0] = Proxy->LightViewProj;
                    CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowSamples[0] = MakeSampleCBData(*SpotShadowMapData);
                }
                else
                {
                    const FCubeShadowMapData* CubeShadowMapData = Proxy->GetCubeShadowMapData();
                    if (!CubeShadowMapData)
                    {
                        continue;
                    }

                    CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowSampleCount = MAX_POINT_SHADOW_FACES;
                    for (uint32 FaceIndex = 0; FaceIndex < MAX_POINT_SHADOW_FACES; ++FaceIndex)
                    {
                        CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowViewProj[FaceIndex] =
                            CubeShadowMapData->FaceViewProj[FaceIndex];
                        CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowSamples[FaceIndex] =
                            MakeSampleCBData(CubeShadowMapData->Faces[FaceIndex]);
                    }
                }
                CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowBias = LC.ShadowBias;
                CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowSlopeBias = LC.ShadowSlopeBias;
                CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowNormalBias = LC.ShadowNormalBias;
                LocalIdx++;
            }
        }
    }
}

void FDrawCollector::ResetCollectedPrimitives(FCollectedPrimitives& OutPrimitives, bool bClearOverlayTexts)
{
    OutPrimitives.VisibleProxies.clear();
    OutPrimitives.OpaqueProxies.clear();
    OutPrimitives.TransparentProxies.clear();

    if (bClearOverlayTexts)
    {
        OutPrimitives.OverlayTexts.clear();
    }
}

void FDrawCollector::ResetCollectedLights(FCollectedLights& OutLights)
{
    OutLights.GlobalLights = {};
    OutLights.LocalLights.clear();
    OutLights.VisibleLightProxies.clear();
}
