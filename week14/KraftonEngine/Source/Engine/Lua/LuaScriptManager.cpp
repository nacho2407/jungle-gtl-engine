#include "LuaScriptManager.h"

#include "Core/Log.h"
#include "Core/Notification.h"
#include "Audio/AudioManager.h"
#include "Component/ActionComponent.h"
#include "Component/LuaScriptComponent.h"
#include "Component/Movement/FloatingPawnMovementComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/CollisionTypes.h"
#include "Runtime/Engine.h"
#include "Viewport/GameViewportClient.h"
#include "Input/InputSystem.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerCameraManager.h"
#include "GameFramework/WaveOscillatorCameraShake.h"
#include "GameFramework/SequenceCameraShake.h"
#include "GameFramework/GameplayStatics.h"
#include "GameFramework/World.h"
#include "Object/UClass.h"
#include "Platform/Paths.h"
#include "Math/Vector.h"
#include "Runtime/WindowsWindow.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <windows.h>  // PostQuitMessage

std::unique_ptr<sol::state> FLuaScriptManager::Lua;
sol::protected_function FLuaScriptManager::OnEscapePressedCallback;
std::mutex FLuaScriptManager::ComponentMutex;
TArray<ULuaScriptComponent*> FLuaScriptManager::RegisteredComponents;
FSubscriptionID FLuaScriptManager::WatchSub = 0;

void FLuaScriptManager::SetOnEscapePressed(sol::protected_function Callback)
{
	OnEscapePressedCallback = std::move(Callback);
}

void FLuaScriptManager::RegisterComponent(ULuaScriptComponent* Component)
{
	if (!Component) return;

	std::lock_guard<std::mutex> Lock(ComponentMutex);
	auto It = std::find(RegisteredComponents.begin(), RegisteredComponents.end(), Component);
	if (It == RegisteredComponents.end())
	{
		RegisteredComponents.push_back(Component);
	}
}

void FLuaScriptManager::InvalidateChangedModules(const TSet<FString>& ChangedFiles)
{
	if (!Lua) return;

	sol::table Loaded = (*Lua)["package"]["loaded"];
	if (!Loaded.valid()) return;

	for (const FString& File : ChangedFiles)
	{
		FString ModuleName = GetModuleNameFromPath(File);
		if (ModuleName.empty()) continue;

		Loaded[ModuleName] = sol::nil;
		UE_LOG("[LuaHotReload] Invalidated module: %s", ModuleName.c_str());
	}
}

FString FLuaScriptManager::GetModuleNameFromPath(const FString& ScriptPath)
{
	if (ScriptPath.empty())
	{
		return {};
	}

	FString Normalized = ScriptPath;
	for (char& Ch : Normalized)
	{
		if (Ch == '\\')
		{
			Ch = '/';
		}
	}

	constexpr const char* LuaExt = ".lua";
	if (Normalized.size() <= 4 || Normalized.substr(Normalized.size() - 4) != LuaExt)
	{
		return {};
	}

	Normalized.erase(Normalized.size() - 4);
	for (char& Ch : Normalized)
	{
		if (Ch == '/')
		{
			Ch = '.';
		}
	}

	return Normalized;
}

void FLuaScriptManager::UnregisterComponent(ULuaScriptComponent* Component)
{
	if (!Component) return;

	std::lock_guard<std::mutex> Lock(ComponentMutex);
	auto It = std::find(RegisteredComponents.begin(), RegisteredComponents.end(), Component);
	if (It != RegisteredComponents.end())
	{
		RegisteredComponents.erase(It);
	}
}

void FLuaScriptManager::OnScriptsChanged(const TSet<FString>& ChangedFiles)
{
	TSet<ULuaScriptComponent*> Targets;

	InvalidateChangedModules(ChangedFiles);

	{
		std::lock_guard<std::mutex> Lock(ComponentMutex);
		for (ULuaScriptComponent* Component : RegisteredComponents)
		{
			if (!Component) continue;

			const FString& ScriptFile = Component->GetScriptFile();
			if (ScriptFile.empty()) continue;

			for (const FString& File : ChangedFiles)
			{
				if (File == ScriptFile)
				{
					Targets.insert(Component);
					break;
				}
			}
		}
	}

	for (ULuaScriptComponent* Component : Targets)
	{
		if (!Component) continue;

		UE_LOG("[LuaHotReload] Reloading: %s", Component->GetScriptFile().c_str());
		FNotificationManager::Get().AddNotification("Lua Reloaded: " + Component->GetScriptFile(), ENotificationType::Success, 3.0f);
		Component->ReloadScript();
	}
}

void FLuaScriptManager::FireOnEscapePressed()
{
	if (!OnEscapePressedCallback.valid())
	{
		return;
	}
	sol::protected_function_result Result = OnEscapePressedCallback();
	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("[Lua] OnEscapePressed callback error: %s", Err.what());
	}
}

