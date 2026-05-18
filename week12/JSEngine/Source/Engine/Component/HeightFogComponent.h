#pragma once
#include "PrimitiveComponent.h"

UCLASS(SpawnableComponent, DisplayName = "HeightFog Component", Category = "Basic")
class UHeightFogComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UHeightFogComponent, UPrimitiveComponent)

	UHeightFogComponent();
	~UHeightFogComponent() override = default;

	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_FOG; }

	void SetFogDensity(float InFogDensity) { FogDensity = InFogDensity; }
	float GetFogDensity() const { return FogDensity; }

	void SetHeightFalloff(float InHeightFalloff) { HeightFalloff = InHeightFalloff; }
	float GetHeightFalloff() const { return HeightFalloff; }

	void SetFogInscatteringColor(const FVector4& InColor) { FogInscatteringColor = FColor(InColor.X, InColor.Y, InColor.Z, InColor.W); }
	FVector4 GetFogInscatteringColor() const { return FogInscatteringColor.ToVector4(); }

	void SetFogHeight(float InFogHeight) { FogHeight = InFogHeight; }
	float GetFogHeight() const { return FogHeight; }

	void SetFogStartDistance(float InFogStartDistance) { FogStartDistance = InFogStartDistance; }
	float GetFogStartDistance() const { return FogStartDistance; }

	void SetFogCutoffDistance(float InCutoffDistance) { FogCutoffDistance = InCutoffDistance; }
	float GetFogCutoffDistance() const { return FogCutoffDistance; }

	void SetFogMaxOpacity(float InFogMaxOpacity) { FogMaxOpacity = InFogMaxOpacity; }
	float GetFogMaxOpacity() const { return FogMaxOpacity; }

private:
	UPROPERTY(DisplayName = "Fog Inscattering Color")
	FColor FogInscatteringColor;

	UPROPERTY(DisplayName = "Fog Density", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float FogDensity = 0;

	UPROPERTY(DisplayName = "Height Falloff", Min = 0.0f, Max = 10.0f, Speed = 0.01f)
	float HeightFalloff = 0;

	UPROPERTY(DisplayName = "Fog Height")
	float FogHeight = 0;

	UPROPERTY(DisplayName = "Fog Start Distance", Min = 0.0f)
	float FogStartDistance = 0;

	UPROPERTY(DisplayName = "Fog Cutoff Distance")
	float FogCutoffDistance = 1000;

	UPROPERTY(DisplayName = "Fog Max Opacity", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float FogMaxOpacity = 1.f;

	// UPrimitiveComponent을(를) 통해 상속됨
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
};
