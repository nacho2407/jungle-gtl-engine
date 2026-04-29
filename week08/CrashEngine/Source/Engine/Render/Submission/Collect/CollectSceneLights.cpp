// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Submission/Collect/DrawCollector.h"

#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/LightComponent.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/Proxies/Light/DirectionalLightSceneProxy.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Scene/Proxies/Light/PointLightSceneProxy.h"
#include "Render/Scene/Proxies/Light/SpotLightSceneProxy.h"
#include "Render/Execute/Context/Scene/SceneView.h"

namespace
{
float GetLightDebugScale(const FShowFlags& ShowFlags, uint32 LightType)
{
    if (LightType == static_cast<uint32>(ELightType::Directional))
    {
        return ShowFlags.DirectionalLightDebugScale;
    }

    if (LightType == static_cast<uint32>(ELightType::Spot))
    {
        return ShowFlags.SpotLightDebugScale;
    }

    if (LightType == static_cast<uint32>(ELightType::Point))
    {
        return ShowFlags.PointLightDebugScale;
    }

    return 1.0f;
}
}

// ==================== Public API ====================

void FDrawCollector::CollectSceneLights(UWorld* World, FScene* Scene, const FSceneView* SceneView)
{
    ResetCollectedLights(CollectedSceneData.Lights);

    if (!Scene || !World)
    {
        return;
    }

    const bool bIsEditorWorld = (World->GetWorldType() == EWorldType::Editor);

    const TArray<FLightProxy*>& LightProxies = Scene->GetLightProxies();
    for (FLightProxy* Proxy : LightProxies)
    {
        if (!Proxy || !Proxy->bAffectsWorld || !Proxy->Owner)
        {
            continue;
        }

        AActor* OwnerActor = Proxy->Owner->GetOwner();
        if (!OwnerActor || !OwnerActor->IsVisible())
        {
            continue;
        }

        CollectedSceneData.Lights.VisibleLightProxies.push_back(Proxy);

        FLightProxyInfo& LC = Proxy->LightProxyInfo;
        if (LC.LightType == static_cast<uint32>(ELightType::Ambient))
        {
            CollectedSceneData.Lights.GlobalLights.Ambient.Color     = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
            CollectedSceneData.Lights.GlobalLights.Ambient.Intensity = LC.Intensity;
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Directional))
        {
            if (CollectedSceneData.Lights.GlobalLights.NumDirectionalLights < MAX_DIRECTIONAL_LIGHTS)
            {
                const FCascadeShadowMapData* CascadeShadowMapData = Proxy->GetCascadeShadowMapData();
                uint32 Index = CollectedSceneData.Lights.GlobalLights.NumDirectionalLights;
                CollectedSceneData.Lights.GlobalLights.Directional[Index].Color = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
                CollectedSceneData.Lights.GlobalLights.Directional[Index].Intensity = LC.Intensity;
                CollectedSceneData.Lights.GlobalLights.Directional[Index].Direction = LC.Direction;

                if (CascadeShadowMapData)
                {
                    CollectedSceneData.Lights.GlobalLights.Directional[Index].CascadeCount =
                        static_cast<int32>(CascadeShadowMapData->CascadeCount);
                    for (uint32 CascadeIndex = 0; CascadeIndex < MAX_DIRECTIONAL_SHADOW_CASCADES; ++CascadeIndex)
                    {
                        CollectedSceneData.Lights.GlobalLights.Directional[Index].ShadowViewProj[CascadeIndex] =
                            CascadeShadowMapData->CascadeViewProj[CascadeIndex];
                        CollectedSceneData.Lights.GlobalLights.Directional[Index].ShadowSamples[CascadeIndex] =
                            MakeSampleCBData(CascadeShadowMapData->Cascades[CascadeIndex]);
                    }
                }

                CollectedSceneData.Lights.GlobalLights.Directional[Index].ShadowBias = LC.ShadowBias;
                CollectedSceneData.Lights.GlobalLights.Directional[Index].ShadowSlopeBias = LC.ShadowSlopeBias;
                CollectedSceneData.Lights.GlobalLights.Directional[Index].ShadowNormalBias = LC.ShadowNormalBias;
                CollectedSceneData.Lights.GlobalLights.Directional[Index].ShadowSharpen = LC.ShadowSharpen;
                CollectedSceneData.Lights.GlobalLights.Directional[Index].ShadowESMExponent = LC.ShadowESMExponent;
                CollectedSceneData.Lights.GlobalLights.NumDirectionalLights++;
            }
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Point) || LC.LightType == static_cast<uint32>(ELightType::Spot))
        {
            FLocalLightCBData LocalLight = {};
            LocalLight.LightType         = LC.LightType;
            LocalLight.Color             = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
            LocalLight.Intensity         = LC.Intensity;
            LocalLight.Position          = LC.Position;
            LocalLight.AttenuationRadius = LC.AttenuationRadius;
            LocalLight.Direction         = LC.Direction;
            LocalLight.InnerConeAngle    = LC.InnerConeAngle;
            LocalLight.OuterConeAngle    = LC.OuterConeAngle;
            if (LC.LightType == static_cast<uint32>(ELightType::Spot))
            {
                const FShadowMapData* SpotShadowMapData = Proxy->GetSpotShadowMapData();
                if (SpotShadowMapData)
                {
                    LocalLight.ShadowSampleCount = 1;
                    LocalLight.ShadowViewProj[0] = Proxy->LightViewProj;
                    LocalLight.ShadowSamples[0] = MakeSampleCBData(*SpotShadowMapData);
                }
            }
            else
            {
                const FCubeShadowMapData* CubeShadowMapData = Proxy->GetCubeShadowMapData();
                const FMatrix* ShadowViewProjMatrices = Proxy->GetPointShadowViewProjMatrices();
                if (CubeShadowMapData && ShadowViewProjMatrices)
                {
                    LocalLight.ShadowSampleCount = MAX_POINT_SHADOW_FACES;
                    for (uint32 FaceIndex = 0; FaceIndex < MAX_POINT_SHADOW_FACES; ++FaceIndex)
                    {
                        LocalLight.ShadowViewProj[FaceIndex] = CubeShadowMapData->FaceViewProj[FaceIndex];
                        LocalLight.ShadowSamples[FaceIndex] = MakeSampleCBData(CubeShadowMapData->Faces[FaceIndex]);
                    }
                }
            }
            LocalLight.ShadowBias = LC.ShadowBias;
            LocalLight.ShadowSlopeBias = LC.ShadowSlopeBias;
            LocalLight.ShadowNormalBias = LC.ShadowNormalBias;
            LocalLight.ShadowSharpen = LC.ShadowSharpen;
            LocalLight.ShadowESMExponent = LC.ShadowESMExponent;
            CollectedSceneData.Lights.LocalLights.push_back(LocalLight);
        }

        if (bIsEditorWorld && SceneView && SceneView->ShowFlags.bLightDebugLines)
        {
            Proxy->VisualizeLightsInEditor(*Scene, GetLightDebugScale(SceneView->ShowFlags, LC.LightType));
        }
    }

    CollectedSceneData.Lights.GlobalLights.NumLocalLights = static_cast<int32>(CollectedSceneData.Lights.LocalLights.size());
}
