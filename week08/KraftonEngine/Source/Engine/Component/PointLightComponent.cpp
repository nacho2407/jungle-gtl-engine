#include "PointLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent)

void UPointLightComponent::Serialize(FArchive& Ar)
{
    ULightComponent::Serialize(Ar);
    Ar << AttenuationRadius;
    Ar << LightFalloffExponent;
	
	// ─── UE5에 있는 프로퍼티 중 사용할 수도 있을 것처럼 보이는 속성들 (에디터 창 노출, 직렬화 X)─────
    // Ar << SoftSourceRadius;
    // Ar << SourceLength;
    // Ar << bUseInverseSquaredFalloff;
}

void UPointLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "AttenuationRadius", EPropertyType::Float, &AttenuationRadius, 0.0f, 10000.0f, 10.0f });
    OutProps.push_back({ "LightFalloffExponent", EPropertyType::Float, &LightFalloffExponent, 0.0f, 20.0f, 0.1f });
    OutProps.push_back({ "bAffectsWorld", EPropertyType::Bool, &bAffectsWorld });
    OutProps.push_back({ "bCastShadows", EPropertyType::Bool, &bCastShadows });
}

void UPointLightComponent::PostEditProperty(const char* PropertyName)
{
    ULightComponent::PostEditProperty(PropertyName);
}
