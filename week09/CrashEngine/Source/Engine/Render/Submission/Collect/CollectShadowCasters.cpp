#include "Render/Submission/Collect/DrawCollector.h"
#include "GameFramework/World.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Collision/SpatialPartition.h"
#include "Render/Execute/Context/Scene/SceneView.h"

#include <algorithm>

void FDrawCollector::CollectShadowCasters(UWorld* World, const FSceneView* SceneView)
{
    if (!World || !SceneView)
    {
        return;
    }

    uint32 ShadowMap2DCount = 0;
    uint32 ShadowMapCubeCount = 0;

    for (FLightProxy* Light : CollectedSceneData.Lights.VisibleLightProxies)
    {
        if (!Light) continue;

        // Reset
        Light->ShadowMapIndex = -1;

        if (!Light->bCastShadow)
        {
            continue;
        }

        FLightProxyInfo& LC = Light->LightProxyInfo;

        // Assign indices based on type
        if (LC.LightType == static_cast<uint32>(ELightType::Point))
        {
            if (ShadowMapCubeCount >= 5) continue;
            Light->ShadowMapIndex = static_cast<int32>(ShadowMapCubeCount++);
        }
        else // Directional or Spot
        {
            if (ShadowMap2DCount >= 5) continue;
            Light->ShadowMapIndex = static_cast<int32>(ShadowMap2DCount++);
        }

        // Clear previous frame results
        Light->VisibleShadowCasters.clear();

        // Update ShadowViewFrustum and LightViewProj based on light type
        FMatrix LightView = FMatrix::Identity;
        FMatrix LightProj = FMatrix::Identity;

        if (LC.LightType == static_cast<uint32>(ELightType::Directional))
        {
            FVector LightDir = LC.Direction.Normalized();
            
            FVector Up = (std::abs(LightDir.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
            FVector Right = LightDir.Cross(Up).Normalized();
            Up = Right.Cross(LightDir).Normalized();

            // Get scene bounds from Octree 
            // Use default value if not exist
            const FOctree* Octree = World->GetPartition().GetOctree();
            FBoundingBox SceneBounds = Octree ? Octree->GetCellBounds() : FBoundingBox(FVector(-500), FVector(500));
            FVector SceneCenter = SceneBounds.GetCenter();

            // Calculate initial LightView centered on the scene
            // We'll adjust the Z-range later
            LightView = FMatrix(
                Right.X, Up.X, LightDir.X, 0,
                Right.Y, Up.Y, LightDir.Y, 0,
                Right.Z, Up.Z, LightDir.Z, 0,
                -SceneCenter.Dot(Right), -SceneCenter.Dot(Up), -SceneCenter.Dot(LightDir), 1);

            // Transform all 8 corners of the scene AABB to light view space
            FVector Corners[8];
            SceneBounds.GetCorners(Corners);

            FVector MinLS(FLT_MAX), MaxLS(-FLT_MAX);
            for (int i = 0; i < 8; ++i)
            {
                FVector CornerLS = LightView.TransformPositionWithW(Corners[i]);
                MinLS.X = std::min(MinLS.X, CornerLS.X);
                MaxLS.X = std::max(MaxLS.X, CornerLS.X);
                MinLS.Y = std::min(MinLS.Y, CornerLS.Y);
                MaxLS.Y = std::max(MaxLS.Y, CornerLS.Y);
                MinLS.Z = std::min(MinLS.Z, CornerLS.Z);
                MaxLS.Z = std::max(MaxLS.Z, CornerLS.Z);
            }

            // Adjust LightView translation so that Z starts from the nearest point of the scene
            // to maximize precision. Directional light "origin" is moved back.
            float NearZ = MinLS.Z - 100.0f; // Add small padding for casters outside bounds
            float FarZ = MaxLS.Z + 100.0f;
            
            float Width = MaxLS.X - MinLS.X;
            float Height = MaxLS.Y - MinLS.Y;

            // Update LightView to center the projection on the scene's XY extent in light space
            FVector OffsetLS(-(MinLS.X + MaxLS.X) * 0.5f, -(MinLS.Y + MaxLS.Y) * 0.5f, 0.0f);
            FMatrix LSAdjustment = FMatrix::MakeTranslationMatrix(OffsetLS);
            LightView = LightView * LSAdjustment;

            LightProj = FMatrix::MakeOrthographic(Width, Height, NearZ, FarZ);
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