void FLuaScriptManager::FireWorldReset()
{
	if (!Lua) return;

	// require 로 한 번 로드된 모듈 테이블은 package.loaded 에 캐시된다. 씬 전환 시에도
	// 살아남기 때문에, 이 두 모듈이 보유한 죽은-월드 참조를 비워준다.
	sol::table Loaded = (*Lua)["package"]["loaded"];
	if (!Loaded.valid()) return;

	// 1) CoroutineManager — 옛 액터의 lua 클로저가 캡처한 환경의 obj 가 dangling.
	//    Wait(30) 도중에 씬 전환되면 새 월드 Tick 에서 만료되면서 freed AActor* deref.
	if (sol::object Coro = Loaded["CoroutineManager"]; Coro.valid() && Coro.get_type() == sol::type::table)
	{
		Coro.as<sol::table>()["coroutines"] = Lua->create_table();
	}

	// 2) ObjRegistry — 액터 핸들 캐시. 새 월드의 BeginPlay 가 다시 등록해줄 때까지 nil 로.
	if (sol::object Reg = Loaded["ObjRegistry"]; Reg.valid() && Reg.get_type() == sol::type::table)
	{
		sol::table T = Reg.as<sol::table>();
		T["car"]        = sol::nil;
		T["carCamera"]  = sol::nil;
		T["carGas"]     = sol::nil;
		T["manObj"]     = sol::nil;
		T["manCamera"]  = sol::nil;
		T["gasNozzle"]  = sol::nil;
		T["carWasher"]  = sol::nil;
		T["dirtyCar"]   = sol::nil;
		T["policeCars"] = Lua->create_table();
	}
}

void FLuaScriptManager::Initialize()
{
	Lua = std::make_unique<sol::state>();
	Lua->open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::coroutine);
	(*Lua)["package"]["path"] = FPaths::ToUtf8(FPaths::Combine(FPaths::ScriptDir(), L"?.lua").c_str());

	// 한글 경로 호환을 위해 require 의 파일 검색을 wide-aware 로 교체.
	// Lua 5.2+ 는 package.searchers, Lua 5.1/LuaJIT 은 package.loaders 를 사용한다.
	sol::table Package = (*Lua)["package"];
	sol::object Searchers = Package["searchers"];
	sol::table ModuleLoaders = Searchers.valid() && Searchers.get_type() == sol::type::table
		? Searchers.as<sol::table>()
		: Package["loaders"].get<sol::table>();
	ModuleLoaders[2] = [](sol::this_state ts, const std::string& ModName) -> sol::object
	{
		sol::state_view L(ts);
		const std::wstring WidePath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ModName + ".lua"));
		std::error_code EC;
		if (!std::filesystem::exists(WidePath, EC))
		{
			return sol::make_object(L, std::string("\n\tno file '") + FPaths::ToUtf8(WidePath) + "'");
		}

		FString Content;
		if (!ReadScriptFileContent(ModName + ".lua", Content))
		{
			return sol::make_object(L, std::string("\n\tcannot read '") + FPaths::ToUtf8(WidePath) + "'");
		}

		const FString ChunkName = FPaths::ToUtf8(WidePath);
		sol::load_result LR = L.load(Content, ChunkName);
		if (!LR.valid())
		{
			sol::error Err = LR;
			return sol::make_object(L, std::string("\n\t") + Err.what());
		}
		return LR.get<sol::object>();
	};

	RegisterBindings(*Lua);

	FWatchID WatchID = FDirectoryWatcher::Get().Watch(FPaths::ScriptDir(), "");
	if (WatchID != 0)
	{
		WatchSub = FDirectoryWatcher::Get().Subscribe(WatchID,
			[](const TSet<FString>& Files) { FLuaScriptManager::OnScriptsChanged(Files); });
	}
}

void FLuaScriptManager::Shutdown()
{
	if (WatchSub != 0)
	{
		FDirectoryWatcher::Get().Unsubscribe(WatchSub);
		WatchSub = 0;
	}

	{
		std::lock_guard<std::mutex> Lock(ComponentMutex);
		RegisteredComponents.clear();
	}

	// 등록된 Lua 콜백 (sol::protected_function 들) 을 lua_State 가 살아있는 동안 먼저 release.
	// static 멤버라 프로그램 종료 시점까지 살아있는데, 그때 destructor 가 luaL_unref 를
	// 호출하면서 이미 reset 된 lua_State 를 만지면 크래시. 빈 함수로 덮어써 deref 를 지금
	// (Lua 가 valid 한 동안) 일으킨다.
	OnEscapePressedCallback = sol::protected_function();

	Lua.reset();
}

FString FLuaScriptManager::ResolveScriptPath(const FString& ScriptFile)
{
	std::wstring FullPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptFile));
	return FPaths::ToUtf8(FullPath);
}

