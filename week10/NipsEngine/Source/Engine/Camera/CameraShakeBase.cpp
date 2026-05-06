#include "Camera/CameraShakeBase.h"

DEFINE_CLASS(UPerlinCameraShakePattern, UCameraShakePattern)
DEFINE_CLASS(UCameraShakePattern, UObject)
DEFINE_CLASS(UCameraShakeBase, UObject)

REGISTER_FACTORY(UPerlinCameraShakePattern)
REGISTER_FACTORY(UCameraShakePattern)

void FCameraShakeState::Start(const UCameraShakePattern* Pattern, const FCameraShakeStartParams& Params)
{
    ElapsedTime = 0.0f;
    CurrentBlendInTime = 0.0f;
    CurrentBlendOutTime = 0.0f;

    bIsActive = true;
    bIsFinished = false;

    bIsBlendingIn = false;
    bIsBlendingOut = false;
    CurrentBlendWeight = 1.0f;

    if (Pattern)
    {
        Pattern->GetCameraShakeInfo(ShakeInfo);
    }

    if (Params.bOverrideDuration)
    {
        ShakeInfo.Duration = Params.DurationOverride;
    }

    if (ShakeInfo.BlendInTime > 0.0f)
    {
        bIsBlendingIn = true;
        CurrentBlendWeight = 0.0f;
    }
}

void FCameraShakeState::Stop(bool bImmediately)
{
    if (bImmediately)
    {
        bIsActive = false;
        bIsFinished = true;
        bIsBlendingIn = false;
        bIsBlendingOut = false;
    }
    else if (ShakeInfo.BlendOutTime <= 0.0f)
    {
        bIsActive = false;
        bIsFinished = true;
        bIsBlendingIn = false;
        bIsBlendingOut = false;
        CurrentBlendWeight = 0.0f;
    }
    else if (!bIsBlendingOut)
    {
        bIsBlendingIn = false;
        bIsBlendingOut = true;
        CurrentBlendOutTime = 0.f;
    }
}

void FCameraShakeState::Update(float DeltaTime)

{
    if (!bIsActive || bIsFinished)
        return;

    CurrentBlendWeight = 1.0f;

    ElapsedTime += DeltaTime;

    if (ShakeInfo.Duration > 0.0f && ElapsedTime >= ShakeInfo.Duration)
    {
        Stop(false);
    }

    if (bIsBlendingIn)
    {
        CurrentBlendInTime += DeltaTime;
    }
    if (bIsBlendingOut)
    {
        CurrentBlendOutTime += DeltaTime;
    }

    if (bIsBlendingIn)
    {
        if (CurrentBlendInTime < ShakeInfo.BlendInTime)
        {
            CurrentBlendWeight *= (CurrentBlendInTime / ShakeInfo.BlendInTime);
        }
        else
        {
            // Finished blending in!
            bIsBlendingIn = false;
            CurrentBlendInTime = ShakeInfo.BlendInTime;
        }
    }
    if (bIsBlendingOut)
    {
        if (CurrentBlendOutTime < ShakeInfo.BlendOutTime)
        {
            CurrentBlendWeight *= (1.f - CurrentBlendOutTime / ShakeInfo.BlendOutTime);
        }
        else
        {
            // Finished blending out!
            bIsBlendingOut = false;
            CurrentBlendOutTime = ShakeInfo.BlendOutTime;
            // We also end the shake itself. In most cases we would have hit the similar case
            // above already, but if we have an infinite shake we have no duration to reach the end
            // of so we only finish here.
            bIsActive = false;
            bIsFinished = true;
            CurrentBlendWeight = 0.0f;
            return;
        }
    }
}



void UCameraShakePattern::StartShakePattern(const FCameraShakeStartParams& Params)
{
    state.Start(this, Params);
    OnStartShakePattern(Params);
}

