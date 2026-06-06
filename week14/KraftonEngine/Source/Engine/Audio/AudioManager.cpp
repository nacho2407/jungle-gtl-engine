#include "AudioManager.h"

#include "Core/Logging/Log.h"
#include "GameFramework/World.h"
#include "Platform/Paths.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Runtime/Engine.h"
#include "SimpleJSON/json.hpp"
#include "miniaudio/miniaudio.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <sstream>

namespace
{
	enum class EAudioBus
	{
		Master,
		BGM,
		SFX,
		UI,
		Voice,
	};

	struct FSoundEventDef
	{
		EAudioBus Bus = EAudioBus::SFX;
		bool bSpatial = false;
		TArray<FString> Clips;
		float VolumeMin = 1.0f;
		float VolumeMax = 1.0f;
		float PitchMin = 1.0f;
		float PitchMax = 1.0f;
		float MinDistance = 100.0f;
		float MaxDistance = 3000.0f;
		ma_attenuation_model Attenuation = ma_attenuation_model_inverse;
		float Cooldown = 0.0f;
		int32 MaxInstances = 0;
	};

	struct FLoadedSound
	{
		FString Path;
		bool bLoop = false;
		ma_sound* Sound = nullptr;
	};

	struct FTransientSound
	{
		FString EventName;
		ma_sound* Sound = nullptr;
	};

	float Clamp01(float Value)
	{
		return std::clamp(Value, 0.0f, 1.0f);
	}

	double GetAudioTimeSeconds()
	{
		using Clock = std::chrono::steady_clock;
		static const Clock::time_point StartTime = Clock::now();
		return std::chrono::duration<double>(Clock::now() - StartTime).count();
	}

	bool ReadTextFile(const std::wstring& Path, FString& OutText)
	{
		std::ifstream File(Path, std::ios::binary);
		if (!File.is_open())
		{
			return false;
		}

		std::ostringstream Buffer;
		Buffer << File.rdbuf();
		OutText = Buffer.str();
		return true;
	}

	float ReadFloat(json::JSON& Object, const FString& Key, float DefaultValue)
	{
		if (!Object.hasKey(Key))
		{
			return DefaultValue;
		}

		bool bOk = false;
		double Value = Object[Key].ToFloat(bOk);
		if (!bOk)
		{
			long IntValue = Object[Key].ToInt(bOk);
			Value = static_cast<double>(IntValue);
		}
		return bOk ? static_cast<float>(Value) : DefaultValue;
	}

	int32 ReadInt(json::JSON& Object, const FString& Key, int32 DefaultValue)
	{
		if (!Object.hasKey(Key))
		{
			return DefaultValue;
		}

		bool bOk = false;
		const long Value = Object[Key].ToInt(bOk);
		return bOk ? static_cast<int32>(Value) : DefaultValue;
	}

	bool ReadBool(json::JSON& Object, const FString& Key, bool DefaultValue)
	{
		if (!Object.hasKey(Key))
		{
			return DefaultValue;
		}

		bool bOk = false;
		const bool Value = Object[Key].ToBool(bOk);
		return bOk ? Value : DefaultValue;
	}

	FString ReadString(json::JSON& Object, const FString& Key, const FString& DefaultValue)
	{
		if (!Object.hasKey(Key))
		{
			return DefaultValue;
		}

		bool bOk = false;
		const FString Value = Object[Key].ToString(bOk);
		return bOk ? Value : DefaultValue;
	}

	EAudioBus ParseBus(const FString& BusName)
	{
		if (BusName == "Master") return EAudioBus::Master;
		if (BusName == "BGM") return EAudioBus::BGM;
		if (BusName == "UI") return EAudioBus::UI;
		if (BusName == "Voice") return EAudioBus::Voice;
		return EAudioBus::SFX;
	}

	ma_attenuation_model ParseAttenuation(const FString& Attenuation)
	{
		if (Attenuation == "none") return ma_attenuation_model_none;
		if (Attenuation == "linear") return ma_attenuation_model_linear;
		if (Attenuation == "exponential") return ma_attenuation_model_exponential;
		return ma_attenuation_model_inverse;
	}
}

