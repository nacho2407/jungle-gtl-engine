#include "LightComponentBase.h"

void ULightComponentBase::PostDuplicate(UObject* Original)
{
	USceneComponent::PostDuplicate(Original);
	const ULightComponentBase* Orig = Cast<ULightComponentBase>(Original);

	LightColor = Orig->LightColor;
}