void UCameraShakePattern::UpdateShakePattern(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& OutResult)
{
    state.Update(Params.DeltaTime);

    if (state.IsFinished())
        return;

    FCameraShakeUpdateResult RawResult;
    OnUpdateShakePattern(Params, RawResult);

    const float Weight =
        state.GetBlendWeight() *
        Params.ShakeScale *
        Params.DynamicScale;

    OutResult.LocationOffset += RawResult.LocationOffset * Weight;
    OutResult.RotationOffset += RawResult.RotationOffset * Weight;
    OutResult.FOVOffset += RawResult.FOVOffset * Weight;
}
void UCameraShakePattern::StopShakePattern()
{
    StopShakePattern(false);
}

void UCameraShakePattern::StopShakePattern(const bool bImmediately)
{
    state.Stop(bImmediately);
    OnStopShakePattern(bImmediately);
}

//---------UPerlinCameraShakePattern---------

void UPerlinCameraShakePattern::OnUpdateShakePattern(
    const FCameraShakeUpdateParams& Params,
    FCameraShakeUpdateResult& OutResult)
{
    const float T = state.ElapsedTime;

    const float A = sinf(T * Frequency);
    const float B = sinf(T * Frequency * 1.7f);
    const float C = sinf(T * Frequency * 2.3f);

    // 위치 흔들림
    OutResult.LocationOffset.X = A * LocationAmplitude;
    OutResult.LocationOffset.Y = B * LocationAmplitude * 0.5f;
    OutResult.LocationOffset.Z = C * LocationAmplitude * 0.25f;

    // 회전 흔들림
    OutResult.RotationOffset.Pitch = A * RotationAmplitude;
    OutResult.RotationOffset.Yaw = B * RotationAmplitude;
    OutResult.RotationOffset.Roll = C * RotationAmplitude * 0.5f;

    // FOV 흔들림
    OutResult.FOVOffset = A * FOVAmplitude;
}

UCameraShakeBase::UCameraShakeBase()
    : PlayerCameraManager(nullptr)
    , RootShakePattern(nullptr)
    , bIsActive(false)
    , ShakeScale(1.0f)
{
}

void UCameraShakeBase::StartShake(APlayerCameraManager* Camera, float Scale, float DurationOverride)
{
    if (!RootShakePattern)
    {
        bIsActive = false;
        return;
    }

    const bool bIsRestarting = bIsActive;
    PlayerCameraManager = Camera;
    bIsActive = true;
    ShakeScale = Scale;

    FCameraShakeStartParams StartParams;
    StartParams.bIsRestarting = bIsRestarting;
    StartParams.DurationOverride = DurationOverride;
    StartParams.bOverrideDuration = DurationOverride > 0.0f;
    RootShakePattern->StartShakePattern(StartParams);
}

void UCameraShakeBase::UpdateAndApplyCameraShake(float DeltaTime, float Alpha, FMinimalViewInfo& InOutPOV)
{
    if (bIsActive && RootShakePattern)
    {
		FCameraShakeUpdateParams Params;
        Params.POV = InOutPOV;
		Params.DeltaTime = DeltaTime;
        Params.ShakeScale = ShakeScale;
		Params.DynamicScale = Alpha;

		FCameraShakeUpdateResult Result;
        RootShakePattern->UpdateShakePattern(Params, Result);

		if (RootShakePattern->IsFinished())
        {
            bIsActive = false;
            return;
        }

        ApplyResult(Result, InOutPOV);
    }
}

void UCameraShakeBase::StopShake(bool bImmediately)
{
    if (!RootShakePattern)
    {
        bIsActive = false;
        return;
    }

    RootShakePattern->StopShakePattern(bImmediately);
	bIsActive = !RootShakePattern->IsFinished();
}

void UCameraShakeBase::ApplyResult(const FCameraShakeUpdateResult& InResult, FMinimalViewInfo& InOutPOV)
{
    InOutPOV.Location += InResult.LocationOffset;
    InOutPOV.Rotation += InResult.RotationOffset.Quaternion();
    InOutPOV.FOV += InResult.FOVOffset;
}
