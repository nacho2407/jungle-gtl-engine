#pragma once
#include "Object/Reflection/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "GameFramework/Camera/CameraTypes.h"
#include "Math/MathUtils.h"
#include "Math/Vector.h"

struct FMinimalViewInfo;

struct FCameraState
{
	float FOV = 3.14159265358979f / 3.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearZ = 0.1f;
	float FarZ = 1000.0f;
	float OrthoWidth = 10.0f;
	bool bIsOrthogonal = false;
};

#include "Source/Engine/Component/Camera/CameraComponent.generated.h"

UCLASS()
class UCameraComponent : public USceneComponent
{
public:
	GENERATED_BODY()

	UCameraComponent() = default;

	void BeginPlay() override;
	void EndPlay() override;
	class UBillboardComponent* EnsureEditorBillboard();


	void LookAt(const FVector& Target);
	void SetCameraState(const FCameraState& NewState);
	const FCameraState& GetCameraState() const { return CameraState; }
	void SetDepthOfFieldSettings(const FCameraDepthOfFieldSettings& NewSettings) { DepthOfField = NewSettings; }
	const FCameraDepthOfFieldSettings& GetDepthOfFieldSettings() const { return DepthOfField; }
	FCameraDepthOfFieldSettings& GetMutableDepthOfFieldSettings() { return DepthOfField; }

	// 카메라 POV 통화 산출 — UE: UCameraComponent::GetCameraView.
	// CameraManager / RenderPipeline 이 이걸 받아 매트릭스/프러스텀을 빌드한다.
	// DeltaTime 은 향후 카메라 lag / interpolation 에 쓰이도록 시그니처 보존.
	void GetCameraView(float DeltaTime, FMinimalViewInfo& OutPOV) const;

	void SetFOV(float InFOV) { CameraState.FOV = InFOV; }
	void SetOrthoWidth(float InWidth) { CameraState.OrthoWidth = InWidth; }
	void SetOrthographic(bool bOrtho) { CameraState.bIsOrthogonal = bOrtho; }

	void OnResize(int32 Width, int32 Height);

	float GetFOV() const { return CameraState.FOV; }
	float GetNearPlane() const { return CameraState.NearZ; }
	float GetFarPlane() const { return CameraState.FarZ; }
	float GetOrthoWidth() const { return CameraState.OrthoWidth; }
	bool IsOrthogonal() const { return CameraState.bIsOrthogonal; }

private:
	UPROPERTY(Edit, Save, Category="Camera", DisplayName="FOV (deg)", Member=CameraState.FOV, Type=Float, Min=0.1f, Max=3.14f, Speed=0.01f, DisplayUnit="Degrees", StorageUnit="Radians");
	UPROPERTY(Edit, Save, Category="Camera", DisplayName="Near Z", Member=CameraState.NearZ, Type=Float, Min=0.01f, Max=100.0f, Speed=0.01f);
	UPROPERTY(Edit, Save, Category="Camera", DisplayName="Far Z", Member=CameraState.FarZ, Type=Float, Min=1.0f, Max=100000.0f, Speed=10.0f);
	UPROPERTY(Edit, Save, Category="Camera", DisplayName="Orthographic", Member=CameraState.bIsOrthogonal, Type=Bool);
	UPROPERTY(Edit, Save, Category="Camera", DisplayName="Ortho Width", Member=CameraState.OrthoWidth, Type=Float, Min=0.1f, Max=1000.0f, Speed=0.5f);
	FCameraState CameraState;

	UPROPERTY(Edit, Save, Category="Depth Of Field", DisplayName="Enable DOF", Member=DepthOfField.bEnabled, Type=Bool);
	UPROPERTY(Edit, Save, Category="Depth Of Field", DisplayName="Focus Distance", Member=DepthOfField.FocusDistance, Type=Float, Min=0.01f, Max=100000.0f, Speed=0.1f);
	UPROPERTY(Edit, Save, Category="Depth Of Field", DisplayName="F-Stop", Member=DepthOfField.FStop, Type=Float, Min=0.1f, Max=32.0f, Speed=0.05f);
	UPROPERTY(Edit, Save, Category="Depth Of Field", DisplayName="Sensor Width", Member=DepthOfField.SensorWidth, Type=Float, Min=0.1f, Max=1000.0f, Speed=0.1f);
	UPROPERTY(Edit, Save, Category="Depth Of Field", DisplayName="Gather Rings", Member=DepthOfField.GatherRingCount, Type=Int, Min=1, Max=10, Speed=1.0f);
	UPROPERTY(Edit, Save, Category="Depth Of Field", DisplayName="Samples Per Ring", Member=DepthOfField.GatherSamplesPerRing, Type=Int, Min=4, Max=32, Speed=1.0f);
	UPROPERTY(Edit, Save, Category="Depth Of Field", DisplayName="Foreground", Member=DepthOfField.bEnableForeground, Type=Bool);
	UPROPERTY(Edit, Save, Category="Depth Of Field", DisplayName="Background", Member=DepthOfField.bEnableBackground, Type=Bool);
	UPROPERTY(Edit, Save, Category="Depth Of Field", DisplayName="Half Res", Member=DepthOfField.bHalfRes, Type=Bool);
	FCameraDepthOfFieldSettings DepthOfField;
};