struct FAudioManager::FImpl
{
	ma_engine Engine = {};
	ma_sound_group MasterGroup = {};
	ma_sound_group BGMGroup = {};
	ma_sound_group SFXGroup = {};
	ma_sound_group UIGroup = {};
	ma_sound_group VoiceGroup = {};

	bool bInitialized = false;
	bool bMasterGroupInitialized = false;
	bool bBGMGroupInitialized = false;
	bool bSFXGroupInitialized = false;
	bool bUIGroupInitialized = false;
	bool bVoiceGroupInitialized = false;

	FLoadedSound BGMPlayback;
	TMap<FString, FLoadedSound> LoadedSounds;
	TMap<FString, FLoadedSound> LoopSounds;
	TMap<FString, FSoundEventDef> SoundEvents;
	TMap<FString, double> LastEventPlayTime;
	TArray<FTransientSound> TransientSounds;

	float MasterVolume = 1.0f;
	float BGMVolume = 1.0f;
	float SFXVolume = 1.0f;
	float UIVolume = 1.0f;
	float VoiceVolume = 1.0f;

	FVector ListenerPosition = FVector::ZeroVector;
	FVector ListenerForward = FVector::ForwardVector;
	FVector ListenerUp = FVector::UpVector;

	std::mt19937 Random{ std::random_device{}() };

	ma_sound_group* GetGroup(EAudioBus Bus)
	{
		switch (Bus)
		{
		case EAudioBus::Master: return &MasterGroup;
		case EAudioBus::BGM: return &BGMGroup;
		case EAudioBus::UI: return &UIGroup;
		case EAudioBus::Voice: return &VoiceGroup;
		case EAudioBus::SFX:
		default: return &SFXGroup;
		}
	}

	float* GetBusVolumePtr(EAudioBus Bus)
	{
		switch (Bus)
		{
		case EAudioBus::Master: return &MasterVolume;
		case EAudioBus::BGM: return &BGMVolume;
		case EAudioBus::UI: return &UIVolume;
		case EAudioBus::Voice: return &VoiceVolume;
		case EAudioBus::SFX:
		default: return &SFXVolume;
		}
	}
};

FAudioManager::FAudioManager()
	: Impl(std::make_unique<FImpl>())
{
}

FAudioManager::~FAudioManager() = default;

bool FAudioManager::Initialize()
{
	if (Impl->bInitialized)
	{
		return true;
	}

	if (ma_engine_init(nullptr, &Impl->Engine) != MA_SUCCESS)
	{
		UE_LOG("Failed to initialize miniaudio engine.");
		return false;
	}

	Impl->bInitialized = true;

	auto InitGroup = [this](ma_sound_group* Group, ma_sound_group* Parent, bool& bFlag, const char* Name) -> bool
	{
		if (ma_sound_group_init(&Impl->Engine, 0, Parent, Group) != MA_SUCCESS)
		{
			UE_LOG("Failed to initialize audio group: %s", Name);
			return false;
		}
		bFlag = true;
		return true;
	};

	if (!InitGroup(&Impl->MasterGroup, nullptr, Impl->bMasterGroupInitialized, "Master") ||
		!InitGroup(&Impl->BGMGroup, &Impl->MasterGroup, Impl->bBGMGroupInitialized, "BGM") ||
		!InitGroup(&Impl->SFXGroup, &Impl->MasterGroup, Impl->bSFXGroupInitialized, "SFX") ||
		!InitGroup(&Impl->UIGroup, &Impl->MasterGroup, Impl->bUIGroupInitialized, "UI") ||
		!InitGroup(&Impl->VoiceGroup, &Impl->MasterGroup, Impl->bVoiceGroupInitialized, "Voice"))
	{
		Shutdown();
		return false;
	}

	ma_sound_group_set_volume(&Impl->MasterGroup, Impl->MasterVolume);
	ma_sound_group_set_volume(&Impl->BGMGroup, Impl->BGMVolume);
	ma_sound_group_set_volume(&Impl->SFXGroup, Impl->SFXVolume);
	ma_sound_group_set_volume(&Impl->UIGroup, Impl->UIVolume);
	ma_sound_group_set_volume(&Impl->VoiceGroup, Impl->VoiceVolume);

	SetListenerTransform(Impl->ListenerPosition, Impl->ListenerForward, Impl->ListenerUp);
	LoadDefaultAudios();
	LoadSoundEvents();
	return true;
}

