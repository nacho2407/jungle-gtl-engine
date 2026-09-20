#pragma once

#include <string>
#include <unordered_map>

#include <fmod.hpp>

class USoundManager
{
public:
	USoundManager() = default;
	~USoundManager() = default;

	void Initialize();

	void Release();

	void LoadSound(std::wstring Path, bool bIsLoop);

	// BGM: 하나만 재생 (이전 BGM 정지 후 교체)
	void PlayBGM(std::wstring Path, float Volume = 1.f);

	void PlaySFX(std::wstring Path, float Volume = 1.f);

	void StopAll();

	void Update();

private:
	FMOD::System* System = nullptr;
	FMOD::ChannelGroup* BGMGroup = nullptr;
	FMOD::ChannelGroup* SFXGroup = nullptr;
	FMOD::Channel* BGMChannel = nullptr;

	std::unordered_map<std::wstring, FMOD::Sound*> Sounds;
};
