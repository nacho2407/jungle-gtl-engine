// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Submission/Collect/DrawCollector.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Scene/Proxies/Light/LightProxyInfo.h"

// ==================== Public API ====================


void FDrawCollector::Reset()
{
    ResetCollectedPrimitives(CollectedSceneData.Primitives, true);
    ResetCollectedLights(CollectedSceneData.Lights);
    CollectedOverlayData.ClearTransientData();
}


// ==================== Reset Helpers ====================
// 그림자를 드리우는 라이트가 몇번 t슬롯을 사용하고 있는지에 대한 정보를 업데이트
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
                CollectedSceneData.Lights.GlobalLights.Directional[DirIdx].ShadowMapIndex = Proxy->ShadowMapIndex;
                CollectedSceneData.Lights.GlobalLights.Directional[DirIdx].ShadowViewProj = Proxy->LightViewProj;
                DirIdx++;
            }
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Point) || LC.LightType == static_cast<uint32>(ELightType::Spot))
        {
            if (LocalIdx < CollectedSceneData.Lights.LocalLights.size())
            {
                CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowMapIndex = Proxy->ShadowMapIndex;
                CollectedSceneData.Lights.LocalLights[LocalIdx].ShadowViewProj = Proxy->LightViewProj;
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
