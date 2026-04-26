#pragma once
#include "Component/SceneComponent.h"
#include "Math/Matrix.h"

enum class ELightComponentType : uint8
{
	Ambient,
	Directional,
	Point,
	Spot,
	Unknown
};

struct FLightViewProjResult
{
	FMatrix View;
	FMatrix Proj;
	bool bIsOrtho = false;
};

class UCameraComponent;

class ULightComponentBase : public USceneComponent
{
public:
	DECLARE_CLASS(ULightComponentBase, USceneComponent)

	ULightComponentBase() { SetComponentTickEnabled(false); }

	virtual void PushToScene() {};
	virtual void DestroyFromScene() {};
	virtual void OnTransformDirty() override { USceneComponent::OnTransformDirty(); PushToScene(); }
	virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	virtual void PostEditProperty(const char* PropertyName) override { USceneComponent::PostEditProperty(PropertyName); PushToScene(); }
	virtual void CreateRenderState() override { PushToScene(); }
	virtual void DestroyRenderState() override { DestroyFromScene(); }

	virtual void Serialize(FArchive& Ar) override;

	float GetIntensity() const { return Intensity; }
	FVector4 GetLightColor() const { return LightColor; }
	bool IsVisible() const { return bVisible; }
	bool CastShadows() const { return bCastShadows; }

	virtual ELightComponentType GetLightType() const { return ELightComponentType::Unknown; }
	virtual bool GetLightViewProj(FLightViewProjResult& OutResult, const UCameraComponent* Camera = nullptr, int32 FaceIndex = 0) const { return false; }
	class UBillboardComponent* EnsureEditorBillboard();

protected:
	float Intensity = 1.f;
	FVector4 LightColor = { 1.0f,1.0f,1.0f,1.0f };
	bool bVisible = true;
	bool bCastShadows = true;
};