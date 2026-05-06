#include "GameTimeManager.h"

void FGameTimeManager::Tick(float InUnScaledDeltatime)
{
	if (HitStopRemainingTime > 0)
	{
		HitStopRemainingTime = HitStopRemainingTime - InUnScaledDeltatime >= 0 ? HitStopRemainingTime - InUnScaledDeltatime : 0;
		TimeScale = 0;
	}

	if (SlomoRemainingTime > 0)
	{
		SlomoRemainingTime = SlomoRemainingTime - InUnScaledDeltatime >= 0 ? SlomoRemainingTime - InUnScaledDeltatime : 0;
		TimeScale = InUnScaledDeltatime * SlomoTimeScale;
	}

	Deltatime = InUnScaledDeltatime * TimeScale;
}

void FGameTimeManager::RequestHitStop(float Duration)
{
	HitStopRemainingTime = Duration >= 0 ? Duration : 0;
}

void FGameTimeManager::RequestSlomo(float InTimeScale, float Duration)
{
	SlomoRemainingTime = Duration >= 0 ? Duration : 0;
	SlomoTimeScale = InTimeScale >= 0 ? InTimeScale : 0;
}