#include "Render/Submission/Collect/DrawCollector.h"

#include "Render/Scene/Proxies/Light/LightSceneProxy.h"
#include "Render/Scene/Scene.h"

// ==================== Public API ====================

void FDrawCollector::CollectSceneLights(FScene* Scene)
{
    ResetCollectedLights(CollectedSceneData.Lights);

    if (!Scene)
    {
        return;
    }

    const TArray<FLightSceneProxy*>& LightProxies = Scene->GetLightProxies();
    for (FLightSceneProxy* Proxy : LightProxies)
    {
        if (!Proxy)
        {
            continue;
        }

        FLightConstants& LC = Proxy->LightConstants;
        if (LC.LightType == static_cast<uint32>(ELightType::Ambient))
        {
            CollectedSceneData.Lights.GlobalLights.Ambient.Color     = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
            CollectedSceneData.Lights.GlobalLights.Ambient.Intensity = LC.Intensity;
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Directional))
        {
            if (CollectedSceneData.Lights.GlobalLights.NumDirectionalLights < MAX_DIRECTIONAL_LIGHTS)
            {
                uint32 Index                                                        = CollectedSceneData.Lights.GlobalLights.NumDirectionalLights;
                CollectedSceneData.Lights.GlobalLights.Directional[Index].Color     = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
                CollectedSceneData.Lights.GlobalLights.Directional[Index].Intensity = LC.Intensity;
                CollectedSceneData.Lights.GlobalLights.Directional[Index].Direction = LC.Direction;
                CollectedSceneData.Lights.GlobalLights.NumDirectionalLights++;
            }
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Point) || LC.LightType == static_cast<uint32>(ELightType::Spot))
        {
            FLocalLightInfo LocalLight   = {};
            LocalLight.Color             = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
            LocalLight.Intensity         = LC.Intensity;
            LocalLight.Position          = LC.Position;
            LocalLight.AttenuationRadius = LC.AttenuationRadius;
            LocalLight.Direction         = LC.Direction;
            LocalLight.InnerConeAngle    = LC.InnerConeAngle;
            LocalLight.OuterConeAngle    = LC.OuterConeAngle;
            CollectedSceneData.Lights.LocalLights.push_back(LocalLight);
        }

        Proxy->VisualizeLightsInEditor(*Scene);
    }

    CollectedSceneData.Lights.GlobalLights.NumLocalLights = static_cast<int32>(CollectedSceneData.Lights.LocalLights.size());
}
