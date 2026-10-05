#include "PointLightComponent.h"
#include <Render/Resource/ShadowAtlasManager.h>


void UPointLightComponent::PostDuplicate(UObject* Original)
{
	ULightComponent::PostDuplicate(Original);
}


FMatrix UPointLightComponent::ComputePerspectiveShadowMatrix(const FMatrix& CamView, const FMatrix& CamProj,
	const TArray<FBoundingBox>* VisibleObjectsBounds) const
{
	return FMatrix::Identity;
}
