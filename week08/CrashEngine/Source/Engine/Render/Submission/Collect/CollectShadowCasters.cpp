#include "Render/Submission/Collect/DrawCollector.h"

#include "Collision/SpatialPartition.h"
#include "GameFramework/World.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Renderer.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"

#include <algorithm>

namespace
{
FMatrix BuildDirectionalLightMatrix(const FBoundingBox& Bounds, const FVector& LightDir)
{
    FVector Direction = LightDir.Normalized();
    FVector Up = (std::abs(Direction.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
    FVector Right = Direction.Cross(Up).Normalized();
    Up = Right.Cross(Direction).Normalized();

    const FVector SceneCenter = Bounds.GetCenter();
    FMatrix LightView = FMatrix(
        Right.X, Up.X, Direction.X, 0,
        Right.Y, Up.Y, Direction.Y, 0,
        Right.Z, Up.Z, Direction.Z, 0,
        -SceneCenter.Dot(Right), -SceneCenter.Dot(Up), -SceneCenter.Dot(Direction), 1);

    FVector Corners[8];
    Bounds.GetCorners(Corners);

    FVector MinLS(FLT_MAX), MaxLS(-FLT_MAX);
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        const FVector CornerLS = LightView.TransformPositionWithW(Corners[CornerIndex]);
        MinLS.X = std::min(MinLS.X, CornerLS.X);
        MinLS.Y = std::min(MinLS.Y, CornerLS.Y);
        MinLS.Z = std::min(MinLS.Z, CornerLS.Z);
        MaxLS.X = std::max(MaxLS.X, CornerLS.X);
        MaxLS.Y = std::max(MaxLS.Y, CornerLS.Y);
        MaxLS.Z = std::max(MaxLS.Z, CornerLS.Z);
    }

    const float NearZ = MinLS.Z - 100.0f;
    const float FarZ = MaxLS.Z + 100.0f;
    const float Width = std::max(MaxLS.X - MinLS.X, 1.0f);
    const float Height = std::max(MaxLS.Y - MinLS.Y, 1.0f);

    const FVector OffsetLS(-(MinLS.X + MaxLS.X) * 0.5f, -(MinLS.Y + MaxLS.Y) * 0.5f, 0.0f);
    LightView = LightView * FMatrix::MakeTranslationMatrix(OffsetLS);
    return LightView * FMatrix::MakeOrthographic(Width, Height, NearZ, FarZ);
}
} // namespace

void FDrawCollector::CollectShadowCasters(UWorld* World, const FSceneView* SceneView)
{
    if (!World || !SceneView)
    {
        return;
    }

    for (FLightProxy* Light : CollectedSceneData.Lights.VisibleLightProxies)
    {
        if (!Light)
        {
            continue;
        }

        Light->ClearShadowData();
        Light->VisibleShadowCasters.clear();

        if (!Light->bCastShadow)
        {
            continue;
        }

        FLightProxyInfo& LC = Light->LightProxyInfo;
        if (LC.LightType == static_cast<uint32>(ELightType::Directional))
        {
            const FOctree* Octree = World->GetPartition().GetOctree();
            const FBoundingBox SceneBounds = Octree ? Octree->GetCellBounds() : FBoundingBox(FVector(-500), FVector(500));
            Light->LightViewProj = BuildDirectionalLightMatrix(SceneBounds, LC.Direction);
            Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);
            World->GetPartition().QueryFrustumAllProxies(Light->ShadowViewFrustum, Light->VisibleShadowCasters);
            continue;
        }

        if (LC.LightType == static_cast<uint32>(ELightType::Spot))
        {
            const FVector LightPos = LC.Position;
            const FVector LightDir = LC.Direction.Normalized();
            FVector Up = (std::abs(LightDir.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
            FVector Right = LightDir.Cross(Up).Normalized();
            Up = Right.Cross(LightDir).Normalized();

            const FMatrix LightView = FMatrix(
                Right.X, Up.X, LightDir.X, 0,
                Right.Y, Up.Y, LightDir.Y, 0,
                Right.Z, Up.Z, LightDir.Z, 0,
                -LightPos.Dot(Right), -LightPos.Dot(Up), -LightPos.Dot(LightDir), 1);

            const float FOV = LC.OuterConeAngle * 2.0f * (FMath::Pi / 180.0f);
            const FMatrix LightProj = FMatrix::MakePerspective(FOV, 1.0f, 1.0f, LC.AttenuationRadius);

            Light->LightViewProj = LightView * LightProj;
            Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);
            World->GetPartition().QueryFrustumAllProxies(Light->ShadowViewFrustum, Light->VisibleShadowCasters);
            continue;
        }

        if (LC.LightType == static_cast<uint32>(ELightType::Point))
        {
            const FMatrix LightProjCube = FMatrix::MakePerspective(0.5f * FMath::Pi, 1.0f, 1.0f, LC.AttenuationRadius);
            struct FFaceDir { FVector Forward; FVector Up; };
            const FFaceDir Faces[6] = {
                { { 1, 0, 0 }, { 0, 1, 0 } },
                { { -1, 0, 0 }, { 0, 1, 0 } },
                { { 0, 1, 0 }, { 0, 0, -1 } },
                { { 0, -1, 0 }, { 0, 0, 1 } },
                { { 0, 0, 1 }, { 0, 1, 0 } },
                { { 0, 0, -1 }, { 0, 1, 0 } }
            };

            const FVector LightPos = LC.Position;
            for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
            {
                const FVector Forward = Faces[FaceIndex].Forward;
                const FVector Up = Faces[FaceIndex].Up;
                const FVector Right = Up.Cross(Forward).Normalized();

                const FMatrix LightView = FMatrix(
                    Right.X, Up.X, Forward.X, 0,
                    Right.Y, Up.Y, Forward.Y, 0,
                    Right.Z, Up.Z, Forward.Z, 0,
                    -LightPos.Dot(Right), -LightPos.Dot(Up), -LightPos.Dot(Forward), 1);

                Light->ShadowViewProjMatrices[FaceIndex] = LightView * LightProjCube;
            }

            Light->LightViewProj = Light->ShadowViewProjMatrices[0];
            Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);
            World->GetPartition().QuerySphereAllProxies({ LC.Position, LC.AttenuationRadius }, Light->VisibleShadowCasters);
        }
    }
}