bool FLuaScriptManager::ReadScriptFileContent(const FString& ScriptFile, FString& OutContent)
{
	const std::wstring WidePath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptFile));
	std::ifstream File(WidePath.c_str(), std::ios::binary);
	if (!File.is_open())
	{
		return false;
	}
	std::ostringstream SS;
	SS << File.rdbuf();
	OutContent = SS.str();
	return true;
}

bool FLuaScriptManager::OpenOrCreateScript(const FString& ScriptFile)
{
	std::wstring FullPath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(ScriptFile));
	if (!std::filesystem::exists(FullPath))
	{
		FPaths::CreateDir(FPaths::ScriptDir());

		const std::wstring TemplatePath = FPaths::Combine(FPaths::ScriptDir(), L"template.lua");
		std::error_code Error;
		if (std::filesystem::exists(TemplatePath))
		{
			std::filesystem::copy_file(TemplatePath, FullPath, std::filesystem::copy_options::none, Error);
			if (Error)
			{
				UE_LOG("Failed to copy Lua script template: %s", Error.message().c_str());
			}
		}

		if (!std::filesystem::exists(FullPath))
		{
			std::ofstream Out(FullPath);
			if (!Out)
			{
				return false;
			}
		}
	}

	HINSTANCE HInst = ShellExecuteW(nullptr, L"open", FullPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

	if ((INT_PTR)HInst <= 32)
	{
		return false;
	}

	return true;
}

sol::state& FLuaScriptManager::GetState()
{
	return *Lua;
}

void FLuaScriptManager::RegisterBindings(sol::state& Lua)
{
	RegisterLuaHelpers(Lua);
	RegisterCoreBindings(Lua);
	RegisterMathBindings(Lua);
	RegisterActorBindings(Lua);
	RegisterUIBindings(Lua);
}

FInputSystemSnapshot FLuaScriptManager::GetLuaInputSnapshot()
{
	if (GEngine)
	{
		if (UGameViewportClient* GameViewportClient = GEngine->GetGameViewportClient())
		{
			FInputSystemSnapshot Snapshot = GameViewportClient->GetGameInputSnapshot();
			return Snapshot;
		}
	}

	return InputSystem::Get().MakeSnapshot();
}

void FLuaScriptManager::RegisterLuaHelpers(sol::state& Lua)
{
	// 한글 경로 호환 — safe_script_file 은 내부적으로 fopen(UTF-8) 을 쓰므로 ANSI 해석에서
	// 깨진다. wide ifstream 으로 직접 읽어 safe_script(string) 으로 실행.
	FString Content;
	if (!ReadScriptFileContent("CoroutineManager.lua", Content))
	{
		UE_LOG("[Lua] Failed to load CoroutineManager.lua");
		return;
	}
	const FString ChunkName = ResolveScriptPath("CoroutineManager.lua");
	sol::protected_function_result Result = Lua.safe_script(Content, sol::script_pass_on_error, ChunkName);
	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("[Lua] CoroutineManager.lua error: %s", Err.what());
	}
}