void FAudioManager::Shutdown()
{
	if (!Impl)
	{
		return;
	}

	StopBGM();
	StopAllLoops();

	for (FTransientSound& Entry : Impl->TransientSounds)
	{
		if (Entry.Sound)
		{
			ma_sound_uninit(Entry.Sound);
			delete Entry.Sound;
		}
	}
	Impl->TransientSounds.clear();

	for (auto& Pair : Impl->LoadedSounds)
	{
		if (Pair.second.Sound)
		{
			ma_sound_uninit(Pair.second.Sound);
			delete Pair.second.Sound;
		}
	}
	Impl->LoadedSounds.clear();
	Impl->BGMPlayback = FLoadedSound();

	if (Impl->bVoiceGroupInitialized) { ma_sound_group_uninit(&Impl->VoiceGroup); Impl->bVoiceGroupInitialized = false; }
	if (Impl->bUIGroupInitialized) { ma_sound_group_uninit(&Impl->UIGroup); Impl->bUIGroupInitialized = false; }
	if (Impl->bSFXGroupInitialized) { ma_sound_group_uninit(&Impl->SFXGroup); Impl->bSFXGroupInitialized = false; }
	if (Impl->bBGMGroupInitialized) { ma_sound_group_uninit(&Impl->BGMGroup); Impl->bBGMGroupInitialized = false; }
	if (Impl->bMasterGroupInitialized) { ma_sound_group_uninit(&Impl->MasterGroup); Impl->bMasterGroupInitialized = false; }

	if (Impl->bInitialized)
	{
		ma_engine_uninit(&Impl->Engine);
		Impl->bInitialized = false;
	}

	Impl->SoundEvents.clear();
	Impl->LastEventPlayTime.clear();
}

void FAudioManager::Tick()
{
	if (!Impl->bInitialized)
	{
		return;
	}

	UpdateListenerFromWorld();
	CleanupTransientSounds();
}

bool FAudioManager::LoadAudio(const FString& Key, const FString& Path, bool bLoop)
{
	if (!Impl->bInitialized || Key.empty() || Path.empty())
	{
		return false;
	}

	const std::wstring FullPath = FPaths::Combine(FPaths::AudioDir(), FPaths::ToWide(Path));
	ma_sound* Sound = new ma_sound();
	const ma_uint32 Flags = MA_SOUND_FLAG_NO_SPATIALIZATION | (bLoop ? MA_SOUND_FLAG_LOOPING : 0);
	if (ma_sound_init_from_file_w(&Impl->Engine, FullPath.c_str(), Flags, &Impl->SFXGroup, nullptr, Sound) != MA_SUCCESS)
	{
		delete Sound;
		return false;
	}

	ma_sound_set_looping(Sound, bLoop ? MA_TRUE : MA_FALSE);

	auto It = Impl->LoadedSounds.find(Key);
	if (It != Impl->LoadedSounds.end() && It->second.Sound)
	{
		ma_sound_uninit(It->second.Sound);
		delete It->second.Sound;
	}

	Impl->LoadedSounds[Key] = FLoadedSound{ Path, bLoop, Sound };
	return true;
}

