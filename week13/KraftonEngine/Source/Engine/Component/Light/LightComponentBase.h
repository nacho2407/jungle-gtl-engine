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

struct FMinimalViewInfo;

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

	// 런타임 동적 변경 — 값 갱신 후 PushToScene 으로 렌더 측에 즉시 반영.
	void SetIntensity(float V) { Intensity = V; PushToScene(); }
	void SetLightColor(const FVector4& V) { LightColor = V; PushToScene(); }

	virtual ELightComponentType GetLightType() const { return ELightComponentType::Unknown; }
	// CSM/포인트 큐브맵 등 그림자 시점 매트릭스 빌드. Directional 은 viewer POV 의 frustum
	// 을 cascade 분할에 사용하므로 통화가 필요. Point/Spot 은 통화를 받지만 무시.
	virtual bool GetLightViewProj(FLightViewProjResult& OutResult, const FMinimalViewInfo* POV = nullptr, int32 FaceIndex = 0) const { return false; }
	class UBillboardComponent* EnsureEditorBillboard();

protected:
	float Intensity = 1.f;
	FVector4 LightColor = { 1.0f,1.0f,1.0f,1.0f };
	bool bVisible = true;
	bool bCastShadows = true;
};