void FLuaScriptManager::RegisterCoreBindings(sol::state& Lua)
{
	Lua.set_function("print", [](sol::variadic_args Args)
	{
		FString Message;

		for (auto Arg : Args)
		{
			if (!Message.empty())
			{
				Message += "\t";
			}

			Message += Arg.as<FString>();
		}

		UE_LOG("[Lua] %s", Message.c_str());
	});

	sol::table Input = Lua.create_named_table("Input");
	Input.set_function("GetKeyDown", sol::overload(
		[](int VK)
	{
		return GetLuaInputSnapshot().WasPressed(VK);
	}));
	Input.set_function("GetKey", sol::overload(
		[](int VK)
	{
		return GetLuaInputSnapshot().IsDown(VK);
	}));
	Input.set_function("GetKeyUp", sol::overload(
		[](int VK)
	{
		return GetLuaInputSnapshot().WasReleased(VK);
	}));
	Input.set_function("GetMouseDeltaX", []()
	{
		return GetLuaInputSnapshot().MouseDeltaX;
	});
	Input.set_function("GetMouseDeltaY", []()
	{
		return GetLuaInputSnapshot().MouseDeltaY;
	});

	// Engine — 게임 일시정지 / 종료.
	sol::table Engine = Lua.create_named_table("Engine");
	Engine.set_function("PauseGame", []()
	{
		if (GEngine)
		{
			if (UWorld* World = GEngine->GetWorld())
			{
				World->SetPaused(true);
			}
		}
	});
	Engine.set_function("ResumeGame", []()
	{
		if (GEngine)
		{
			if (UWorld* World = GEngine->GetWorld())
			{
				World->SetPaused(false);
			}
		}
	});
	Engine.set_function("IsPaused", []()
	{
		if (GEngine)
		{
			if (UWorld* World = GEngine->GetWorld())
			{
				return World->IsPaused();
			}
		}
		return false;
	});
	Engine.set_function("GetViewportSize", []() -> sol::table
	{
		sol::table Result = FLuaScriptManager::GetState().create_table();
		Result["Width"] = 0.0f;
		Result["Height"] = 0.0f;

		if (GEngine)
		{
			if (FWindowsWindow* Window = GEngine->GetWindow())
			{
				Result["Width"] = Window->GetWidth();
				Result["Height"] = Window->GetHeight();
			}
		}

		return Result;
	});
	Engine.set_function("Exit", []()
	{
		// WM_QUIT — FEngineLoop::Run 이 PumpMessages 에서 잡고 정상 shutdown.
		PostQuitMessage(0);
	});
	Engine.set_function("SetOnEscape", [](sol::protected_function Callback)
	{
		FLuaScriptManager::SetOnEscapePressed(std::move(Callback));
	});

	sol::table Key = Lua.create_named_table("Key");
	Key["W"] = static_cast<int32>('W');
	Key["A"] = static_cast<int32>('A');
	Key["S"] = static_cast<int32>('S');
	Key["D"] = static_cast<int32>('D');
	Key["R"] = static_cast<int32>('R');
	Key["Space"] = VK_SPACE;
	Key["Escape"] = VK_ESCAPE;
	Key["F1"] = VK_F1;
	Key["F2"] = VK_F2;

	sol::table CameraManager = Lua.create_named_table("CameraManager");
	CameraManager.set_function("ToggleActorCamera", [](const FString& ActorName, sol::optional<float> BlendTime)
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return false;
		}

		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		return Manager ? Manager->ToggleActiveCameraForActor(ActorName, BlendTime.value_or(0.0f)) : false;
	});
	CameraManager.set_function("ToggleOwnerCamera", [](AActor* Actor, sol::optional<float> BlendTime)
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return false;
		}

		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		return Manager ? Manager->ToggleActiveCameraForActor(Actor, BlendTime.value_or(0.0f)) : false;
	});
	CameraManager.set_function("PossessCamera", [](UCameraComponent* Camera)
	{
		if (!GEngine || !GEngine->GetWorld() || !Camera)
		{
			return false;
		}

		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (!Manager)
		{
			return false;
		}

		Manager->SetActiveCamera(Camera);
		Manager->Possess(Camera);
		return true;
	});
	CameraManager.set_function("GetActiveCameraOwner", []() -> AActor*
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return nullptr;
		}
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		UCameraComponent* ActiveCamera = Manager ? Manager->GetActiveCamera() : nullptr;
		return ActiveCamera ? ActiveCamera->GetOwner() : nullptr;
	});
	CameraManager.set_function("GetPossessedCamera", []() -> UCameraComponent*
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return nullptr;
		}
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		return Manager ? Manager->GetPossessedCamera() : nullptr;
	});
	CameraManager.set_function("GetPossessedCameraOwner", []() -> AActor*
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return nullptr;
		}
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		UCameraComponent* PossessedCamera = Manager ? Manager->GetPossessedCamera() : nullptr;
		return PossessedCamera ? PossessedCamera->GetOwner() : nullptr;
	});
	CameraManager.set_function("FadeOut", [](float Duration)
	{
		if (!GEngine || !GEngine->GetWorld()) return;
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (Manager)
		{
			Manager->StartCameraFade(0.0f, 1.0f, Duration, FLinearColor::Black(), false, true);
		}
	});
	CameraManager.set_function("FadeIn", [](float Duration)
	{
		if (!GEngine || !GEngine->GetWorld()) return;
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (Manager)
		{
			Manager->StartCameraFade(1.0f, 0.0f, Duration, FLinearColor::Black(), false, true);
		}
	});
	CameraManager.set_function("SetVignette", [](float Intensity, float Radius, float Softness)
	{
		if (!GEngine || !GEngine->GetWorld()) return;
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (Manager)
		{
			Manager->SetCameraVignette(Intensity, Radius, Softness, FLinearColor::Black());
		}
	});
	CameraManager.set_function("ClearVignette", []()
	{
		if (!GEngine || !GEngine->GetWorld()) return;
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (Manager)
		{
			Manager->ClearCameraVignette();
		}
	});
	CameraManager.set_function("SetViewTargetWithBlend", [](AActor* Target, float BlendTime)
	{
		if (!GEngine || !GEngine->GetWorld() || !Target) return;

		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			PC->SetViewTargetWithBlend(Target, BlendTime);
		}
	});
	// Sample wave-oscillator shake — Lua console / 스크립트에서 즉시 흔들기 테스트용.
	// 호출 예: CameraManager.StartWaveShake(1.0)
	CameraManager.set_function("StartWaveShake", [](sol::optional<float> Scale)
	{
		if (!GEngine || !GEngine->GetWorld()) return;
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (Manager)
		{
			Manager->StartCameraShake<UWaveOscillatorCameraShake>(Scale.value_or(1.0f));
		}
	});
	CameraManager.set_function("StartSequenceShake", [](sol::optional<float> Scale)
	{
		if (!GEngine || !GEngine->GetWorld()) return;
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (Manager)
		{
			Manager->StartCameraShake<USequenceCameraShake>(Scale.value_or(1.0f));
		}
	});

	sol::table AudioManager = Lua.create_named_table("AudioManager");
	AudioManager.set_function("Load", [](const FString& SoundName, const FString& Path, sol::optional<bool> bLoop)
	{
		return FAudioManager::Get().LoadAudio(SoundName, Path, bLoop.value_or(false));
	});
	AudioManager.set_function("Play", [](const FString& SoundName, float Volume)
	{
		FAudioManager::Get().PlayAudio(SoundName, Volume);
	});
	AudioManager.set_function("PlayBGM", [](const FString& SoundName, float Volume)
	{
		FAudioManager::Get().PlayBGM(SoundName, Volume);
	});
	AudioManager.set_function("StopBGM", []()
	{
		FAudioManager::Get().StopBGM();
	});
	AudioManager.set_function("PlayLoop", [](const FString& SoundName, const FString& LoopName, sol::optional<float> Volume, sol::optional<float> Pitch)
	{
		FAudioManager::Get().PlayLoop(SoundName, LoopName, Volume.value_or(1.0f), Pitch.value_or(1.0f));
	});
	AudioManager.set_function("StopLoop", [](const FString& LoopName)
	{
		FAudioManager::Get().StopLoop(LoopName);
	});
	AudioManager.set_function("StopAllLoops", []()
	{
		FAudioManager::Get().StopAllLoops();
	});
	AudioManager.set_function("SetLoopVolume", [](const FString& LoopName, float Volume)
	{
		FAudioManager::Get().SetLoopVolume(LoopName, Volume);
	});
	AudioManager.set_function("SetLoopPitch", [](const FString& LoopName, float Pitch)
	{
		FAudioManager::Get().SetLoopPitch(LoopName, Pitch);
	});
	AudioManager.set_function("IsLoopPlaying", [](const FString& LoopName)
	{
		return FAudioManager::Get().IsLoopPlaying(LoopName);
	});

	Lua.set_function("LoadAudio", [](const FString& SoundName, const FString& Path, sol::optional<bool> bLoop)
	{
		return FAudioManager::Get().LoadAudio(SoundName, Path, bLoop.value_or(false));
	});
}

