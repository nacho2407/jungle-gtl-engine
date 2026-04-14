#include "ExponentialHeightFogComponent.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Proxy/FScene.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(UExponentialHeightFogComponent, USceneComponent)

void UExponentialHeightFogComponent::CreateRenderState()
{
	if (!Owner)
	{
		return;
	}

	if (UWorld* World = Owner->GetWorld())
	{
		World->GetScene().RegisterExponentialHeightFog(this);
	}
}

void UExponentialHeightFogComponent::DestroyRenderState()
{
	if (!Owner)
	{
		return;
	}

	if (UWorld* World = Owner->GetWorld())
	{
		World->GetScene().UnregisterExponentialHeightFog(this);
	}
}

void UExponentialHeightFogComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << FogDensity;
	Ar << FogHeightFalloff;
	Ar << FogColor;
	Ar << FogMaxOpacity;
	Ar << StartDistance;
	Ar << EndDistance;
	Ar << FogCutoffDistance;

	if (Ar.IsLoading())
	{
		ClampProperties();
	}
}

void UExponentialHeightFogComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Fog Density", EPropertyType::Float, &FogDensity, 0.0f, 10.0f, 0.01f });
	OutProps.push_back({ "Fog Height Falloff", EPropertyType::Float, &FogHeightFalloff, 0.0f, 2.0f, 0.01f });
	OutProps.push_back({ "Fog Color", EPropertyType::Vec4, &FogColor, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Fog Max Opacity", EPropertyType::Float, &FogMaxOpacity, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Start Distance", EPropertyType::Float, &StartDistance, 0.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "End Distance", EPropertyType::Float, &EndDistance, 0.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Fog Cutoff Distance", EPropertyType::Float, &FogCutoffDistance, 0.0f, 100000.0f, 1.0f });
}

void UExponentialHeightFogComponent::PostEditProperty(const char* PropertyName)
{
	USceneComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Fog Density") == 0
		|| strcmp(PropertyName, "Fog Height Falloff") == 0
		|| strcmp(PropertyName, "Fog Color") == 0
		|| strcmp(PropertyName, "Fog Max Opacity") == 0
		|| strcmp(PropertyName, "Start Distance") == 0
		|| strcmp(PropertyName, "End Distance") == 0
		|| strcmp(PropertyName, "Fog Cutoff Distance") == 0)
	{
		ClampProperties();
	}
}

void UExponentialHeightFogComponent::ClampProperties()
{
	FogDensity = std::max(FogDensity, 0.0f);
	FogHeightFalloff = Clamp(FogHeightFalloff, 0.0f, 2.0f);
	FogColor.X = Clamp(FogColor.X, 0.0f, 1.0f);
	FogColor.Y = Clamp(FogColor.Y, 0.0f, 1.0f);
	FogColor.Z = Clamp(FogColor.Z, 0.0f, 1.0f);
	FogColor.W = 1.0f;
	FogMaxOpacity = Clamp(FogMaxOpacity, 0.0f, 1.0f);
	StartDistance = std::max(StartDistance, 0.0f);
	EndDistance = std::max(EndDistance, 0.0f);
	FogCutoffDistance = std::max(FogCutoffDistance, 0.0f);
}
