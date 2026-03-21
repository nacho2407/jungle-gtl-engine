#include "Engine/Core/Timer.h"

void FTimer::Initialize()
{
	LastTime = Clock::now();
}

void FTimer::Tick()
{
	if (TargetFrameTime > 0.0f)
	{
		Clock::time_point Now;
		float Elapsed;
		do
		{
			Now = Clock::now();
			Elapsed = std::chrono::duration<float>(Now - LastTime).count();
		} while (Elapsed < TargetFrameTime);
	}

	Clock::time_point CurrentTime = Clock::now();
	DeltaTime = std::chrono::duration<float>(CurrentTime - LastTime).count();
	TotalTime += DeltaTime;
	LastTime = CurrentTime;

	// EMA로 FPS 보간 (UE 방식)
	const float CurrentFPS = DeltaTime > 0.0f ? 1.0f / DeltaTime : 0.0f;
	constexpr float Smoothing = 0.2f; // 낮을수록 더 부드러움
	SmoothedFPS = (SmoothedFPS == 0.0f)
		? CurrentFPS
		: SmoothedFPS + (CurrentFPS - SmoothedFPS) * Smoothing;
}

void FTimer::SetMaxFPS(float InMaxFPS)
{
	MaxFPS = InMaxFPS;
	TargetFrameTime = MaxFPS > 0.0f ? 1.0f / MaxFPS : 0.0f;
}
