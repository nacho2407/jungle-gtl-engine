#pragma once

#include "SceneComponent.h"
#include "Render/Types/FogParams.h"

class UHeightFogComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UHeightFogComponent, USceneComponent)

	UHeightFogComponent();

	void CreateRenderState() override;
	void DestroyRenderState() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	void Serialize(FArchive& Ar) override;

	// Transform 변경 시 FogBaseHeight 갱신
	void OnTransformDirty() override;

private:
	void PushToScene();

	float FogDensity        = 0.02f;
	float FogHeightFalloff  = 0.2f;
	float StartDistance     = 0.0f;
	float FogCutoffDistance = 0.0f;
	float FogMaxOpacity     = 1.0f;
	FVector4 FogInscatteringColor = FVector4(0.45f, 0.55f, 0.65f, 1.0f);
};
