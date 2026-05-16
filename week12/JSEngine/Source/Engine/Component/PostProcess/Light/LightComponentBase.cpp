#include "LightComponentBase.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(ULightComponentBase, USceneComponent)
REGISTER_FACTORY(ULightComponentBase)

void ULightComponentBase::PostDuplicate(UObject* Original)
{
	USceneComponent::PostDuplicate(Original);
	const ULightComponentBase* Orig = Cast<ULightComponentBase>(Original);

	LightColor = Orig->LightColor;
}

