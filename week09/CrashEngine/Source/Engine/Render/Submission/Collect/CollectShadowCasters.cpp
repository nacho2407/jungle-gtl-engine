#include "Render/Submission/Collect/DrawCollector.h"
#include "GameFramework/World.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"
#include "Collision/SpatialPartition.h"
#include "Render/Execute/Context/Scene/SceneView.h"

#include <algorithm>
#include <cmath>

namespace
{
    void BuildPerspectiveFrustumCorners(const FSceneView* SceneView, float NearZ, float FarZ, FVector OutCorners[8])
    {
        const float Aspect = SceneView->ViewportWidth / std::max(1.0f, SceneView->ViewportHeight);
        const float TanHalfFov = std::tan(SceneView->FOV * 0.5f);
        const float NearHalfHeight = TanHalfFov * NearZ;
        const float NearHalfWidth = NearHalfHeight * Aspect;
        const float FarHalfHeight = TanHalfFov * FarZ;
        const float FarHalfWidth = FarHalfHeight * Aspect;

        const FVector NearCenter = SceneView->CameraPosition + SceneView->CameraForward * NearZ;
        const FVector FarCenter = SceneView->CameraPosition + SceneView->CameraForward * FarZ;
        const FVector NearRight = SceneView->CameraRight * NearHalfWidth;
        const FVector NearUp = SceneView->CameraUp * NearHalfHeight;
        const FVector FarRight = SceneView->CameraRight * FarHalfWidth;
        const FVector FarUp = SceneView->CameraUp * FarHalfHeight;

        OutCorners[0] = NearCenter - NearRight - NearUp;
        OutCorners[1] = NearCenter + NearRight - NearUp;
        OutCorners[2] = NearCenter + NearRight + NearUp;
        OutCorners[3] = NearCenter - NearRight + NearUp;
        OutCorners[4] = FarCenter - FarRight - FarUp;
        OutCorners[5] = FarCenter + FarRight - FarUp;
        OutCorners[6] = FarCenter + FarRight + FarUp;
        OutCorners[7] = FarCenter - FarRight + FarUp;
    }
}

void FDrawCollector::UpdateShadowViews(UWorld* World, const FSceneView* SceneView)
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

        Light->ShadowMapIndex = -1;

        if (!Light->bCastShadow)
        {
            continue;
        }

        FLightProxyInfo& LC = Light->LightProxyInfo;

        if (LC.LightType == static_cast<uint32>(ELightType::Point))
        {
            if (ShadowMapCubeCount >= 5) continue;
            Light->ShadowMapIndex = static_cast<int32>(ShadowMapCubeCount++);
            ComputePointShadowMatrices(Light);
        }
        else
        {
            if (ShadowMap2DCount >= 5) continue;
            Light->ShadowMapIndex = static_cast<int32>(ShadowMap2DCount++);

            if (LC.LightType == static_cast<uint32>(ELightType::Directional))
            {
                ComputeDirectionalShadowMatrices(Light, World, SceneView);
            }
            else
            {
                ComputeSpotShadowMatrices(Light);
            }
        }

        Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);
    }
}

void FDrawCollector::ComputeDirectionalShadowMatrices(FLightProxy* Light, UWorld* World, const FSceneView* SceneView)
{
    FLightProxyInfo& LC = Light->LightProxyInfo;
    FVector LightDir = LC.Direction.Normalized();

    bool bUsePSM = true;
    if (bUsePSM)
    {
        Light->LightViewProj = GetDirectionalPSMMatrix(World, LightDir, SceneView, Light->DynamicShadowDistance);
    }
    else
    {
        Light->LightViewProj = GetDirectionalSSMMatrix(World, LightDir);
    }
}

