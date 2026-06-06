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
	void PlayAudio(const FString& Key, float Volume = 1.0f);
	void PlayBGM(const FString& Key, float Volume = 1.0f);
	void StopBGM();
	void PlayLoop(const FString& Key, const FString& LoopName, float Volume = 1.0f, float Pitch = 1.0f);
	void StopLoop(const FString& LoopName);
	void StopAllLoops();
	void SetLoopVolume(const FString& LoopName, float Volume);
	void SetLoopPitch(const FString& LoopName, float Pitch);
	bool IsLoopPlaying(const FString& LoopName);

	void SetMasterVolume(float Volume);
	void SetBusVolume(const FString& BusName, float Volume);
	float GetBusVolume(const FString& BusName) const;
	bool PlayEvent(const FString& EventName);
	bool PlayEventAt(const FString& EventName, const FVector& Position);
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