bool FAudioManager::PlayAudio(const FString& Key, float Volume)
{
	if (!Impl->bInitialized || !Impl->LoadedSounds.contains(Key))
	{
		return false;
	}

	FLoadedSound& Entry = Impl->LoadedSounds[Key];
	if (!Entry.Sound)
	{
		return false;
	}

	ma_sound_stop(Entry.Sound);
	ma_sound_seek_to_pcm_frame(Entry.Sound, 0);
	ma_sound_set_volume(Entry.Sound, std::max(0.0f, Volume));
	ma_sound_set_looping(Entry.Sound, Entry.bLoop ? MA_TRUE : MA_FALSE);
	return ma_sound_start(Entry.Sound) == MA_SUCCESS;
}

void FAudioManager::PlayBGM(const FString& Key, float Volume)
{
	if (!Impl->bInitialized || !Impl->LoadedSounds.contains(Key))
	{
		return;
	}

	StopBGM();
	const FLoadedSound& Entry = Impl->LoadedSounds[Key];
	if (Entry.Path.empty())
	{
		return;
	}

	const std::wstring FullPath = FPaths::Combine(FPaths::AudioDir(), FPaths::ToWide(Entry.Path));
	ma_sound* Sound = new ma_sound();
	if (ma_sound_init_from_file_w(&Impl->Engine, FullPath.c_str(), MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_LOOPING, &Impl->BGMGroup, nullptr, Sound) != MA_SUCCESS)
	{
		delete Sound;
		return;
	}

	ma_sound_set_volume(Sound, std::max(0.0f, Volume));
	ma_sound_set_looping(Sound, MA_TRUE);
	ma_sound_start(Sound);
	Impl->BGMPlayback = FLoadedSound{ Entry.Path, true, Sound };
}

void FAudioManager::StopBGM()
{
	if (Impl->BGMPlayback.Sound)
	{
		ma_sound_stop(Impl->BGMPlayback.Sound);
		ma_sound_uninit(Impl->BGMPlayback.Sound);
		delete Impl->BGMPlayback.Sound;
	}
	Impl->BGMPlayback = FLoadedSound();
}

bool FAudioManager::PlayLoop(const FString& Key, const FString& LoopName, float Volume, float Pitch)
{
	if (!Impl->bInitialized || LoopName.empty() || !Impl->LoadedSounds.contains(Key))
	{
		return false;
	}

	if (Impl->LoopSounds.contains(LoopName))
	{
		SetLoopVolume(LoopName, Volume);
		SetLoopPitch(LoopName, Pitch);
		return true;
	}

	const FLoadedSound& Source = Impl->LoadedSounds[Key];
	const std::wstring FullPath = FPaths::Combine(FPaths::AudioDir(), FPaths::ToWide(Source.Path));
	ma_sound* Sound = new ma_sound();
	if (ma_sound_init_from_file_w(&Impl->Engine, FullPath.c_str(), MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_LOOPING, &Impl->SFXGroup, nullptr, Sound) != MA_SUCCESS)
	{
		delete Sound;
		return false;
	}

	ma_sound_set_looping(Sound, MA_TRUE);
	ma_sound_set_volume(Sound, Clamp01(Volume));
	ma_sound_set_pitch(Sound, std::clamp(Pitch, 0.1f, 3.0f));
	if (ma_sound_start(Sound) != MA_SUCCESS)
	{
		ma_sound_uninit(Sound);
		delete Sound;
		return false;
	}
	Impl->LoopSounds[LoopName] = FLoadedSound{ Source.Path, true, Sound };
	return true;
}

bool FAudioManager::SetLoopState(const FString& LoopName, const FString& Key, bool bShouldPlay, float Volume, float Pitch)
{
	if (LoopName.empty())
	{
		return false;
	}

	if (bShouldPlay)
	{
		return PlayLoop(Key, LoopName, Volume, Pitch);
	}

	StopLoop(LoopName);
	return true;
}

void FAudioManager::StopLoop(const FString& LoopName)
{
	auto It = Impl->LoopSounds.find(LoopName);
	if (It == Impl->LoopSounds.end())
	{
		return;
	}

	if (It->second.Sound)
	{
		ma_sound_stop(It->second.Sound);
		ma_sound_uninit(It->second.Sound);
		delete It->second.Sound;
	}
	Impl->LoopSounds.erase(It);
}

