#include "Render/Submission/Collect/DrawCollector.h"
#include "GameFramework/World.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Collision/SpatialPartition.h"
#include "Render/Execute/Context/Scene/SceneView.h"

void FDrawCollector::CollectShadowCasters(UWorld* World, const FSceneView* SceneView)
{
    if (!World || !SceneView)
    {
        return;
    }

    uint32 ShadowMapCount = 0;

    for (FLightProxy* Light : CollectedSceneData.Lights.VisibleLightProxies)
    {
        if (!Light) continue;

        // Reset
        Light->ShadowMapIndex = -1;

        if (!Light->bCastShadow || ShadowMapCount >= 5 /* FShadowMapPass::MAX_SHADOW_MAPS */)
        {
            continue;
        }

        Light->ShadowMapIndex = static_cast<int32>(ShadowMapCount++);

        // Clear previous frame results
        Light->VisibleShadowCasters.clear();

        // 1. Update ShadowViewFrustum and LightViewProj based on light type
        FLightProxyInfo& LC = Light->LightProxyInfo;
        
        FMatrix LightView = FMatrix::Identity;
        FMatrix LightProj = FMatrix::Identity;

        if (LC.LightType == static_cast<uint32>(ELightType::Directional))
        {
            // Simple orthographic for directional light
            // For now, assume a fixed large area. In real CSM, this depends on main camera.
            FVector LightDir = LC.Direction.Normalized();
            
            FVector Up = (std::abs(LightDir.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
            FVector Right = LightDir.Cross(Up).Normalized();
            Up = Right.Cross(LightDir).Normalized();

            // Place "camera" far back along direction
            FVector LightPos = LightDir * -500.0f; 

            LightView = FMatrix(
                Right.X, Up.X, LightDir.X, 0,
                Right.Y, Up.Y, LightDir.Y, 0,
                Right.Z, Up.Z, LightDir.Z, 0,
                -LightPos.Dot(Right), -LightPos.Dot(Up), -LightPos.Dot(LightDir), 1);

            // 임시로 고정 범위
            LightProj = FMatrix::MakeOrthographic(100.0f, 100.0f, 0.1f, 2000.0f);
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Spot))
        {
            FVector LightPos = LC.Position;
            FVector LightDir = LC.Direction.Normalized();
            
            FVector Up = (std::abs(LightDir.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
            FVector Right = LightDir.Cross(Up).Normalized();
            Up = Right.Cross(LightDir).Normalized();

            LightView = FMatrix(
                Right.X, Up.X, LightDir.X, 0,
                Right.Y, Up.Y, LightDir.Y, 0,
                Right.Z, Up.Z, LightDir.Z, 0,
                -LightPos.Dot(Right), -LightPos.Dot(Up), -LightPos.Dot(LightDir), 1);

            float FOV = LC.OuterConeAngle * 2 * (FMath::Pi / 180.0f);
            LightProj = FMatrix::MakePerspective(FOV, 1.0f, 1.0f, LC.AttenuationRadius);
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Point))
        {
            FMatrix LightProjCube = FMatrix::MakePerspective(0.5f * FMath::Pi, 1.0f, 1.0f, LC.AttenuationRadius);

            struct FFaceDir { FVector Forward; FVector Up; };
            FFaceDir Faces[6] = {
                { {  1,  0,  0 }, {  0,  1,  0 } }, // +X
                { { -1,  0,  0 }, {  0,  1,  0 } }, // -X
                { {  0,  1,  0 }, {  0,  0, -1 } }, // +Y
                { {  0, -1,  0 }, {  0,  0,  1 } }, // -Y
                { {  0,  0,  1 }, {  0,  1,  0 } }, // +Z
                { {  0,  0, -1 }, {  0,  1,  0 } }  // -Z
            };

            FVector LightPos = LC.Position;
            for (int i = 0; i < 6; ++i)
            {
                FVector Forward = Faces[i].Forward;
                FVector Up = Faces[i].Up;
                FVector Right = Up.Cross(Forward).Normalized();

                FMatrix LightViewCube = FMatrix(
                    Right.X, Up.X, Forward.X, 0,
                    Right.Y, Up.Y, Forward.Y, 0,
                    Right.Z, Up.Z, Forward.Z, 0,
                    -LightPos.Dot(Right), -LightPos.Dot(Up), -LightPos.Dot(Forward), 1);

                Light->ShadowViewProjMatrices[i] = LightViewCube * LightProjCube;
            }
            
            Light->LightViewProj = Light->ShadowViewProjMatrices[0]; 
            Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);

            World->GetPartition().QuerySphereAllProxies({LC.Position, LC.AttenuationRadius}, Light->VisibleShadowCasters);
            continue; 
        }

        Light->LightViewProj = LightView * LightProj;
        Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);

        World->GetPartition().QueryFrustumAllProxies(Light->ShadowViewFrustum, Light->VisibleShadowCasters);
    }
}