void FLuaScriptManager::RegisterMathBindings(sol::state& Lua)
{
	Lua.new_usertype<FVector>("Vector",
		sol::constructors<FVector(), FVector(float, float, float)>(),
		"X", &FVector::X,
		"Y", &FVector::Y,
		"Z", &FVector::Z,
		"Length", &FVector::Length,
		"Normalize", &FVector::Normalize,
		"Normalized", &FVector::Normalized,
		"Dot", &FVector::Dot,
		"Cross", sol::overload(
		static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::Cross),
		static_cast<FVector(*)(const FVector&, const FVector&)>(&FVector::Cross)
	),
		"Distance", &FVector::Distance,
		"DistSquared", &FVector::DistSquared,
		"Lerp", &FVector::Lerp,
		sol::meta_function::addition, sol::overload(
		static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::operator+),
		static_cast<FVector(FVector::*)(float) const>(&FVector::operator+)
	),
		sol::meta_function::subtraction, sol::overload(
		static_cast<FVector(FVector::*)(const FVector&) const>(&FVector::operator-),
		static_cast<FVector(FVector::*)(float) const>(&FVector::operator-)
	),
		sol::meta_function::multiplication, &FVector::operator*,
		sol::meta_function::division, &FVector::operator/,
		"Zero", []() { return FVector::ZeroVector; },
		"One", []() { return FVector::OneVector; },
		"Up", []() { return FVector::UpVector; },
		"Down", []() { return FVector::DownVector; },
		"Forward", []() { return FVector::ForwardVector; },
		"Backward", []() { return FVector::BackwardVector; },
		"Right", []() { return FVector::RightVector; },
		"Left", []() { return FVector::LeftVector; },
		"XAxis", []() { return FVector::XAxisVector; },
		"YAxis", []() { return FVector::YAxisVector; },
		"ZAxis", []() { return FVector::ZAxisVector; });
}

