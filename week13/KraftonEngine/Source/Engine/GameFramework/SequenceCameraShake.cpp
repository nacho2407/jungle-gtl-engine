#include "SequenceCameraShake.h"

#include "CameraShake/CameraShakeAsset.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/PlayerCameraManager.h"
#include "FloatCurve/FloatCurveManager.h"
#include "FloatCurve/FloatCurveAsset.h"

IMPLEMENT_CLASS(USequenceCameraShake, UCameraShakeBase)

namespace
{
	UFloatCurveAsset* LoadCurveOrNull(const FString& Path)
	{
		return Path.empty() ? nullptr : FFloatCurveManager::Get().Load(Path);
	}
}

void USequenceCameraShake::StartShake(
	APlayerCameraManager* Camera,
	float InScale,
	ECameraShakePlaySpace InPlaySpace,
	FRotator InUserPlaySpaceRot)
{
	Super::StartShake(Camera, InScale, InPlaySpace, InUserPlaySpaceRot);

	ElapsedTime = 0.0f;
}

void USequenceCameraShake::ApplyAsset(const UCameraShakeAsset* ShakeAsset)
{
	if (!ShakeAsset)
	{
		return;
	}

	Duration = ShakeAsset->Duration;
	BlendInTime = ShakeAsset->BlendInTime;
	BlendOutTime = ShakeAsset->BlendOutTime;
	bSingleInstance = ShakeAsset->bSingleInstance;

	LocXCurve = LoadCurveOrNull(ShakeAsset->Sequence.LocXCurvePath);
	LocYCurve = LoadCurveOrNull(ShakeAsset->Sequence.LocYCurvePath);
	LocZCurve = LoadCurveOrNull(ShakeAsset->Sequence.LocZCurvePath);

	PitchCurve = LoadCurveOrNull(ShakeAsset->Sequence.PitchCurvePath);
	YawCurve = LoadCurveOrNull(ShakeAsset->Sequence.YawCurvePath);
	RollCurve = LoadCurveOrNull(ShakeAsset->Sequence.RollCurvePath);

	FOVCurve = LoadCurveOrNull(ShakeAsset->Sequence.FOVCurvePath);
}

void USequenceCameraShake::UpdateAndApplyCameraShake(float DeltaTime, FCameraShakeUpdateResult& OutResult)
{
	if (bFinished || Duration <= 0.0f) return;

	ElapsedTime += DeltaTime;

	if (ElapsedTime >= Duration)
	{
		bFinished = true;
		return;
	}

	const float Gain = Scale;
	const float T = ElapsedTime;

	auto Eval = [T](UFloatCurveAsset* Curve) -> float
	{
		return Curve ? Curve->GetCurve().Evaluate(T) : 0.0f;
	};

	OutResult.Location.X += Eval(LocXCurve) * Gain;
	OutResult.Location.Y += Eval(LocYCurve) * Gain;
	OutResult.Location.Z += Eval(LocZCurve) * Gain;

	OutResult.Rotation.Pitch += Eval(PitchCurve) * Gain;
	OutResult.Rotation.Yaw += Eval(YawCurve) * Gain;
	OutResult.Rotation.Roll += Eval(RollCurve) * Gain;

	OutResult.FOV += Eval(FOVCurve) * Gain;
}

void USequenceCameraShake::StopShake(bool bImmediately)
{
	if (bImmediately)
	{
		bFinished = true;
		return;
	}

	if (BlendOutTime > 0.0f && ElapsedTime + BlendOutTime < Duration)
	{
		Duration = ElapsedTime + BlendOutTime;
	}
	else
	{
		bFinished = true;
	}
}
