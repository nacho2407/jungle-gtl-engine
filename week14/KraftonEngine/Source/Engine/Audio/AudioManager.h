#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include <memory>

class FAudioManager : public TSingleton<FAudioManager>
{
	friend class TSingleton<FAudioManager>;

public:
	bool Initialize();
	void Shutdown();
	void Tick();

	bool LoadAudio(const FString& Key, const FString& Path, bool bLoop = false);
	bool PlayAudio(const FString& Key, float Volume = 1.0f);
	/**
	 * @brief 로드된 일반 오디오 재생을 즉시 멈추고 시작 위치로 되돌립니다
	 *
	 * @param Key 정지할 로드 오디오 키
	 *
	 * @return 정지할 오디오를 찾았으면 true, 없거나 오디오 시스템이 준비되지 않았으면 false
	 */
	bool StopAudio(const FString& Key);
	bool PlayOneShot(const FString& EventName);
	bool PlayOneShotAt(const FString& EventName, const FVector& Position);
	void PlayBGM(const FString& Key, float Volume = 1.0f);
	void StopBGM();
	bool PlayLoop(const FString& Key, const FString& LoopName, float Volume = 1.0f, float Pitch = 1.0f);
	bool PlayLoopAt(const FString& Key, const FString& LoopName, const FVector& Position, float Volume = 1.0f, float Pitch = 1.0f, float MinDistance = 4.0f, float MaxDistance = 120.0f, float Rolloff = 1.0f);
	void StopLoop(const FString& LoopName);
	void FadeOutLoop(const FString& LoopName, float FadeMilliseconds = 200.0f);
	void StopAllLoops();
	bool SetLoopState(const FString& LoopName, const FString& Key, bool bShouldPlay, float Volume = 1.0f, float Pitch = 1.0f);
	void SetLoopVolume(const FString& LoopName, float Volume);
	void SetLoopPitch(const FString& LoopName, float Pitch);
	void SetLoopPosition(const FString& LoopName, const FVector& Position);
	bool IsLoopPlaying(const FString& LoopName);

	void SetMasterVolume(float Volume);
	void SetBusVolume(const FString& BusName, float Volume);
	float GetBusVolume(const FString& BusName) const;
	bool PlayEvent(const FString& EventName);
	bool PlayEventAt(const FString& EventName, const FVector& Position);
	void StopEvent(const FString& EventName);
	void FadeOutEvent(const FString& EventName, float FadeMilliseconds = 120.0f);
	void SetListenerTransform(const FVector& Position, const FVector& Forward, const FVector& Up);

private:
	void LoadDefaultAudios();
	void LoadSoundEvents();
	void UpdateListenerFromWorld();
	void CleanupTransientSounds();

private:
	FAudioManager();
	~FAudioManager();

	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
