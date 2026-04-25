// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Submission/Collect/DrawCollector.h"

#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/LightComponent.h"
#include "Render/Scene/Scene.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Execute/Context/Scene/SceneView.h"

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
                uint32 Index                                                        = CollectedSceneData.Lights.GlobalLights.NumDirectionalLights;
                CollectedSceneData.Lights.GlobalLights.Directional[Index].Color     = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
                CollectedSceneData.Lights.GlobalLights.Directional[Index].Intensity = LC.Intensity;
                CollectedSceneData.Lights.GlobalLights.Directional[Index].Direction = LC.Direction;
                CollectedSceneData.Lights.GlobalLights.NumDirectionalLights++;
            }
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Point) || LC.LightType == static_cast<uint32>(ELightType::Spot))
        {
            FLocalLightCBData LocalLight = {};
            LocalLight.Color             = FVector(LC.LightColor.X, LC.LightColor.Y, LC.LightColor.Z);
            LocalLight.Intensity         = LC.Intensity;
            LocalLight.Position          = LC.Position;
            LocalLight.AttenuationRadius = LC.AttenuationRadius;
            LocalLight.Direction         = LC.Direction;
            LocalLight.InnerConeAngle    = LC.InnerConeAngle;
            LocalLight.OuterConeAngle    = LC.OuterConeAngle;
            CollectedSceneData.Lights.LocalLights.push_back(LocalLight);
        }

        if (bIsEditorWorld && SceneView && SceneView->ShowFlags.bLightDebugLines)
        {
            Proxy->VisualizeLightsInEditor(*Scene);
        }
    }

    CollectedSceneData.Lights.GlobalLights.NumLocalLights = static_cast<int32>(CollectedSceneData.Lights.LocalLights.size());
}

