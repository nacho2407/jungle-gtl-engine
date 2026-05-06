#pragma once

#include "Component/SceneComponent.h"

class AActor;
class UCameraComponent;

class UCameraBoomComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UCameraBoomComponent, USceneComponent)

    UCameraBoomComponent() = default;

    void BeginPlay() override;
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

    AActor* GetTargetActor() const { return TargetActor; }
    const FString& GetTargetActorName() const { return TargetActorName; }

    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    void SetTargetActor(AActor* InTargetActor);

private:
    void UpdateCamera(float DeltaTime);
    void ResolveTargetActor();

    UCameraComponent* FindChildCamera() const;

	/*
	 * note: Spring Arm은 보통 Target Actor의 정확한 바닥 위치가 아니라, 캐릭터 몸통이나 머리 근처를 기준으로 카메라를 따라가게 하기 때문에,
	 *       Player의 위치가 발밑 기준 등이라면 TargetOffset을 통해 조정
	 */
    FVector GetTargetPivotLocation() const;
    FVector ComputeDesiredCameraLocation(const FVector& PivotLocation) const;
    FVector ApplyCameraLag(const FVector& DesiredCameraLocation, float DeltaTime);
    FVector ApplyCollisionTest(const FVector& PivotLocation, const FVector& DesiredCameraLocation) const;

private:
    AActor* TargetActor = nullptr;
    FString TargetActorName = "None";

    // Pivot에서 카메라까지의 거리
    float TargetDistance = 100.0f;

    // Pivot에서 카메라가 놓일 방향입니다.
    // FVector(25, 20, 50)를 정규화하면 쿼터뷰 카메라 방향
    FVector OffsetDirection = FVector(25.0f, 20.0f, 50.0f);

    // Target Actor 위치에서 Pivot을 살짝 올리거나 옮기고 싶을 때 사용
    FVector TargetOffset = FVector(0.0f, 0.0f, 0.0f);

    // 최종 카메라 위치를 조금 더 보정하고 싶을 때 사용
    FVector SocketOffset = FVector(0.0f, 0.0f, 0.0f);

    // 에디터 상태에서도 Spring Arm 미리보기를 갱신할지 여부
    bool bUpdateInEditor = true;

    // 카메라가 즉시 따라가지 않고 부드럽게 따라가도록 하는 옵션
    bool bEnableCameraLag = true;
    float CameraLagSpeed = 10.0f;

    // Target과 Camera 사이의 벽을 감지해서 카메라를 앞으로 당기는 옵션
    bool bDoCollisionTest = false;
    float ProbeSize = 0.3f;

    // 마우스 Orbit 옵션
    bool bEnableMouseOrbit = true;
    bool bRequireRightMouseForOrbit = true;
    float MouseOrbitSensitivity = 0.3f;
    float MinPitchDegrees = -80.0f;
    float MaxPitchDegrees = 80.0f;

    FVector LastAppliedCameraLocationByBoom = FVector(0.0f, 0.0f, 0.0f);
    bool bHasLastAppliedCameraLocation = false;
};