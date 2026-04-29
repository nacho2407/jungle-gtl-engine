#include "Render/Submission/Collect/DrawCollector.h"

#include "Collision/SpatialPartition.h"
#include "GameFramework/World.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Shadows/ShadowMapSettings.h"
#include "Render/Submission/Atlas/ShadowAtlasTypes.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"

#include <algorithm>
#include <cfloat>
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

void BuildCascadeSplitDistances(
    const FSceneView* SceneView,
    float ShadowDistance,
    float CascadeDistribution,
    int SliceCount,
    float OutSplitDistances[ShadowAtlas::MaxCascades + 1])
{
    const float NearClip = SceneView ? std::max(0.1f, SceneView->NearClip) : 0.1f;
    float MaxDistance = ShadowDistance;
    if (SceneView)
    {
        MaxDistance = std::min(MaxDistance, SceneView->FarClip);
    }
    MaxDistance = std::max(NearClip + 1.0f, MaxDistance);

    // Lambda (CascadeDistribution) determines the blend between logarithmic and linear splits.
    // 1.0 = fully logarithmic (optimal for perspective), 0.0 = fully linear.
    const float Lambda = std::clamp(CascadeDistribution, 0.0f, 1.0f);

    OutSplitDistances[0] = NearClip;
    for (int SliceIndex = 1; SliceIndex <= SliceCount; ++SliceIndex)
    {
        const float f = static_cast<float>(SliceIndex) / static_cast<float>(SliceCount);
        const float LogSplit = NearClip * std::pow(MaxDistance / NearClip, f);
        const float LinearSplit = NearClip + (MaxDistance - NearClip) * f;
        
        OutSplitDistances[SliceIndex] = Lambda * LogSplit + (1.0f - Lambda) * LinearSplit;
    }
}

float SnapToTexel(float Value, float WorldUnitsPerTexel)
{
    if (WorldUnitsPerTexel <= 0.0f)
    {
        return Value;
    }

    return std::floor(Value / WorldUnitsPerTexel + 0.5f) * WorldUnitsPerTexel;
}
} // namespace

