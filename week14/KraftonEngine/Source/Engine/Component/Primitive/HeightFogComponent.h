#pragma once

#include "Component/SceneComponent.h"
#include "Render/Types/FogParams.h"


#include "Source/Engine/Component/Primitive/HeightFogComponent.generated.h"

UCLASS()
class UHeightFogComponent : public USceneComponent
{
public:
	GENERATED_BODY()
	UHeightFogComponent();

	void CreateRenderState() override;
	void DestroyRenderState() override;

	void PostEditProperty(const char* PropertyName) override;

	// Transform 변경 시 FogBaseHeight 갱신
	void OnTransformDirty() override;

	class UBillboardComponent* EnsureEditorBillboard();

private:
	void PushToScene();

	UPROPERTY(Edit, Save, Category="Fog", DisplayName="Fog Density", Min=0.0f, Max=0.05f, Speed=0.001f)
	float FogDensity        = 0.02f;
	UPROPERTY(Edit, Save, Category="Fog", DisplayName="Height Falloff", Min=0.001f, Max=5.0f, Speed=0.01f)
	float FogHeightFalloff  = 0.2f;
	UPROPERTY(Edit, Save, Category="Fog", DisplayName="Start Distance", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float StartDistance     = 0.0f;
	UPROPERTY(Edit, Save, Category="Fog", DisplayName="Cutoff Distance", Min=0.0f, Max=100000.0f, Speed=1.0f)
	float FogCutoffDistance = 0.0f;
	UPROPERTY(Edit, Save, Category="Fog", DisplayName="Max Opacity", Min=0.0f, Max=1.0f, Speed=0.01f)
	float FogMaxOpacity     = 1.0f;
	UPROPERTY(Edit, Save, Category="Fog", DisplayName="Inscattering Color", Type=Color4)
	FVector4 FogInscatteringColor = FVector4(0.45f, 0.55f, 0.65f, 1.0f);
};
