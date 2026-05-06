#pragma once

class FGameTimeManager
{
public:
	void Tick(float InUnScaledDeltatime);

	void RequestHitStop(float Duration);
	void RequestSlomo(float InTimeScale, float Duration);

	float GetDeltatime() const { return Deltatime; }
	float GetUnScaledDeltatime() const { return UnScaledDeltatime; }
	float GetTimeScale() const { return TimeScale; }

private:
	float Deltatime = 0.0f;
	float UnScaledDeltatime = 0.0f;
	float TimeScale = 1.0f;

	float HitStopRemainingTime = 0.0f;
	float SlomoRemainingTime = 0.0f;
	float SlomoTimeScale = 1.0f;
};