FShadowViewData FDrawCollector::GetDirectionalSSMView(UWorld* World, FVector LightDir)
{
    FVector Up = (std::abs(LightDir.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
    FVector Right = LightDir.Cross(Up).Normalized();
    Up = Right.Cross(LightDir).Normalized();

    const FOctree* Octree = World->GetPartition().GetOctree();
    const FBoundingBox SceneBounds = Octree ? Octree->GetCellBounds() : FBoundingBox(FVector(-500), FVector(500));
    const FVector SceneCenter = SceneBounds.GetCenter();

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
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        const FVector CornerLS = LightView.TransformPositionWithW(Corners[CornerIndex]);
        MinLS.X = std::min(MinLS.X, CornerLS.X);
        MaxLS.X = std::max(MaxLS.X, CornerLS.X);
        MinLS.Y = std::min(MinLS.Y, CornerLS.Y);
        MaxLS.Y = std::max(MaxLS.Y, CornerLS.Y);
        MinLS.Z = std::min(MinLS.Z, CornerLS.Z);
        MaxLS.Z = std::max(MaxLS.Z, CornerLS.Z);
    }

    const float NearZ = MinLS.Z - 100.0f;
    const float FarZ = MaxLS.Z + 100.0f;
    const float Width = std::max(MaxLS.X - MinLS.X, 1.0f);
    const float Height = std::max(MaxLS.Y - MinLS.Y, 1.0f);

    const FVector OffsetLS(-(MinLS.X + MaxLS.X) * 0.5f, -(MinLS.Y + MaxLS.Y) * 0.5f, 0.0f);
    LightView = LightView * FMatrix::MakeTranslationMatrix(OffsetLS);

    FShadowViewData ShadowView = {};
    ShadowView.Set(LightView, FMatrix::MakeOrthographic(Width, Height, NearZ, FarZ), NearZ, FarZ, 0);
    return ShadowView;
}

TArray<FShadowViewData> FDrawCollector::GetDirectionalCSMViews(
    UWorld* World,
    FVector LightDir,
    const FSceneView* SceneView,
    int SliceCount,
    float ShadowDistance,
    float CascadeDistribution,
    uint32 ShadowResolution,
    float OutCascadeSplits[ShadowAtlas::MaxCascades + 1])
{
    TArray<FShadowViewData> Result;
    if (!SceneView || SliceCount <= 0)
    {
        return Result;
    }

    BuildCascadeSplitDistances(SceneView, ShadowDistance, CascadeDistribution, SliceCount, OutCascadeSplits);

    FVector Up = (std::abs(LightDir.Z) < 0.999f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
    FVector Right = LightDir.Cross(Up).Normalized();
    Up = Right.Cross(LightDir).Normalized();

    FMatrix LightViewBase = FMatrix::Identity;
    LightViewBase.M[0][0] = Right.X; LightViewBase.M[0][1] = Up.X; LightViewBase.M[0][2] = LightDir.X;
    LightViewBase.M[1][0] = Right.Y; LightViewBase.M[1][1] = Up.Y; LightViewBase.M[1][2] = LightDir.Y;
    LightViewBase.M[2][0] = Right.Z; LightViewBase.M[2][1] = Up.Z; LightViewBase.M[2][2] = LightDir.Z;

    const FOctree* Octree = World ? World->GetPartition().GetOctree() : nullptr;
    const FBoundingBox SceneBounds = Octree ? Octree->GetCellBounds() : FBoundingBox(FVector(-500), FVector(500));
    FVector SceneCorners[8];
    SceneBounds.GetCorners(SceneCorners);
    const uint32 EffectiveResolution = std::max(
        1u,
        ShadowResolution > (ShadowAtlas::Padding * 2u) ? ShadowResolution - (ShadowAtlas::Padding * 2u) : ShadowResolution);

    for (int i = 0; i < SliceCount; i++)
    {
        float NearSplit = OutCascadeSplits[i];
        float FarSplit = OutCascadeSplits[i + 1];

        // Cascade Blending을 위해 다음 캐스케이드가 이전 캐스케이드 영역을 약간 더 커버하도록 확장합니다.
        // 셰이더의 BlendZone(10%)과 일치시킵니다.
        if (i > 0)
        {
            float Overlap = (FarSplit - NearSplit) * 0.1f;
            NearSplit = std::max(OutCascadeSplits[0], NearSplit - Overlap);
        }

        FVector FrustumPoints[8];
        BuildPerspectiveFrustumCorners(SceneView, NearSplit, FarSplit, FrustumPoints);

        FVector AABBMin(FLT_MAX), AABBMax(-FLT_MAX);
        for (int j = 0; j < 8; j++)
        {
            FVector P = LightViewBase.TransformPositionWithW(FrustumPoints[j]);
            AABBMin.X = std::min(AABBMin.X, P.X);
            AABBMax.X = std::max(AABBMax.X, P.X);
            AABBMin.Y = std::min(AABBMin.Y, P.Y);
            AABBMax.Y = std::max(AABBMax.Y, P.Y);
            AABBMin.Z = std::min(AABBMin.Z, P.Z);
            AABBMax.Z = std::max(AABBMax.Z, P.Z);
        }

        float SceneMinZ = FLT_MAX;
        float SceneMaxZ = -FLT_MAX;
        for (int CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            const FVector CornerLS = LightViewBase.TransformPositionWithW(SceneCorners[CornerIndex]);
            SceneMinZ = std::min(SceneMinZ, CornerLS.Z);
            SceneMaxZ = std::max(SceneMaxZ, CornerLS.Z);
        }

        AABBMin.Z = std::min(AABBMin.Z, SceneMinZ) - 100.0f;
        AABBMax.Z = std::max(AABBMax.Z, SceneMaxZ) + 100.0f;

        const float Width = std::max(AABBMax.X - AABBMin.X, 1.0f);
        const float Height = std::max(AABBMax.Y - AABBMin.Y, 1.0f);
        const float NearZ = AABBMin.Z;
        const float FarZ = AABBMax.Z;

        FVector Center = (AABBMax + AABBMin) * 0.5f;
        Center.X = SnapToTexel(Center.X, Width / static_cast<float>(EffectiveResolution));
        Center.Y = SnapToTexel(Center.Y, Height / static_cast<float>(EffectiveResolution));
        FMatrix Translation = FMatrix::MakeTranslationMatrix(FVector(-Center.X, -Center.Y, 0.0f));
        FMatrix CascadeView = LightViewBase * Translation;
        FMatrix CascadeProj = FMatrix::MakeOrthographic(Width, Height, NearZ, FarZ);

        FShadowViewData ShadowView = {};
        ShadowView.Set(CascadeView, CascadeProj, NearZ, FarZ, 0);
        Result.push_back(ShadowView);
    }

    return Result;
}

FShadowViewData FDrawCollector::GetDirectionalPSMView(UWorld* World, FVector LightDir, const FSceneView* SceneView, float ShadowDistance)
{
    (void)World;

    FVector Corners[8];
    const float CameraNear = std::max(0.1f, SceneView->NearClip);
    const float ClampedShadowDistance = std::max(CameraNear + 100.0f, ShadowDistance);
    const float CameraFar = std::max(CameraNear + 100.0f, std::min(SceneView->FarClip, ClampedShadowDistance));
    BuildPerspectiveFrustumCorners(SceneView, CameraNear, CameraFar, Corners);

    float MaxBackDist = 0.0f;
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        const float Dist = (Corners[CornerIndex] - SceneView->CameraPosition).Dot(SceneView->CameraForward);
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

    FMatrix ViewPP;
    FMatrix ProjPP;
    float NearPP = 0.0f;
    float FarPP = 1.0f;
    {
        const FMatrix WorldToPPS = VCView * VCProj;
        FVector CornersPPS[8];
        FVector CenterPPS(0.0f);
        for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            CornersPPS[CornerIndex] = WorldToPPS.TransformPositionWithW(Corners[CornerIndex]);
            CenterPPS += CornersPPS[CornerIndex];
        }
        CenterPPS /= 8.0f;

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
        for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            const FVector CornerLS = ViewPP.TransformPositionWithW(CornersPPS[CornerIndex]);
            MinLS.X = std::min(MinLS.X, CornerLS.X);
            MaxLS.X = std::max(MaxLS.X, CornerLS.X);
            MinLS.Y = std::min(MinLS.Y, CornerLS.Y);
            MaxLS.Y = std::max(MaxLS.Y, CornerLS.Y);
            MinLS.Z = std::min(MinLS.Z, CornerLS.Z);
            MaxLS.Z = std::max(MaxLS.Z, CornerLS.Z);
        }

        const float Width = std::max(0.01f, MaxLS.X - MinLS.X);
        const float Height = std::max(0.01f, MaxLS.Y - MinLS.Y);
        NearPP = MinLS.Z - 0.1f;
        FarPP = MaxLS.Z + 0.1f;

        const FVector OffsetLS(-(MinLS.X + MaxLS.X) * 0.5f, -(MinLS.Y + MaxLS.Y) * 0.5f, 0.0f);
        ViewPP = ViewPP * FMatrix::MakeTranslationMatrix(OffsetLS);
        ProjPP = FMatrix::MakeOrthographic(Width, Height, NearPP, FarPP);
    }

    FShadowViewData ShadowView = {};
    ShadowView.Set(VCView * VCProj * ViewPP, ProjPP, NearPP, FarPP, 0);
    return ShadowView;
}

