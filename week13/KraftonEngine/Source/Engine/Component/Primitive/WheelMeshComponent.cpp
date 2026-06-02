#include "Component/Primitive/WheelMeshComponent.h"

#include "Render/Proxy/WheelMeshSceneProxy.h"

#include <algorithm>

FPrimitiveSceneProxy* UWheelMeshComponent::CreateSceneProxy()
{
	return new FWheelMeshSceneProxy(this);
}

void UWheelMeshComponent::SetSuspensionLoad(float InSuspensionLoad, const FVector& InContactNormal, float InWheelRadius, bool bInAir)
{
	WheelRadius = std::max(InWheelRadius, 0.0f);
	ContactNormal = InContactNormal;
	if (ContactNormal.IsNearlyZero())
	{
		ContactNormal = FVector(0.0f, 0.0f, 1.0f);
	}
	else
	{
		ContactNormal.Normalize();
	}

	const float SafeReferenceLoad = std::max(ReferenceLoad, 1.0f);
	const float CompressionRatio = bEnableTireDeformation && !bInAir
		? std::clamp(InSuspensionLoad / SafeReferenceLoad, 0.0f, 1.0f)
		: 0.0f;
	DeformationDepth = CompressionRatio * std::max(MaxDeformationDepth, 0.0f);
}