void FAudioManager::StopAllLoops()
{
	for (auto& Pair : Impl->LoopSounds)
	{
		if (Pair.second.Sound)
		{
			ma_sound_stop(Pair.second.Sound);
			ma_sound_uninit(Pair.second.Sound);
			delete Pair.second.Sound;
		}
	}
	Impl->LoopSounds.clear();
}

void FAudioManager::SetLoopVolume(const FString& LoopName, float Volume)
{
	auto It = Impl->LoopSounds.find(LoopName);
	if (It != Impl->LoopSounds.end() && It->second.Sound)
	{
		ma_sound_set_volume(It->second.Sound, Clamp01(Volume));
	}
}

void FAudioManager::SetLoopPitch(const FString& LoopName, float Pitch)
{
	auto It = Impl->LoopSounds.find(LoopName);
	if (It != Impl->LoopSounds.end() && It->second.Sound)
	{
		ma_sound_set_pitch(It->second.Sound, std::clamp(Pitch, 0.1f, 3.0f));
	}
}

bool FAudioManager::IsLoopPlaying(const FString& LoopName)
{
	auto It = Impl->LoopSounds.find(LoopName);
	return It != Impl->LoopSounds.end() && It->second.Sound && ma_sound_is_playing(It->second.Sound);
}

void FAudioManager::SetMasterVolume(float Volume)
{
	SetBusVolume("Master", Volume);
}

void FAudioManager::SetBusVolume(const FString& BusName, float Volume)
{
	const EAudioBus Bus = ParseBus(BusName);
	const float Clamped = Clamp01(Volume);
	*Impl->GetBusVolumePtr(Bus) = Clamped;

	if (!Impl->bInitialized)
	{
		return;
	}

	ma_sound_group_set_volume(Impl->GetGroup(Bus), Clamped);
	if (Bus == EAudioBus::SFX)
	{
		Impl->UIVolume = Clamped;
		ma_sound_group_set_volume(&Impl->UIGroup, Clamped);
	}
}

float FAudioManager::GetBusVolume(const FString& BusName) const
{
	return *Impl->GetBusVolumePtr(ParseBus(BusName));
}

bool FAudioManager::PlayOneShot(const FString& EventName)
{
	return PlayEvent(EventName);
}

bool FAudioManager::PlayOneShotAt(const FString& EventName, const FVector& Position)
{
	return PlayEventAt(EventName, Position);
}

bool FAudioManager::PlayEvent(const FString& EventName)
{
	return PlayEventAt(EventName, FVector::ZeroVector);
}