void FLuaScriptManager::RegisterActorBindings(sol::state& Lua)
{
	Lua.new_usertype<UActionComponent>("ActionComponent",
		"HitStop", &UActionComponent::HitStop,
		"HitSquash", &UActionComponent::HitSquash,
		"Knockback", &UActionComponent::Knockback,
		"Slomo", &UActionComponent::Slomo,
		"StopHitStop", &UActionComponent::StopHitStop,
		"StopHitSquash", &UActionComponent::StopHitSquash,
		"StopKnockback", &UActionComponent::StopKnockback,
		"StopSlomo", &UActionComponent::StopSlomo,
		"StopAllActions", &UActionComponent::StopAllActions);

	Lua.new_usertype<UFloatingPawnMovementComponent>("FloatingPawnMovementComponent",
		"SetMoveInput", &UFloatingPawnMovementComponent::SetMoveInput,
		"SetLookInput", &UFloatingPawnMovementComponent::SetLookInput);

	Lua.new_usertype<USceneComponent>("SceneComponent",
		"Location", sol::property(
		[](USceneComponent& Component)
	{
		return Component.GetWorldLocation();
	},
		[](USceneComponent& Component, const FVector& Location)
	{
		Component.SetWorldLocation(Location);
	}
	),
		"Rotation", sol::property(
		[](USceneComponent& Component)
	{
		return Component.GetRelativeRotation().ToVector();
	},
		[](USceneComponent& Component, const FVector& Rotation)
	{
		Component.SetRelativeRotation(Rotation);
	}
	),
		"Forward", sol::property([](USceneComponent& Component)
	{
		return Component.GetForwardVector();
	}
	),
		"Right", sol::property([](USceneComponent& Component)
	{
		return Component.GetRightVector();
	}
	),
		"Up", sol::property([](USceneComponent& Component)
	{
		return Component.GetUpVector();
	}
	),
		"GetLocation", [](USceneComponent& Component)
	{
		return Component.GetWorldLocation();
	},
		"SetLocation", [](USceneComponent& Component, const FVector& Location)
	{
		Component.SetWorldLocation(Location);
	},
		"GetRotation", [](USceneComponent& Component)
	{
		return Component.GetRelativeRotation().ToVector();
	},
		"SetRotation", [](USceneComponent& Component, const FVector& Rotation)
	{
		Component.SetRelativeRotation(Rotation);
	},

		// 부모 기준 상대 위치 — 동일한 메시를 4개 깐 바퀴 같은 케이스에서 앞/뒤 구분 등
		// 위치 기반 필터링에 쓰인다. 월드 위치는 위 "Location" 프로퍼티 참고.
		"RelativeLocation", sol::property(
		[](USceneComponent& Component) { return Component.GetRelativeLocation(); },
		[](USceneComponent& Component, const FVector& V) { Component.SetRelativeLocation(V); }
		));

	Lua.new_usertype<UPrimitiveComponent>("PrimitiveComponent",
		sol::base_classes, sol::bases<USceneComponent>(),
		"SetSimulatePhysics", &UPrimitiveComponent::SetSimulatePhysics,
		"GetSimulatePhysics", &UPrimitiveComponent::GetSimulatePhysics,
		"AddForce", &UPrimitiveComponent::AddForce,
		"AddForceAtLocation", &UPrimitiveComponent::AddForceAtLocation,
		"AddTorque", &UPrimitiveComponent::AddTorque,
		"GetLinearVelocity", &UPrimitiveComponent::GetLinearVelocity,
		"SetLinearVelocity", &UPrimitiveComponent::SetLinearVelocity,
		"GetAngularVelocity", &UPrimitiveComponent::GetAngularVelocity,
		"SetAngularVelocity", &UPrimitiveComponent::SetAngularVelocity,
		"GetMass", &UPrimitiveComponent::GetMass,
		"SetMass", &UPrimitiveComponent::SetMass,
		"GetGenerateOverlapEvents", &UPrimitiveComponent::GetGenerateOverlapEvents);

	// 메시 에셋 경로로 컴포넌트 식별 가능하게 노출. 자동 생성된 FName ("UStaticMeshComponent_41")
	// 은 월드 초기화 순서에 따라 카운터가 달라져 빌드별로 매칭이 깨질 수 있다. 메시 경로는
	// 씬 파일에 명시 저장되므로 deterministic.
	Lua.new_usertype<UStaticMeshComponent>("StaticMeshComponent",
		sol::base_classes, sol::bases<UPrimitiveComponent, USceneComponent>(),
		"MeshPath", sol::property([](UStaticMeshComponent& C) { return C.GetStaticMeshPath(); }),
		"GetMeshPath", [](UStaticMeshComponent& C) { return C.GetStaticMeshPath(); });

	Lua.new_usertype<FHitResult>("HitResult",
		"HitComponent", &FHitResult::HitComponent,
		"HitActor", &FHitResult::HitActor,
		"Distance", &FHitResult::Distance,
		"PenetrationDepth", &FHitResult::PenetrationDepth,
		"WorldHitLocation", &FHitResult::WorldHitLocation,
		"WorldNormal", &FHitResult::WorldNormal,
		"ImpactNormal", &FHitResult::ImpactNormal,
		"FaceIndex", &FHitResult::FaceIndex,
		"bHit", &FHitResult::bHit);

	Lua.new_usertype<UCameraComponent>("CameraComponent",
		sol::base_classes, sol::bases<USceneComponent>());

	Lua.new_usertype<AActor>("Actor",
		"Location", sol::property(
		[](AActor& Actor)
	{
		return Actor.GetActorLocation();
	},
		[](AActor& Actor, const FVector& Location)
	{
		Actor.SetActorLocation(Location);
	}
	),
		"Rotation", sol::property(
		[](AActor& Actor)
	{
		return Actor.GetActorRotation().ToVector();
	},
		[](AActor& Actor, const FVector& Rotation)
	{
		Actor.SetActorRotation(Rotation);
	}
	),

		"Scale", sol::property(
		[](AActor& Actor)
	{
		return Actor.GetActorScale();
	},
		[](AActor& Actor, const FVector& Scale)
	{
		Actor.SetActorScale(Scale);
	}
	),

		"Forward", sol::property([](AActor& Actor)
	{
		return Actor.GetActorForward();
	}
	),
		
		"Right", sol::property([](AActor& Actor)
	{
		return Actor.GetActorRight();
	}
	),

		"AddWorldOffset", [](AActor& Actor, const FVector& Offset)
	{
		Actor.AddActorWorldOffset(Offset);
	},

		"Destroy", [](AActor& Actor)
	{
		// World->DestroyActor가 EndPlay + 정리. Lua는 호출 후 해당 액터를 더 참조하지 말 것.
		if (UWorld* W = Actor.GetWorld()) W->DestroyActor(&Actor);
	},

		"IsValid", [](AActor* Actor)
	{
		// Lua가 보유한 actor 핸들이 cpp 측에서 destroy됐는지 확인. nil/destroyed면 false.
		return Actor != nullptr && IsAliveObject(Actor);
	},

		"HasTag", [](AActor& Actor, const FString& Tag)
	{
		return Actor.HasTag(FName(Tag));
	},
		"AddTag", [](AActor& Actor, const FString& Tag)
	{
		Actor.AddTag(FName(Tag));
	},
		"RemoveTag", [](AActor& Actor, const FString& Tag)
	{
		Actor.RemoveTag(FName(Tag));
	},

		"GetFloatingPawnMovement", [](AActor& Actor)
	{
		return Actor.GetComponentByClass<UFloatingPawnMovementComponent>();
	},

		"GetCamera", [](AActor& Actor)
	{
		return Actor.GetComponentByClass<UCameraComponent>();
	},

		"GetActionComponent", [](AActor& Actor)
	{
		return Actor.GetComponentByClass<UActionComponent>();
	},

		"GetRootPrimitiveComponent", [](AActor& Actor) -> UPrimitiveComponent*
	{
		return Cast<UPrimitiveComponent>(Actor.GetRootComponent());
	},

		"GetPrimitiveComponent", [](AActor& Actor) -> UPrimitiveComponent*
	{
		return Actor.GetComponentByClass<UPrimitiveComponent>();
	},

		"GetPrimitiveComponentByName", [](AActor& Actor, const FString& ComponentName) -> UPrimitiveComponent*
	{
		for (UActorComponent* Component : Actor.GetComponents())
		{
			UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
			if (PrimitiveComponent && PrimitiveComponent->GetFName().ToString() == ComponentName)
			{
				return PrimitiveComponent;
			}
		}
		return nullptr;
	},

		"GetComponentByName", [](AActor& Actor, const FString& ComponentName) -> USceneComponent*
	{
		for (UActorComponent* Component : Actor.GetComponents())
		{
			USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
			if (SceneComponent && SceneComponent->GetFName().ToString() == ComponentName)
			{
				return SceneComponent;
			}
		}
		return nullptr;
	},

		// 메시 에셋 경로로 첫 매칭 컴포넌트 반환. 자동 생성된 FName 의존성을 피하기 위한
		// deterministic 방식 — 핸들 / 점멸등 처럼 한 액터에 1개뿐인 메시 컴포넌트용.
		"FindFirstMeshComponentByPath", [](AActor& Actor, const FString& MeshPath) -> UStaticMeshComponent*
	{
		for (UActorComponent* Component : Actor.GetComponents())
		{
			UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
			if (Mesh && Mesh->GetStaticMeshPath() == MeshPath)
			{
				return Mesh;
			}
		}
		return nullptr;
	},

		// 같은 메시를 여러 번 사용하는 경우(타이어 4개 등) 전부 모아서 반환. 호출 측에서 추가
		// 필터링(RelativeLocation 의 X 부호로 앞/뒤 구분 등) 하면 된다.
		"FindMeshComponentsByPath", [](AActor& Actor, const FString& MeshPath) -> sol::table
	{
		sol::table Result = FLuaScriptManager::GetState().create_table();
		int32 Idx = 1; // Lua arrays are 1-indexed
		for (UActorComponent* Component : Actor.GetComponents())
		{
			UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
			if (Mesh && Mesh->GetStaticMeshPath() == MeshPath)
			{
				Result[Idx++] = Mesh;
			}
		}
		return Result;
	},

		"UUID", sol::property([](AActor& Actor)
	{
		return Actor.GetUUID();
	}),

		"Name", sol::property([](AActor& Actor)
	{
		return Actor.GetFName().ToString();
	}));

	Lua.new_usertype<APawn>("Pawn",
		sol::base_classes, sol::bases<AActor>(),
		"IsPossessed", &APawn::IsPossessed,
		"SetAutoPossessPlayer", &APawn::SetAutoPossessPlayer,
		"GetAutoPossessPlayer", &APawn::GetAutoPossessPlayer);

	// --- World binding — 런타임 액터 spawn 용 (Engine 일반 기능) ---
	sol::table World = Lua.create_named_table("World");
	World.set_function("SpawnActor", [](const FString& ClassName) -> AActor*
	{
		if (!GEngine) return nullptr;
		UWorld* W = GEngine->GetWorld();
		if (!W) return nullptr;
		UClass* Cls = UClass::FindByName(ClassName.c_str());
		if (!Cls) return nullptr;
		return W->SpawnActorByClass(Cls);
	});
	World.set_function("FindActorByName", [](const FString& ActorName) -> AActor*
	{
		if (!GEngine || !GEngine->GetWorld()) return nullptr;
		for (AActor* Actor : GEngine->GetWorld()->GetActors())
		{
			if (Actor && Actor->GetFName().ToString() == ActorName)
			{
				return Actor;
			}
		}
		return nullptr;
	});
	World.set_function("FindFirstActorByClass", [](const FString& ClassName) -> AActor*
	{
		if (!GEngine || !GEngine->GetWorld()) return nullptr;
		UClass* Cls = UClass::FindByName(ClassName.c_str());
		if (!Cls) return nullptr;
		for (AActor* Actor : GEngine->GetWorld()->GetActors())
		{
			if (Actor && Actor->GetClass()->IsA(Cls))
			{
				return Actor;
			}
		}
		return nullptr;
	});
	World.set_function("FindFirstActorByTag", [](const FString& Tag) -> AActor*
	{
		return FGameplayStatics::FindFirstActorByTag(
			GEngine ? GEngine->GetWorld() : nullptr, FName(Tag));
	});
	World.set_function("FindActorsByTag", [](const FString& Tag) -> sol::table
	{
		sol::table Result = FLuaScriptManager::GetState().create_table();
		const TArray<AActor*> Found = FGameplayStatics::FindActorsByTag(
			GEngine ? GEngine->GetWorld() : nullptr, FName(Tag));
		int Idx = 1; // Lua arrays are 1-indexed
		for (AActor* Actor : Found)
		{
			Result[Idx++] = Actor;
		}
		return Result;
	});

	// 게임 특화 usertype/enum/global(GetGameState 등) 은 Game 모듈의
	// RegisterGameLuaBindings 가 등록한다. 호출 순서는 GameEngine/EditorEngine::Init
	// 에서 UEngine::Init() 직후.
}

