#include "GameTimeManager.h"

void FGameTimeManager::Tick(float InUnScaledDeltatime)
{
	UnScaledDeltatime = InUnScaledDeltatime;
	Deltatime = InUnScaledDeltatime;
	TimeScale = 1.0f;

	if (HitStopRemainingTime > 0)
	{
		HitStopRemainingTime = HitStopRemainingTime - InUnScaledDeltatime >= 0 ? HitStopRemainingTime - InUnScaledDeltatime : 0;
		TimeScale = 0;
	}

	if (SlomoRemainingTime > 0)
	{
		SlomoRemainingTime = SlomoRemainingTime - InUnScaledDeltatime >= 0 ? SlomoRemainingTime - InUnScaledDeltatime : 0;
		TimeScale = SlomoTimeScale;
	}

	Deltatime = InUnScaledDeltatime * TimeScale;
}

void FGameTimeManager::RequestHitStop(float Duration)
{
	if (Duration <= 0) return;
	HitStopRemainingTime = Duration;
}

void FGameTimeManager::RequestSlomo(float InTimeScale, float Duration)
{
	if (InTimeScale <= 0) return;
	if (Duration <= 0) return;

	SlomoTimeScale = InTimeScale;
	SlomoRemainingTime = Duration;
}