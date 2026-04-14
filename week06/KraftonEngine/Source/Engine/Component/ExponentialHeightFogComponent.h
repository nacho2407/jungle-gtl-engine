#pragma once

#include "Component/SceneComponent.h"

class UExponentialHeightFogComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UExponentialHeightFogComponent, USceneComponent)

	UExponentialHeightFogComponent() = default;
	~UExponentialHeightFogComponent() override = default;

	void CreateRenderState() override;
	void DestroyRenderState() override;

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	float GetFogHeight() const { return GetWorldLocation().Z; }
	float GetFogDensity() const { return FogDensity; }
	float GetFogHeightFalloff() const { return FogHeightFalloff; }
	const FVector4& GetFogColor() const { return FogColor; }
	float GetFogMaxOpacity() const { return FogMaxOpacity; }
	float GetStartDistance() const { return StartDistance; }
	float GetEndDistance() const { return EndDistance; }
	float GetFogCutoffDistance() const { return FogCutoffDistance; }

private:
	void ClampProperties();

private:
	float FogDensity = 0.02f;
	float FogHeightFalloff = 0.2f;
	FVector4 FogColor = FVector4(0.447f, 0.638f, 1.0f, 1.0f);
	float FogMaxOpacity = 1.0f;
	float StartDistance = 0.0f;
	float EndDistance = 0.0f;
	float FogCutoffDistance = 0.0f;
};