bool FAudioManager::PlayEventAt(const FString& EventName, const FVector& Position)
{
	if (!Impl->bInitialized || !Impl->SoundEvents.contains(EventName))
	{
		UE_LOG("[Audio] Missing sound event: %s", EventName.c_str());
		return false;
	}

	FSoundEventDef& Event = Impl->SoundEvents[EventName];
	if (Event.Clips.empty())
	{
		return false;
	}

	const double Now = GetAudioTimeSeconds();
	if (Event.Cooldown > 0.0f && Impl->LastEventPlayTime.contains(EventName))
	{
		if (Now - Impl->LastEventPlayTime[EventName] < Event.Cooldown)
		{
			return false;
		}
	}

	if (Event.MaxInstances > 0)
	{
		int32 ActiveCount = 0;
		for (const FTransientSound& Entry : Impl->TransientSounds)
		{
			if (Entry.EventName == EventName && Entry.Sound && ma_sound_is_playing(Entry.Sound))
			{
				++ActiveCount;
			}
		}
		if (ActiveCount >= Event.MaxInstances)
		{
			return false;
		}
	}

	std::uniform_int_distribution<size_t> ClipDist(0, Event.Clips.size() - 1);
	const FString& Clip = Event.Clips[ClipDist(Impl->Random)];
	const std::wstring FullPath = FPaths::Combine(FPaths::AudioDir(), FPaths::ToWide(Clip));

	ma_sound* Sound = new ma_sound();
	const ma_uint32 Flags = Event.bSpatial ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
	if (ma_sound_init_from_file_w(&Impl->Engine, FullPath.c_str(), Flags, Impl->GetGroup(Event.Bus), nullptr, Sound) != MA_SUCCESS)
	{
		delete Sound;
		UE_LOG("[Audio] Failed to load sound event clip: %s (%s)", EventName.c_str(), Clip.c_str());
		return false;
	}

	std::uniform_real_distribution<float> VolumeDist((std::min)(Event.VolumeMin, Event.VolumeMax), (std::max)(Event.VolumeMin, Event.VolumeMax));
	std::uniform_real_distribution<float> PitchDist((std::min)(Event.PitchMin, Event.PitchMax), (std::max)(Event.PitchMin, Event.PitchMax));

	ma_sound_set_volume(Sound, std::max(0.0f, VolumeDist(Impl->Random)));
	ma_sound_set_pitch(Sound, std::clamp(PitchDist(Impl->Random), 0.1f, 3.0f));
	ma_sound_set_looping(Sound, MA_FALSE);

	if (Event.bSpatial)
	{
		ma_sound_set_position(Sound, Position.X, Position.Y, Position.Z);
		ma_sound_set_min_distance(Sound, std::max(0.0f, Event.MinDistance));
		ma_sound_set_max_distance(Sound, std::max(Event.MinDistance, Event.MaxDistance));
		ma_sound_set_attenuation_model(Sound, Event.Attenuation);
	}

	if (ma_sound_start(Sound) != MA_SUCCESS)
	{
		ma_sound_uninit(Sound);
		delete Sound;
		return false;
	}

	Impl->TransientSounds.push_back(FTransientSound{ EventName, Sound });
	Impl->LastEventPlayTime[EventName] = Now;
	return true;
}

void FAudioManager::StopEvent(const FString& EventName)
{
	if (!Impl->bInitialized || EventName.empty())
	{
		return;
	}

	for (auto It = Impl->TransientSounds.begin(); It != Impl->TransientSounds.end();)
	{
		if (It->EventName != EventName)
		{
			++It;
			continue;
		}

		if (It->Sound)
		{
			ma_sound_stop(It->Sound);
			ma_sound_uninit(It->Sound);
			delete It->Sound;
		}
		It = Impl->TransientSounds.erase(It);
	}
}

void FAudioManager::FadeOutEvent(const FString& EventName, float FadeMilliseconds)
{
	if (!Impl->bInitialized || EventName.empty())
	{
		return;
	}

	const float ClampedFade = std::max(0.0f, FadeMilliseconds);
	for (FTransientSound& Entry : Impl->TransientSounds)
	{
		if (Entry.EventName == EventName && Entry.Sound && ma_sound_is_playing(Entry.Sound))
		{
			ma_sound_stop_with_fade_in_milliseconds(Entry.Sound, static_cast<ma_uint64>(ClampedFade));
		}
	}
}

void FAudioManager::SetListenerTransform(const FVector& Position, const FVector& Forward, const FVector& Up)
{
	Impl->ListenerPosition = Position;
	Impl->ListenerForward = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward.Normalized();
	Impl->ListenerUp = Up.IsNearlyZero() ? FVector::UpVector : Up.Normalized();

	if (!Impl->bInitialized)
	{
		return;
	}

	ma_engine_listener_set_position(&Impl->Engine, 0, Impl->ListenerPosition.X, Impl->ListenerPosition.Y, Impl->ListenerPosition.Z);
	ma_engine_listener_set_direction(&Impl->Engine, 0, Impl->ListenerForward.X, Impl->ListenerForward.Y, Impl->ListenerForward.Z);
	ma_engine_listener_set_world_up(&Impl->Engine, 0, Impl->ListenerUp.X, Impl->ListenerUp.Y, Impl->ListenerUp.Z);
}

