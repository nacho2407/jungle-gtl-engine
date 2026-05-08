#include "AudioManager.h"
#include "Core/Log.h"
#include "Platform/Paths.h"
#include <algorithm>

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

	LoadDefaultAudios();

	return true;
}

void FAudioManager::Shutdown()
{
	if (MasterGroup)
	{
		MasterGroup->stop();
	}
	BGMChannel = nullptr;
	LoopChannels.clear();

	if (System)
	{
		System->update();
	}

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
		System->update();
		System->close();
		System->release();
		System = nullptr;
	}
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

	if (Audios.contains(Key) && Audios[Key])
	{
		Audios[Key]->release();
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

void FAudioManager::PlayLoop(const FString& Key, const FString& LoopName, float Volume, float Pitch)
{
	if (!System || !Audios.contains(Key) || LoopName.empty())
	{
		return;
	}

	if (FMOD::Channel* ExistingChannel = FindPlayingLoopChannel(LoopName))
	{
		ExistingChannel->setVolume(std::clamp(Volume, 0.0f, 1.0f));
		ExistingChannel->setPitch(std::clamp(Pitch, 0.1f, 3.0f));
		return;
	}

	FMOD::Channel* Channel = nullptr;
	System->playSound(Audios[Key], nullptr, false, &Channel);

	if (Channel)
	{
		Channel->setMode(FMOD_LOOP_NORMAL);
		Channel->setVolume(std::clamp(Volume, 0.0f, 1.0f));
		Channel->setPitch(std::clamp(Pitch, 0.1f, 3.0f));
		LoopChannels[LoopName] = Channel;
	}
}

void FAudioManager::StopLoop(const FString& LoopName)
{
	if (!LoopChannels.contains(LoopName))
	{
		return;
	}

	if (LoopChannels[LoopName])
	{
		LoopChannels[LoopName]->stop();
	}
	LoopChannels.erase(LoopName);
}

void FAudioManager::StopAllLoops()
{
	for (auto& Pair : LoopChannels)
	{
		if (Pair.second)
		{
			Pair.second->stop();
		}
	}
	LoopChannels.clear();
}

void FAudioManager::SetLoopVolume(const FString& LoopName, float Volume)
{
	if (FMOD::Channel* Channel = FindPlayingLoopChannel(LoopName))
	{
		Channel->setVolume(std::clamp(Volume, 0.0f, 1.0f));
	}
}

void FAudioManager::SetLoopPitch(const FString& LoopName, float Pitch)
{
	if (FMOD::Channel* Channel = FindPlayingLoopChannel(LoopName))
	{
		Channel->setPitch(std::clamp(Pitch, 0.1f, 3.0f));
	}
}

bool FAudioManager::IsLoopPlaying(const FString& LoopName)
{
	return FindPlayingLoopChannel(LoopName) != nullptr;
}

FMOD::Channel* FAudioManager::FindPlayingLoopChannel(const FString& LoopName)
{
	if (!LoopChannels.contains(LoopName))
	{
		return nullptr;
	}

	FMOD::Channel* Channel = LoopChannels[LoopName];
	bool bIsPlaying = false;
	if (!Channel || Channel->isPlaying(&bIsPlaying) != FMOD_OK || !bIsPlaying)
	{
		LoopChannels.erase(LoopName);
		return nullptr;
	}

	return Channel;
}

void FAudioManager::SetMasterVolume(float Volume)
{
	if (MasterGroup)
	{
		MasterGroup->setVolume(Volume);
	}
}

void FAudioManager::LoadDefaultAudios()
{
	LoadAudio("CityBgm", "city_bgm.mp3", true);
	LoadAudio("Phase_EscapePolice", "phase_escapepolice.wav", true);
	LoadAudio("Phase_Meteor", "phase_meteor.mp3", true);
	LoadAudio("Click", "pop.mp3");
	LoadAudio("CarEngineLoop", "car_engine_loop.mp3", true);
	LoadAudio("Notify", "notify.mp3");
	LoadAudio("Complete", "complete.mp3");
	LoadAudio("Crash", "crash.mp3");
	LoadAudio("Water", "water.mp3", true);
	LoadAudio("Siren", "siren.mp3", true);
	LoadAudio("Fueling", "fueling.mp3", true);
	LoadAudio("ScoreUp", "score_up.mp3");
	LoadAudio("MeteorBoom", "meteor_boom.mp3");
	LoadAudio("MeteorFall", "meteor_fall.mp3");
	LoadAudio("Whoosh", "whoosh.mp3");
}
