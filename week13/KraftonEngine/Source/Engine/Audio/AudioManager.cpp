#include "AudioManager.h"
#include "Core/Log.h"
#include "Platform/Paths.h"

bool FAudioManager::Initialize()
{
	if (FMOD::System_Create(&System) != FMOD_OK || !System)
	{
		UE_LOG("Failed to create FMOD system.");
		return false;
	}

	if (System->init(512, FMOD_INIT_NORMAL, nullptr) != FMOD_OK)
	{
		UE_LOG("Failed to initialize FMOD system.");
		Shutdown();
		return false;
	}

	System->getMasterChannelGroup(&MasterGroup);
	return true;
}

void FAudioManager::Shutdown()
{
	for (auto& Pair : Audios)
	{
		if (Pair.second)
		{
			Pair.second->release();
		}
	}
	Audios.clear();
	
	if (System)
	{
		System->close();
		System->release();
		System = nullptr;
	}

	MasterGroup = nullptr;
	BGMChannel = nullptr;
}

void FAudioManager::Tick()
{
	if (System)
	{
		System->update();
	}
}

bool FAudioManager::LoadAudio(const FString& Key, const FString& Path, bool bLoop)
{
	if (!System)
	{
		return false;
	}

	FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::AudioDir(), FPaths::ToWide(Path)));

	FMOD::Sound* Sound = nullptr;
	const FMOD_MODE Mode = FMOD_DEFAULT | (bLoop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

	if (System->createSound(FullPath.c_str(), Mode, nullptr, &Sound) != FMOD_OK)
	{
		return false;
	}

	Audios[Key] = Sound;
	return true;
}

void FAudioManager::PlayAudio(const FString& Key, float Volume)
{
	if (!System || !Audios.contains(Key))
	{
		return;
	}

	FMOD::Channel* Channel = nullptr;
	System->playSound(Audios[Key], nullptr, false, &Channel);

	if (Channel)
	{
		Channel->setVolume(Volume);
	}
}

void FAudioManager::PlayBGM(const FString& Key, float Volume)
{
	if (!System || !Audios.contains(Key))
	{
		return;
	}

	StopBGM();
	System->playSound(Audios[Key], nullptr, false, &BGMChannel);

	if (BGMChannel)
	{
		BGMChannel->setVolume(Volume);
	}
}

void FAudioManager::StopBGM()
{
	if (BGMChannel)
	{
		BGMChannel->stop();
		BGMChannel = nullptr;
	}
}

void FAudioManager::SetMasterVolume(float Volume)
{
	if (MasterGroup)
	{
		MasterGroup->setVolume(Volume);
	}
}
