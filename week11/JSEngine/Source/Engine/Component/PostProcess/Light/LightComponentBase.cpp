#include "LightComponentBase.h"
#include "Object/ObjectFactory.h"
#include "Core/ReflectionUtils.h"

DEFINE_CLASS(ULightComponentBase, USceneComponent)
REGISTER_FACTORY(ULightComponentBase)

void ULightComponentBase::PostDuplicate(UObject* Original)
{
    USceneComponent::PostDuplicate(Original);
    const ULightComponentBase* Orig = Cast<ULightComponentBase>(Original);

    LightColor = Orig->LightColor;
}

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ReflectionUtils::AppendGeneratedPropertiesRecursive(this, GetStaticClass(), OutProps);
	constexpr EPropertyUsageFlags EditAndAnimate =
		EPropertyUsageFlags::Editable | EPropertyUsageFlags::Animatable;
    OutProps.push_back({ "Color", EPropertyType::Color, &LightColor, 0.0f, 0.0f, 0.1f, nullptr, 0, nullptr, EditAndAnimate });
    OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity, 0.0f, 0.0f, 0.1f, nullptr, 0, nullptr, EditAndAnimate });
	OutProps.push_back({ "Cast Shadows", EPropertyType::Bool, &bCastShadows });
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
    ReflectionUtils::SerializeGeneratedPropertiesLocal(this, &ULightComponentBase::StaticClassInfo, Ar);
}