void FLuaScriptManager::RegisterUIBindings(sol::state& Lua)
{
	Lua.new_usertype<UUserWidget>("UserWidget",
		"AddToViewport", [](UUserWidget& Widget)
	{
		Widget.AddToViewport();
	},
		"AddToViewportZ", [](UUserWidget& Widget, int32 ZOrder)
	{
		Widget.AddToViewport(ZOrder);
	},
		"RemoveFromParent", &UUserWidget::RemoveFromParent,
		"Show", [](UUserWidget& Widget)
	{
		Widget.AddToViewport();
	},
		"Hide", &UUserWidget::RemoveFromParent,
		"show", [](UUserWidget& Widget)
	{
		Widget.AddToViewport();
	},
		"hide", &UUserWidget::RemoveFromParent,
		"IsInViewport", &UUserWidget::IsInViewport,
		"bind_click", [](UUserWidget& Widget, const FString& ElementId, sol::protected_function Callback)
	{
		Widget.BindClick(ElementId, Callback);
	},
		"SetText", &UUserWidget::SetText,
		"set_text", &UUserWidget::SetText,
		"SetProperty", &UUserWidget::SetProperty,
		"set_property", &UUserWidget::SetProperty,
		"SetWantsMouse", &UUserWidget::SetWantsMouse,
		"WantsMouse", &UUserWidget::WantsMouse);

	sol::table UI = Lua.create_named_table("UI");
	UI.set_function("CreateWidget", [](const FString& DocumentPath)
	{
		return UUIManager::Get().CreateWidget(nullptr, DocumentPath);
	});
}
