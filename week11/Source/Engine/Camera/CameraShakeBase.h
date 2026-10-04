#pragma once
#include "Engine/Object/Object.h"
#include "Engine/Component/CameraComponent.h"
#include <algorithm>

#include "CameraShakeBase.generated.h"

class APlayerCameraManager;
class UCameraShakePattern;

struct FCameraShakeStartParams
{
	bool bIsRestarting = false;
	bool bOverrideDuration = false;
    float DurationOverride = 0.0f;
};

struct FCameraShakeUpdateParams
{
    float DeltaTime = 0.f;

    float ShakeScale = 1.f;
    float DynamicScale = 1.f;

    FMinimalViewInfo POV;
};

struct FCameraShakeUpdateResult
{
    FVector LocationOffset = FVector::ZeroVector;
    FRotator RotationOffset = FRotator::ZeroRotator;
    float FOVOffset = 0.0f;
};

struct FCameraShakeInfo
{
	float Duration = 0.0f;
	float BlendInTime = 0.0f;
	float BlendOutTime = 0.0f;
};

struct FCameraShakeState
{
    FCameraShakeInfo ShakeInfo;
    float ElapsedTime = 0.0f;

    float CurrentBlendInTime = 0.0f;
    float CurrentBlendOutTime = 0.0f;

    bool bIsActive = false;
    bool bIsFinished = true;

	bool bIsBlendingIn = false;
	bool bIsBlendingOut = false;

    float CurrentBlendWeight = 1.0f;

    bool IsFinished() const { return bIsFinished; }

	void Start(const UCameraShakePattern* Pattern, const FCameraShakeStartParams& Params);

    void Stop(bool bImmediately);

	void Update(float DeltaTime);

    float GetBlendWeight() const {return CurrentBlendWeight;}
};

UCLASS()
class UCameraShakePattern : public UObject
{
    GENERATED_BODY_UCameraShakePattern()
public:
    DECLARE_CLASS(UCameraShakePattern, UObject)

    void StartShakePattern(const FCameraShakeStartParams& Params);
    void UpdateShakePattern(
        const FCameraShakeUpdateParams& Params,
        FCameraShakeUpdateResult& OutResult);
    void StopShakePattern();
    void StopShakePattern(const bool bImmediately);

	bool IsFinished() const { return state.IsFinished(); }

    virtual void GetCameraShakeInfo(FCameraShakeInfo& OutCameraInfo) const;
	
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    /** Gets the shake pattern's parent shake */
    template <typename InstanceType>
    InstanceType* GetShakeInstance() const { return Cast<InstanceType>(GetShakeInstance()); }

private:
    // UCameraShakePattern interface
    virtual void OnStartShakePattern(const FCameraShakeStartParams& Params) {}
    virtual void OnStopShakePattern(bool bImmediately) {}
    virtual void OnUpdateShakePattern(
        const FCameraShakeUpdateParams& Params,
        FCameraShakeUpdateResult& OutResult) {}

public:
    UPROPERTY(EditAnywhere)
    float Duration = 1.0f;
    UPROPERTY(EditAnywhere)
    float BlendInTime = 0.2f;
    UPROPERTY(EditAnywhere)
    float BlendOutTime = 0.2f;

protected:
    FCameraShakeState state;
};

// 나중에 다른곳으로 뺄것
UCLASS()
class UPerlinCameraShakePattern : public UCameraShakePattern
{
    GENERATED_BODY_UPerlinCameraShakePattern()
public:
    DECLARE_CLASS(UPerlinCameraShakePattern, UCameraShakePattern)

    UPROPERTY(EditAnywhere)
	float LocationAmplitude = 20.0f;
    UPROPERTY(EditAnywhere)
    float RotationAmplitude = 5.0f;
    UPROPERTY(EditAnywhere)
    float FOVAmplitude = 3.0f;

    float Frequency = 10.0f;

private:
    virtual void OnUpdateShakePattern(
        const FCameraShakeUpdateParams& Params,
        FCameraShakeUpdateResult& OutResult);
};

UCLASS()
class UCameraShakeBase : public UObject
{
    GENERATED_BODY_UCameraShakeBase()
public:
    DECLARE_CLASS(UCameraShakeBase, UObject)

	UCameraShakeBase();

    // Lifecycle
    void StartShake(APlayerCameraManager* Camera, float Scale, float DurationOverride);
    void UpdateAndApplyCameraShake(float DeltaTime, float Alpha, FMinimalViewInfo& InOutPOV);
    void StopShake(bool bImmediately = false);

	void ApplyResult(
        const FCameraShakeUpdateResult& InResult,
        FMinimalViewInfo& InOutPOV);

	APlayerCameraManager* GetCameraManager() const { return PlayerCameraManager; }
    UCameraShakePattern* GetRootShakePattern() const { return RootShakePattern; }
    bool IsFinished() const { return !bIsActive || !RootShakePattern || RootShakePattern->IsFinished(); }
    void SetRootShakePattern(UCameraShakePattern* InPattern) { 
		if (!bIsActive) RootShakePattern = InPattern;  //실행중이면  변경불가
	}

private:
	APlayerCameraManager* PlayerCameraManager;
    UCameraShakePattern* RootShakePattern;

    bool bIsActive;
    float ShakeScale;
};