FMatrix FDrawCollector::GetDirectionalSSMMatrix(UWorld* World, FVector LightDir)
{
    FVector Up = (std::abs(LightDir.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
    FVector Right = LightDir.Cross(Up).Normalized();
    Up = Right.Cross(LightDir).Normalized();

    const FOctree* Octree = World->GetPartition().GetOctree();
    FBoundingBox SceneBounds = Octree ? Octree->GetCellBounds() : FBoundingBox(FVector(-500), FVector(500));
    FVector SceneCenter = SceneBounds.GetCenter();

    FMatrix LightView = FMatrix::Identity;
    LightView.M[0][0] = Right.X; LightView.M[0][1] = Up.X; LightView.M[0][2] = LightDir.X;
    LightView.M[1][0] = Right.Y; LightView.M[1][1] = Up.Y; LightView.M[1][2] = LightDir.Y;
    LightView.M[2][0] = Right.Z; LightView.M[2][1] = Up.Z; LightView.M[2][2] = LightDir.Z;
    LightView.M[3][0] = -SceneCenter.Dot(Right);
    LightView.M[3][1] = -SceneCenter.Dot(Up);
    LightView.M[3][2] = -SceneCenter.Dot(LightDir);

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

    float NearZ = MinLS.Z - 100.0f;
    float FarZ = MaxLS.Z + 100.0f;
    float Width = MaxLS.X - MinLS.X;
    float Height = MaxLS.Y - MinLS.Y;

    FVector OffsetLS(-(MinLS.X + MaxLS.X) * 0.5f, -(MinLS.Y + MaxLS.Y) * 0.5f, 0.0f);
    FMatrix LSAdjustment = FMatrix::MakeTranslationMatrix(OffsetLS);

    LightView = LightView * LSAdjustment;
    FMatrix LightProj = FMatrix::MakeOrthographic(Width, Height, NearZ, FarZ);
    return LightView * LightProj;
}

FMatrix FDrawCollector::GetDirectionalPSMMatrix(UWorld* World, FVector LightDir, const FSceneView* SceneView, float ShadowDistance)
{
    (void)World;

    FVector Corners[8];
    const float CameraNear = std::max(0.1f, SceneView->NearClip);
    const float ClampedShadowDistance = std::max(CameraNear + 100.0f, ShadowDistance);
    const float CameraFar = std::max(CameraNear + 100.0f, std::min(SceneView->FarClip, ClampedShadowDistance));
    BuildPerspectiveFrustumCorners(SceneView, CameraNear, CameraFar, Corners);

    float MaxBackDist = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        const float Dist = (Corners[i] - SceneView->CameraPosition).Dot(SceneView->CameraForward);
        if (Dist < 0.0f)
        {
            MaxBackDist = std::max(MaxBackDist, std::abs(Dist));
        }
    }
    const float VCSlideBack = MaxBackDist + CameraNear;

    const FVector VCPos = SceneView->CameraPosition - SceneView->CameraForward * VCSlideBack;
    const FVector VCTarget = SceneView->CameraPosition;

    const FMatrix VCView = FMatrix::MakeLookAt(VCPos, VCTarget, SceneView->CameraUp);
    const float Aspect = SceneView->ViewportWidth / std::max(1.0f, SceneView->ViewportHeight);
    const float FOV = SceneView->FOV;
    const float VCNear = std::max(0.1f, VCSlideBack - MaxBackDist);
    const float VCFar = VCSlideBack + CameraFar;
    const FMatrix VCProj = FMatrix::MakePerspective(FOV, Aspect, VCNear, VCFar);

    FMatrix ViewPP, ProjPP;
    {
        const FMatrix WorldToPPS = VCView * VCProj;
        FVector CornersPPS[8];
        FVector CenterPPS(0.0f);
        for (int i = 0; i < 8; ++i)
        {
            CornersPPS[i] = WorldToPPS.TransformPositionWithW(Corners[i]);
            CenterPPS += CornersPPS[i];
        }
        CenterPPS /= 8.0f;

        // Estimate a projection-aware light direction around the center of the camera frustum.
        const FVector FrustumCenterWorld =
            SceneView->CameraPosition + SceneView->CameraForward * (CameraNear + CameraFar) * 0.5f;
        const float DirectionProbeDistance = std::max(10.0f, CameraFar * 0.1f);
        const FVector ProbeCenterPPS = WorldToPPS.TransformPositionWithW(FrustumCenterWorld);
        const FVector ProbeOffsetPPS = WorldToPPS.TransformPositionWithW(FrustumCenterWorld + LightDir * DirectionProbeDistance);

        FVector LightDirPP = (ProbeOffsetPPS - ProbeCenterPPS).Normalized();
        if (LightDirPP.LengthSquared() < 1e-4f)
        {
            LightDirPP = VCView.TransformVector(LightDir).Normalized();
        }
        if (LightDirPP.LengthSquared() < 1e-4f)
        {
            LightDirPP = FVector(0.0f, 0.0f, 1.0f);
        }

        const FVector UpHint = (std::abs(LightDirPP.Z) < 0.99f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
        const FVector LightPosPP = CenterPPS - LightDirPP * 4.0f;
        ViewPP = FMatrix::MakeLookAt(LightPosPP, CenterPPS, UpHint);

        FVector MinLS(FLT_MAX), MaxLS(-FLT_MAX);
        for (int i = 0; i < 8; ++i)
        {
            const FVector CornerLS = ViewPP.TransformPositionWithW(CornersPPS[i]);
            MinLS.X = std::min(MinLS.X, CornerLS.X);
            MaxLS.X = std::max(MaxLS.X, CornerLS.X);
            MinLS.Y = std::min(MinLS.Y, CornerLS.Y);
            MaxLS.Y = std::max(MaxLS.Y, CornerLS.Y);
            MinLS.Z = std::min(MinLS.Z, CornerLS.Z);
            MaxLS.Z = std::max(MaxLS.Z, CornerLS.Z);
        }

        const float Width = std::max(0.01f, MaxLS.X - MinLS.X);
        const float Height = std::max(0.01f, MaxLS.Y - MinLS.Y);
        const float NearPP = MinLS.Z - 0.1f;
        const float FarPP = MaxLS.Z + 0.1f;

        const FVector OffsetLS(-(MinLS.X + MaxLS.X) * 0.5f, -(MinLS.Y + MaxLS.Y) * 0.5f, 0.0f);
        ViewPP = ViewPP * FMatrix::MakeTranslationMatrix(OffsetLS);
        ProjPP = FMatrix::MakeOrthographic(Width, Height, NearPP, FarPP);
    }

    return VCView * VCProj * ViewPP * ProjPP;
}

void FDrawCollector::ComputeSpotShadowMatrices(FLightProxy* Light)
{
    FLightProxyInfo& LC = Light->LightProxyInfo;
    FVector LightPos = LC.Position;
    FVector LightDir = LC.Direction.Normalized();

    FVector Up = (std::abs(LightDir.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
    FVector Right = LightDir.Cross(Up).Normalized();
    Up = Right.Cross(LightDir).Normalized();

    FMatrix LightView = FMatrix::Identity;
    LightView.M[0][0] = Right.X; LightView.M[0][1] = Up.X; LightView.M[0][2] = LightDir.X;
    LightView.M[1][0] = Right.Y; LightView.M[1][1] = Up.Y; LightView.M[1][2] = LightDir.Y;
    LightView.M[2][0] = Right.Z; LightView.M[2][1] = Up.Z; LightView.M[2][2] = LightDir.Z;
    LightView.M[3][0] = -LightPos.Dot(Right);
    LightView.M[3][1] = -LightPos.Dot(Up);
    LightView.M[3][2] = -LightPos.Dot(LightDir);

    float FOV = LC.OuterConeAngle * 2 * (3.141592f / 180.0f);
    FMatrix LightProj = FMatrix::MakePerspective(FOV, 1.0f, 1.0f, LC.AttenuationRadius);

    Light->LightViewProj = LightView * LightProj;
}

void FDrawCollector::ComputePointShadowMatrices(FLightProxy* Light)
{
    FLightProxyInfo& LC = Light->LightProxyInfo;
    FVector LightPos = LC.Position;
    FMatrix LightProjCube = FMatrix::MakePerspective(0.5f * 3.141592f, 1.0f, 1.0f, LC.AttenuationRadius);

    struct FFaceDir { FVector Forward; FVector Up; };
    FFaceDir Faces[6] = {
        { {  1,  0,  0 }, {  0,  1,  0 } },
        { { -1,  0,  0 }, {  0,  1,  0 } },
        { {  0,  1,  0 }, {  0,  0, -1 } },
        { {  0, -1,  0 }, {  0,  0,  1 } },
        { {  0,  0,  1 }, {  0,  1,  0 } },
        { {  0,  0, -1 }, {  0,  1,  0 } }
    };

    for (int i = 0; i < 6; ++i)
    {
        FVector Forward = Faces[i].Forward;
        FVector Up = Faces[i].Up;
        FVector Right = Up.Cross(Forward).Normalized();

        FMatrix LightViewCube = FMatrix::Identity;
        LightViewCube.M[0][0] = Right.X; LightViewCube.M[0][1] = Up.X; LightViewCube.M[0][2] = Forward.X;
        LightViewCube.M[1][0] = Right.Y; LightViewCube.M[1][1] = Up.Y; LightViewCube.M[1][2] = Forward.Y;
        LightViewCube.M[2][0] = Right.Z; LightViewCube.M[2][1] = Up.Z; LightViewCube.M[2][2] = Forward.Z;
        LightViewCube.M[3][0] = -LightPos.Dot(Right);
        LightViewCube.M[3][1] = -LightPos.Dot(Up);
        LightViewCube.M[3][2] = -LightPos.Dot(Forward);

        Light->ShadowViewProjMatrices[i] = LightViewCube * LightProjCube;
    }

    Light->LightViewProj = Light->ShadowViewProjMatrices[0];
}

void FDrawCollector::CollectShadowCasters(UWorld* World, const FSceneView* SceneView)
{
    if (!World || !SceneView)
    {
        return;
    }

    for (FLightProxy* Light : CollectedSceneData.Lights.VisibleLightProxies)
    {
        if (!Light || Light->ShadowMapIndex == -1)
        {
            continue;
        }

        Light->VisibleShadowCasters.clear();

        FLightProxyInfo& LC = Light->LightProxyInfo;

        if (LC.LightType == static_cast<uint32>(ELightType::Point))
        {
            // Point light uses sphere query
            World->GetPartition().QuerySphereAllProxies({LC.Position, LC.AttenuationRadius}, Light->VisibleShadowCasters);
        }
        else
        {
            // Directional or Spot uses frustum query
            World->GetPartition().QueryFrustumAllProxies(Light->ShadowViewFrustum, Light->VisibleShadowCasters);
        }
    }
}