void FAudioManager::LoadDefaultAudios()
{
	LoadAudio("CityBgm", "city_bgm.mp3", true);
	LoadAudio("WindBgm", "Environment/Winds.mp3", true);
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
	LoadAudio("SlideLoop", "Movement/Slide/SlideLoop1.wav", true);
	LoadAudio("WallRunRub", "Movement/WallRun/WallRunRub1.wav", true);
}

void FAudioManager::LoadSoundEvents()
{
	Impl->SoundEvents.clear();

	FString Content;
	const std::wstring EventPath = FPaths::Combine(FPaths::AudioDir(), L"SoundEvents.json");
	if (!ReadTextFile(EventPath, Content))
	{
		UE_LOG("[Audio] SoundEvents.json not found: %s", FPaths::ToUtf8(EventPath).c_str());
		return;
	}

	json::JSON Root = json::JSON::Load(Content);
	if (Root.JSONType() != json::JSON::Class::Object)
	{
		UE_LOG("[Audio] SoundEvents.json root must be an object.");
		return;
	}

	for (auto& Pair : Root.ObjectRange())
	{
		const FString EventName = Pair.first;
		json::JSON& Object = Pair.second;
		if (Object.JSONType() != json::JSON::Class::Object)
		{
			continue;
		}

		FSoundEventDef Def;
		Def.Bus = ParseBus(ReadString(Object, "bus", "SFX"));
		Def.bSpatial = ReadBool(Object, "spatial", false);
		Def.VolumeMin = ReadFloat(Object, "volumeMin", 1.0f);
		Def.VolumeMax = ReadFloat(Object, "volumeMax", Def.VolumeMin);
		Def.PitchMin = ReadFloat(Object, "pitchMin", 1.0f);
		Def.PitchMax = ReadFloat(Object, "pitchMax", Def.PitchMin);
		Def.MinDistance = ReadFloat(Object, "minDistance", 100.0f);
		Def.MaxDistance = ReadFloat(Object, "maxDistance", 3000.0f);
		Def.Attenuation = ParseAttenuation(ReadString(Object, "attenuation", "inverse"));
		Def.Cooldown = std::max(0.0f, ReadFloat(Object, "cooldown", 0.0f));
		Def.MaxInstances = std::max(0, ReadInt(Object, "maxInstances", 0));

		if (Object.hasKey("clips") && Object["clips"].JSONType() == json::JSON::Class::Array)
		{
			for (json::JSON& ClipValue : Object["clips"].ArrayRange())
			{
				bool bOk = false;
				const FString Clip = ClipValue.ToString(bOk);
				if (bOk && !Clip.empty())
				{
					Def.Clips.push_back(Clip);
				}
			}
		}

		if (!Def.Clips.empty())
		{
			Impl->SoundEvents[EventName] = Def;
		}
	}

	UE_LOG("[Audio] Loaded %d sound events.", static_cast<int32>(Impl->SoundEvents.size()));
}

void FAudioManager::UpdateListenerFromWorld()
{
	if (!GEngine || !GEngine->GetWorld())
	{
		return;
	}

	FMinimalViewInfo POV;
	if (!GEngine->GetWorld()->GetActivePOV(POV))
	{
		return;
	}

	SetListenerTransform(POV.Location, POV.Rotation.GetForwardVector(), POV.Rotation.GetUpVector());
}

void FAudioManager::CleanupTransientSounds()
{
	for (auto It = Impl->TransientSounds.begin(); It != Impl->TransientSounds.end();)
	{
		ma_sound* Sound = It->Sound;
		if (!Sound || ma_sound_at_end(Sound) || !ma_sound_is_playing(Sound))
		{
			if (Sound)
			{
				ma_sound_uninit(Sound);
				delete Sound;
			}
			It = Impl->TransientSounds.erase(It);
		}
		else
		{
			++It;
		}
	}
}
