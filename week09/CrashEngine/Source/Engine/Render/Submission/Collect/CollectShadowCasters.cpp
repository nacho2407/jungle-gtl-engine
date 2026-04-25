#include "Render/Submission/Collect/DrawCollector.h"
#include "GameFramework/World.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Scene/Scene.h"
#include "Collision/SpatialPartition.h"

void FDrawCollector::CollectShadowCasters(UWorld* World)
{
    if (!World)
    {
        return;
    }

    for (FLightProxy* Light : CollectedSceneData.Lights.VisibleLightProxies)
    {
        if (!Light || !Light->bCastShadow)
        {
            continue;
        }

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
            FVector Right = Up.Cross(LightDir).Normalized();
            Up = LightDir.Cross(Right).Normalized();

            // Place "camera" far back along direction
            FVector LightPos = LightDir * -500.0f; 

            LightView = FMatrix(
                Right.X, Up.X, LightDir.X, 0,
                Right.Y, Up.Y, LightDir.Y, 0,
                Right.Z, Up.Z, LightDir.Z, 0,
                -LightPos.Dot(Right), -LightPos.Dot(Up), -LightPos.Dot(LightDir), 1);

            float HalfW = 1000.0f; // Fixed coverage for now
            float HalfH = 1000.0f;
            float N = 0.1f;
            float F = 2000.0f;
            float Denom = N - F;

            // Reversed-Z orthographic
            LightProj = FMatrix(
                1.0f / HalfW, 0, 0, 0,
                0, 1.0f / HalfH, 0, 0,
                0, 0, 1.0f / Denom, 0,
                0, 0, -F / Denom, 1);
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Spot))
        {
            FVector LightPos = LC.Position;
            FVector LightDir = LC.Direction.Normalized();
            
            // Standard LookAt calculation
            FVector Up = (std::abs(LightDir.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
            FVector Right = LightDir.Cross(Up).Normalized();
            Up = Right.Cross(LightDir).Normalized();

            // Row-major View Matrix
            LightView = FMatrix(
                Right.X, Up.X, LightDir.X, 0,
                Right.Y, Up.Y, LightDir.Y, 0,
                Right.Z, Up.Z, LightDir.Z, 0,
                -LightPos.Dot(Right), -LightPos.Dot(Up), -LightPos.Dot(LightDir), 1);

            // OuterConeAngle is stored in degrees (UE-style), convert to radians for tanf.
            float HalfFOV = LC.OuterConeAngle * (FMath::Pi / 180.0f); 
            float Cot = 1.0f / tanf(HalfFOV);
            float Aspect = 1.0f;
            float N = 1.0f; // Increased near plane slightly for stability
            float F = LC.AttenuationRadius;
            if (F <= N) F = N + 100.0f;
            float Denom = N - F;

            // Reversed-Z perspective (N -> 1, F -> 0)
            LightProj = FMatrix(
                Cot / Aspect, 0, 0, 0,
                0, Cot, 0, 0,
                0, 0, N / Denom, 1,
                0, 0, -(F * N) / Denom, 0);
        }
        else if (LC.LightType == static_cast<uint32>(ELightType::Point))
        {
            FVector LightPos = LC.Position;
            float N = 1.0f;
            float F = LC.AttenuationRadius;
            if (F <= N) F = N + 100.0f;
            float Denom = N - F;

            // 90 deg FOV, aspect 1.0
            FMatrix LightProjCube = FMatrix(
                1.0f, 0, 0, 0,
                0, 1.0f, 0, 0,
                0, 0, N / Denom, 1,
                0, 0, -(F * N) / Denom, 0);

            // D3D Cube Map Directions
            struct FFaceDir { FVector Forward; FVector Up; };
            FFaceDir Faces[6] = {
                { {  1,  0,  0 }, {  0,  1,  0 } }, // +X
                { { -1,  0,  0 }, {  0,  1,  0 } }, // -X
                { {  0,  1,  0 }, {  0,  0, -1 } }, // +Y
                { {  0, -1,  0 }, {  0,  0,  1 } }, // -Y
                { {  0,  0,  1 }, {  0,  1,  0 } }, // +Z
                { {  0,  0, -1 }, {  0,  1,  0 } }  // -Z
            };

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

            // Use +Z as the primary for frustum update (though we use sphere for culling)
            Light->LightViewProj = Light->ShadowViewProjMatrices[4]; 
            Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);

            // 2. Perform Culling using Sphere Query
            World->GetPartition().QuerySphereAllProxies({ LC.Position, LC.AttenuationRadius }, Light->VisibleShadowCasters);
            continue; 
        }

        Light->LightViewProj = LightView * LightProj;
        Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);

        // 2. Perform Culling using BVH (Frustum Query for Directional/Spot)
        World->GetPartition().QueryFrustumAllProxies(Light->ShadowViewFrustum, Light->VisibleShadowCasters);

        // 3. Filter for shadow casters (if needed, e.g. bCastShadow on primitive)
        // For now, assume all visible proxies cast shadows.
    }
}
