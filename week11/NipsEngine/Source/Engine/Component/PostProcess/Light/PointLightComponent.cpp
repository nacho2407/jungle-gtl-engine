#include "PointLightComponent.h"
#include "Object/ObjectFactory.h"
#include <Render/Resource/ShadowAtlasManager.h>

DEFINE_CLASS(UPointLightComponent, ULightComponent)
REGISTER_FACTORY(UPointLightComponent)

void UPointLightComponent::PostDuplicate(UObject* Original)
{
    ULightComponent::PostDuplicate(Original);
}

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Attenuation Radius", EPropertyType::Float, &AttenuationRadius });
    OutProps.push_back({ "Light Falloff", EPropertyType::Float, &LightFalloffExponent });
}

void UPointLightComponent::Serialize(FArchive& Ar)
{
	ULightComponent::Serialize(Ar);
	Ar << "AttenuationRadius" << AttenuationRadius;
	Ar << "LightFalloffExponent" << LightFalloffExponent;
}

FMatrix UPointLightComponent::ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	return FMatrix::Identity;
}

void UPointLightComponent::PrintShadowMapDebugInfo(TArray<FPropertyDescriptor>& OutProps) const
{
    FShadowAtlasManager& AtlasManager = FShadowAtlasManager::Get();

	static const char* ShadowMapFaceNames[] = { "PositiveX", "NegativeX", "PositiveY", "NegativeY", "PositiveZ", "NegativeZ" };

	if (!bHasDebugShadowCubeTile)
	{
		return;
    }

	static ID3D11ShaderResourceView* FaceSRV[6];

	for (int i = 0; i < 6; i++)
    {	
		FaceSRV[i] = AtlasManager.GetCubeDebugSRV(static_cast<int>(DebugShadowCubeIndex), i);
        if (!FaceSRV[i]) return;
    }

	static FSRVDisplayInfo ShadowMapDisplay;
    ShadowMapDisplay = {
        64.f,
        64.f,
        0,
        0,
        64.f,
        64.f
    };

    OutProps.push_back({ "CubeMap",
                         EPropertyType::CubeSRV,
                         FaceSRV, 0.f, 0.f, 0.f, nullptr , 0});
}
