#include "USoundManager.hpp"

void USoundManager::Initialize()
{
	FMOD::System_Create(&System);
	System->init(512, FMOD_INIT_NORMAL, nullptr);
	System->createChannelGroup("BGM", &BGMGroup);
	System->createChannelGroup("SFX", &SFXGroup);
}

void USoundManager::Release()
{
	for (auto MyPair : Sounds)
	{
		MyPair.second->release();
	}

	if (BGMGroup) BGMGroup->release();
	if (SFXGroup) SFXGroup->release();
	System->close();
	System->release();
}

void USoundManager::LoadSound(std::wstring Path, bool bIsLoop)
{
	FMOD_MODE Mode = (bIsLoop) ? (FMOD_LOOP_NORMAL) : (FMOD_DEFAULT);

	if (Sounds.find(Path) == Sounds.end())
	{
		System->createSound(std::string(Path.begin(), Path.end()).c_str(), Mode, nullptr, &Sounds[Path]);
	}
}

void USoundManager::PlayBGM(std::wstring Path, float Volume)
{
	if (Sounds.find(Path) == Sounds.end()) return;

	if (BGMChannel) BGMChannel->stop();

	System->playSound(Sounds[Path], BGMGroup, false, &BGMChannel);
	if (BGMChannel) BGMChannel->setVolume(Volume);
}

void USoundManager::PlaySFX(std::wstring Path, float Volume)
{
	if (Sounds.find(Path) == Sounds.end()) return;

	FMOD::Channel* Ch = nullptr;
	System->playSound(Sounds[Path], SFXGroup, false, &Ch);
	if (Ch) Ch->setVolume(Volume);
}

void USoundManager::StopAll()
{
	if (BGMGroup) BGMGroup->stop();
	if (SFXGroup) SFXGroup->stop();
}

void USoundManager::Update()
{
	System->update();
}
