#include "LightRenderCollector.h"

#include "Component/PostProcess/Light/AmbientLightComponent.h"
#include "Component/PostProcess/Light/DirectionalLightComponent.h"
#include "Component/PostProcess/Light/LightComponentBase.h"
#include "Component/PostProcess/Light/PointLightComponent.h"
#include "Component/PostProcess/Light/SpotlightComponent.h"
#include "Math/Utils.h"
#include "Render/Scene/RenderBus.h"

void FLightRenderCollector::Reset()
{
    LastStats = {};
}

void FLightRenderCollector::CollectLight(const ULightComponentBase* Light, FRenderBus& RenderBus)
{
    if (!Light) return;
    const auto& LightColor = Light->LightColor;
    FVector Color = FVector(LightColor.R, LightColor.G, LightColor.B);

    LastStats.TotalLightCount += 1;

    if (const UAmbientLightComponent* AmbientLight = Cast<UAmbientLightComponent>(Light))
    {
        FAmbientLightInfo AmbientLightData = {};
        AmbientLightData.Color = Color;
        AmbientLightData.Intensity = AmbientLight->Intensity;
        RenderBus.AmbientLightInfo = AmbientLightData;
    }
    else if (const UDirectionalLightComponent* DirLight = Cast<UDirectionalLightComponent>(Light))
    {
        FDirectionalLightInfo DirLightData = {};
        DirLightData.Color = Color;
        DirLightData.Intensity = DirLight->Intensity;
        DirLightData.Direction = DirLight->GetForwardVector();
        RenderBus.DirectionalLightInfo = DirLightData;

        if (RenderBus.GetShowFlags().bShadow && DirLight->bCastShadows)
        {
            FShadowLightRequest DirLightDataShadow = {};
            DirLightDataShadow.LightIndex = InvalidShadowIndex;
            DirLightDataShadow.LightComponent = const_cast<UDirectionalLightComponent*>(DirLight);
            DirLightDataShadow.Type = EShadowLightType::SLT_Directional;
            DirLightDataShadow.bCastShadows = true;
            DirLightDataShadow.WorldLocation = DirLight->GetWorldLocation();
            DirLightDataShadow.ConstantBias = DirLight->ConstantBias;
            DirLightDataShadow.SlopeScaledBias = DirLight->SlopeScaledBias;
            DirLightDataShadow.ShadowSharpen = DirLight->ShadowSharpen;
            DirLightDataShadow.ShadowResolution = DirLight->ShadowResolutionScale;
            RenderBus.ShadowLightRequests.push_back(DirLightDataShadow);
        }

        LastStats.DirectionalLightCount += 1;
        if (DirLight->bCastShadows)
            LastStats.ShadowCastingLightCount += 1;
    }

    if (const USpotlightComponent* SpotLight = Cast<USpotlightComponent>(Light))
    {
        const uint32 LightIndex = static_cast<uint32>(RenderBus.LightInfos.size());
        FLightInfo LightData = {};
        LightData.Color = Color;
        LightData.Intensity = SpotLight->Intensity;
        LightData.Position = SpotLight->GetWorldLocation();
        LightData.Direction = SpotLight->GetForwardVector();
        LightData.Radius = SpotLight->AttenuationRadius;
        LightData.Falloff = SpotLight->LightFalloffExponent;
        LightData.InnerAngle = MathUtil::DegreesToRadians(SpotLight->InnerConeAngle);
        LightData.OuterAngle = MathUtil::DegreesToRadians(SpotLight->OuterConeAngle);
        LightData.Type = 0;
        RenderBus.LightInfos.push_back(LightData);

        if (RenderBus.GetShowFlags().bShadow && SpotLight->bCastShadows)
        {
            FShadowLightRequest ShadowRequest = {};
            ShadowRequest.LightIndex = LightIndex;
            ShadowRequest.LightComponent = const_cast<USpotlightComponent*>(SpotLight);
            ShadowRequest.Type = EShadowLightType::SLT_Spot;
            ShadowRequest.bCastShadows = true;
            ShadowRequest.ShadowResolution = SpotLight->ShadowResolutionScale;
            ShadowRequest.WorldLocation = SpotLight->GetWorldLocation();
            ShadowRequest.ConstantBias = SpotLight->ConstantBias;
            ShadowRequest.SlopeScaledBias = SpotLight->SlopeScaledBias;
            ShadowRequest.ShadowSharpen = SpotLight->ShadowSharpen;
            RenderBus.ShadowLightRequests.push_back(ShadowRequest);
        }

        LastStats.SpotlightCount += 1;
        if (SpotLight->bCastShadows)
            LastStats.ShadowCastingLightCount += 1;
    }
    else if (const UPointLightComponent* PointLight = Cast<UPointLightComponent>(Light))
    {
        FLightInfo LightData = {};
        LightData.Color = Color;
        LightData.Intensity = PointLight->Intensity;
        LightData.Position = PointLight->GetWorldLocation();
        LightData.Radius = PointLight->AttenuationRadius;
        LightData.Falloff = PointLight->LightFalloffExponent;
        LightData.Type = 1;
        LightData.ShadowTextureIndex = InvalidShadowIndex;
        RenderBus.LightInfos.push_back(LightData);

        if (RenderBus.GetShowFlags().bShadow && PointLight->bCastShadows)
        {
            FShadowLightRequest PointLightDataShadow = {};
            PointLightDataShadow.LightIndex = static_cast<uint32>(RenderBus.LightInfos.size() - 1);
            PointLightDataShadow.LightComponent = const_cast<UPointLightComponent*>(PointLight);
            PointLightDataShadow.Type = EShadowLightType::SLT_Point;
            PointLightDataShadow.bCastShadows = true;
            PointLightDataShadow.ShadowResolution = PointLight->ShadowResolutionScale;
            PointLightDataShadow.WorldLocation = PointLight->GetWorldLocation();
            PointLightDataShadow.ConstantBias = PointLight->ConstantBias;
            PointLightDataShadow.SlopeScaledBias = PointLight->SlopeScaledBias;
            PointLightDataShadow.ShadowSharpen = PointLight->ShadowSharpen;
            RenderBus.ShadowLightRequests.push_back(PointLightDataShadow);
        }

        LastStats.PointLightCount += 1;
        if (PointLight->bCastShadows)
            LastStats.ShadowCastingLightCount += 1;
    }
}