void FDrawCollector::ComputeDirectionalShadowMatrices(FLightProxy* Light, UWorld* World, const FSceneView* SceneView)
{
    const FCascadeShadowMapData* ExistingCascadeData = Light->GetCascadeShadowMapData();
    FCascadeShadowMapData* CascadeShadowMapData = Light->GetCascadeShadowMapData();
    if (!ExistingCascadeData || !CascadeShadowMapData)
    {
        return;
    }

    FShadowViewData ShadowView = {};
    const FVector LightDir = Light->LightProxyInfo.Direction.Normalized();
    const EShadowMapMethod Method = GetShadowMapMethod();

    if (Method == EShadowMapMethod::Cascade)
    {
        const uint32 Count = static_cast<uint32>(std::clamp(Light->GetCascadeCountSetting(), 1, static_cast<int32>(ShadowAtlas::MaxCascades)));
        float CascadeSplits[ShadowAtlas::MaxCascades + 1] = {};
        TArray<FShadowViewData> CascadeViews = GetDirectionalCSMViews(
            World,
            LightDir,
            SceneView,
            static_cast<int>(Count),
            Light->GetDynamicShadowDistanceSetting(),
            Light->GetCascadeDistributionSetting(),
            Light->ShadowResolution,
            CascadeSplits);
        
        CascadeShadowMapData->CascadeCount = Count;
        for (uint32 i = 0; i < Count; i++)
        {
            CascadeShadowMapData->CascadeViews[i] = CascadeViews[i];
            CascadeShadowMapData->CascadeViewProj[i] = CascadeViews[i].ViewProj;
        }

        for (uint32 SplitIndex = 0; SplitIndex <= Count; ++SplitIndex)
        {
            CascadeShadowMapData->CascadeSplits[SplitIndex] = CascadeSplits[SplitIndex];
        }

        // CSM 컬링을 위해 모든 캐스케이드를 포함할 수 있는 SSM 뷰를 대표 ViewProj로 설정합니다.
        ShadowView = GetDirectionalSSMView(World, LightDir);
    }
    else
    {
        switch (Method)
        {
        case EShadowMapMethod::Standard:
            ShadowView = GetDirectionalSSMView(World, LightDir);
            break;
        case EShadowMapMethod::PSM:
            ShadowView = GetDirectionalPSMView(World, LightDir, SceneView, Light->GetDynamicShadowDistanceSetting());
            break;
        default:
            ShadowView = GetDirectionalSSMView(World, LightDir);
            break;
        }

        // CSM이 아닐 때는 CascadeCount를 1로 강제하고 데이터를 초기화합니다.
        CascadeShadowMapData->Reset(); 
        CascadeShadowMapData->CascadeCount = 1;
        CascadeShadowMapData->CascadeViews[0] = ShadowView;
        CascadeShadowMapData->CascadeViewProj[0] = ShadowView.ViewProj;
    }

    Light->LightViewProj = ShadowView.ViewProj;
}

