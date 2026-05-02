#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include <fmod.hpp>

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

	void SetMasterVolume(float Volume);

private:
	FAudioManager() = default;
	~FAudioManager() = default;

	FMOD::System* System = nullptr;
	FMOD::ChannelGroup* MasterGroup = nullptr;
	FMOD::Channel* BGMChannel = nullptr;

	TMap<FString, FMOD::Sound*> Audios;
};