void FDrawCollector::ComputeSpotShadowMatrices(FLightProxy* Light)
{
    FShadowViewData* SpotShadowView = Light->GetSpotShadowView();
    if (!SpotShadowView)
    {
        return;
    }

    const FLightProxyInfo& LC = Light->LightProxyInfo;
    const FVector LightPos = LC.Position;
    const FVector LightDir = LC.Direction.Normalized();

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

    const float NearZ = 1.0f;
    const float FarZ = std::max(NearZ + 0.001f, LC.AttenuationRadius);
    const float FOV = LC.OuterConeAngle * 2.0f * (3.141592f / 180.0f);
    const FMatrix LightProj = FMatrix::MakePerspective(FOV, 1.0f, NearZ, FarZ);

    SpotShadowView->Set(LightView, LightProj, NearZ, FarZ, 1);
    Light->LightViewProj = SpotShadowView->ViewProj;
}

void FDrawCollector::ComputePointShadowMatrices(FLightProxy* Light)
{
    FCubeShadowMapData* CubeShadowMapData = Light->GetCubeShadowMapData();
    FMatrix* PointShadowViewProjMatrices = Light->GetPointShadowViewProjMatrices();
    if (!CubeShadowMapData || !PointShadowViewProjMatrices)
    {
        return;
    }

    const FLightProxyInfo& LC = Light->LightProxyInfo;
    const FVector LightPos = LC.Position;
    const float NearZ = 1.0f;
    const float FarZ = std::max(NearZ + 0.001f, LC.AttenuationRadius);
    const FMatrix LightProjCube = FMatrix::MakePerspective(0.5f * 3.141592f, 1.0f, NearZ, FarZ);

    struct FFaceDir
    {
        FVector Forward;
        FVector Up;
    };

    const FFaceDir Faces[ShadowAtlas::MaxPointFaces] = {
        {{1, 0, 0}, {0, 1, 0}},
        {{-1, 0, 0}, {0, 1, 0}},
        {{0, 1, 0}, {0, 0, -1}},
        {{0, -1, 0}, {0, 0, 1}},
        {{0, 0, 1}, {0, 1, 0}},
        {{0, 0, -1}, {0, 1, 0}},
    };

    for (uint32 FaceIndex = 0; FaceIndex < ShadowAtlas::MaxPointFaces; ++FaceIndex)
    {
        const FVector Forward = Faces[FaceIndex].Forward;
        const FVector Up = Faces[FaceIndex].Up;
        const FVector Right = Up.Cross(Forward).Normalized();

        FMatrix LightView = FMatrix::Identity;
        LightView.M[0][0] = Right.X; LightView.M[0][1] = Up.X; LightView.M[0][2] = Forward.X;
        LightView.M[1][0] = Right.Y; LightView.M[1][1] = Up.Y; LightView.M[1][2] = Forward.Y;
        LightView.M[2][0] = Right.Z; LightView.M[2][1] = Up.Z; LightView.M[2][2] = Forward.Z;
        LightView.M[3][0] = -LightPos.Dot(Right);
        LightView.M[3][1] = -LightPos.Dot(Up);
        LightView.M[3][2] = -LightPos.Dot(Forward);

        CubeShadowMapData->FaceViews[FaceIndex].Set(LightView, LightProjCube, NearZ, FarZ, 1);
        PointShadowViewProjMatrices[FaceIndex] = CubeShadowMapData->FaceViews[FaceIndex].ViewProj;
        CubeShadowMapData->FaceViewProj[FaceIndex] = CubeShadowMapData->FaceViews[FaceIndex].ViewProj;
    }

    Light->LightViewProj = PointShadowViewProjMatrices[0];
}

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
            ComputeDirectionalShadowMatrices(Light, World, SceneView);
            Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);

            const FConvexVolume& CasterQueryFrustum =
                GetShadowMapMethod() == EShadowMapMethod::PSM ? SceneView->FrustumVolume : Light->ShadowViewFrustum;
            World->GetPartition().QueryFrustumAllProxies(CasterQueryFrustum, Light->VisibleShadowCasters);
            continue;
        }

        if (LC.LightType == static_cast<uint32>(ELightType::Spot))
        {
            ComputeSpotShadowMatrices(Light);
            Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);
            World->GetPartition().QueryFrustumAllProxies(Light->ShadowViewFrustum, Light->VisibleShadowCasters);
            continue;
        }

        if (LC.LightType == static_cast<uint32>(ELightType::Point))
        {
            ComputePointShadowMatrices(Light);
            Light->ShadowViewFrustum.UpdateFromMatrix(Light->LightViewProj);
            World->GetPartition().QuerySphereAllProxies({LC.Position, LC.AttenuationRadius}, Light->VisibleShadowCasters);
        }
    }
}
