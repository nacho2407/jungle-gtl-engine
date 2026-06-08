#include "LuaScriptManager.h"

#include "Lua/LuaDocRegistry.h"
#include "Core/Logging/Log.h"
#include "Core/Logging/Notification.h"
#include "Audio/AudioManager.h"
#include "Animation/AnimationManager.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Component/Input/ActionComponent.h"
#include "Component/Script/LuaScriptComponent.h"
#include "Component/Input/InputComponent.h"
#include "Animation/Instance/LuaAnimInstance.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Movement/FloatingPawnMovementComponent.h"
#include "Component/Movement/PhysX/VehicleMovementComponent4W.h"
#include "Component/Movement/PhysX/VehicleMovementComponentTank.h"
#include "Component/Camera/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Primitive/SkinnedMeshComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/Light/PointLightComponent.h"
#include "Component/Light/SpotLightComponent.h"
#include "Particles/ParticleSystemManager.h"
#include "Core/Types/CollisionTypes.h"
#include "Runtime/Engine.h"
#include "Viewport/GameViewportClient.h"
#include "Input/InputSystem.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/Character.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/Camera/WaveOscillatorCameraShake.h"
#include "GameFramework/Camera/SequenceCameraShake.h"
#include "GameFramework/GameMode/GameplayStatics.h"
#include "GameFramework/World.h"
#include "Debug/DrawDebugHelpers.h"
#include "Object/Reflection/UClass.h"
#include "Render/Scene/FScene.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Platform/Paths.h"
#include "Core/ProjectSettings.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Platform/WindowsWindow.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <windows.h>

#include "Intermediate/Generated/LuaBindings.generated.h"

namespace
{
TArray<FString> BuildLuaModuleScriptFileCandidates(const FString& ModuleOrScriptFile)
{
	FString Normalized = ModuleOrScriptFile;
	for (char& Ch : Normalized)
	{
		if (Ch == '\\')
		{
			Ch = '/';
		}
	}

	TArray<FString> Candidates;
	auto AddCandidate = [&Candidates](const FString& Candidate)
	{
		if (std::find(Candidates.begin(), Candidates.end(), Candidate) == Candidates.end())
		{
			Candidates.push_back(Candidate);
		}
	};

	constexpr const char* LuaExt = ".lua";
	const bool bHasLuaExtension = Normalized.size() >= 4 && Normalized.substr(Normalized.size() - 4) == LuaExt;
	if (bHasLuaExtension)
	{
		AddCandidate(Normalized);
		return Candidates;
	}

	// 경로 구분자가 이미 있는 모듈명은 파일명 안의 점을 보존하는 후보를 먼저 사용합니다.
	// 예: Dialogue/Prologue.dialogue -> Dialogue/Prologue.dialogue.lua
	AddCandidate(Normalized + LuaExt);

	// Lua 관례형 dotted module도 기존처럼 하위 경로로 변환해서 지원합니다.
	// 예: Game.FractureDirector -> Game/FractureDirector.lua
	FString DottedModulePath = Normalized;
	for (char& Ch : DottedModulePath)
	{
		if (Ch == '.')
		{
			Ch = '/';
		}
	}
	AddCandidate(DottedModulePath + LuaExt);
	return Candidates;
}

sol::object RequireLuaModule(sol::state& LuaState, const FString& ModuleName)
{
	sol::protected_function Require = LuaState["require"];
	if (!Require.valid())
	{
		return sol::make_object(LuaState, sol::lua_nil);
	}

	sol::protected_function_result Result = Require(ModuleName);
	if (!Result.valid())
	{
		sol::error Error = Result;
		UE_LOG("[Lua] require('%s') error: %s", ModuleName.c_str(), Error.what());
		return sol::make_object(LuaState, sol::lua_nil);
	}

	return Result.get<sol::object>();
}
}

std::unique_ptr<sol::state> FLuaScriptManager::Lua;
sol::protected_function FLuaScriptManager::OnEscapePressedCallback;
std::mutex FLuaScriptManager::ComponentMutex;
TArray<ULuaScriptComponent*> FLuaScriptManager::RegisteredComponents;
TArray<ULuaAnimInstance*>    FLuaScriptManager::RegisteredAnimInstances;
FSubscriptionID FLuaScriptManager::WatchSub = 0;
float FLuaScriptManager::RuntimeGamma = 2.4f;
float FLuaScriptManager::RuntimeSaturation = 1.0f;
float FLuaScriptManager::RuntimeMouseSensitivity = 1.0f;

void FLuaScriptManager::SetOnEscapePressed(sol::protected_function Callback)
{
	OnEscapePressedCallback = std::move(Callback);
}

float FLuaScriptManager::GetRuntimeGamma()
{
	return RuntimeGamma;
}

void FLuaScriptManager::SetRuntimeGamma(float InGamma)
{
	RuntimeGamma = std::clamp(InGamma, 1.0f, 3.0f);
}

float FLuaScriptManager::GetRuntimeSaturation()
{
	return RuntimeSaturation;
}

void FLuaScriptManager::SetRuntimeSaturation(float InSaturation)
{
	RuntimeSaturation = std::clamp(InSaturation, 0.0f, 1.0f);
}

float FLuaScriptManager::GetRuntimeMouseSensitivity()
{
	return RuntimeMouseSensitivity;
}

void FLuaScriptManager::SetRuntimeMouseSensitivity(float InSensitivity)
{
	RuntimeMouseSensitivity = std::clamp(InSensitivity, 0.05f, 15.0f);
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

	FString ScriptRoot = FPaths::ToUtf8(FPaths::ScriptDir());
	for (char& Ch : ScriptRoot)
	{
		if (Ch == '\\')
		{
			Ch = '/';
		}
	}
	if (!ScriptRoot.empty() && Normalized.rfind(ScriptRoot, 0) == 0)
	{
		Normalized.erase(0, ScriptRoot.size());
	}
	while (!Normalized.empty() && Normalized.front() == '/')
	{
		Normalized.erase(Normalized.begin());
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

void FLuaScriptManager::RegisterAnimInstance(ULuaAnimInstance* Instance)
{
	if (!Instance) return;
	std::lock_guard<std::mutex> Lock(ComponentMutex);
	auto It = std::find(RegisteredAnimInstances.begin(), RegisteredAnimInstances.end(), Instance);
	if (It == RegisteredAnimInstances.end())
	{
		RegisteredAnimInstances.push_back(Instance);
	}
}

void FLuaScriptManager::UnregisterAnimInstance(ULuaAnimInstance* Instance)
{
	if (!Instance) return;
	std::lock_guard<std::mutex> Lock(ComponentMutex);
	auto It = std::find(RegisteredAnimInstances.begin(), RegisteredAnimInstances.end(), Instance);
	if (It != RegisteredAnimInstances.end())
	{
		RegisteredAnimInstances.erase(It);
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

	// AnimInstance 측도 같은 패턴 — 매칭되는 ScriptFile 의 인스턴스 reload.
	TSet<ULuaAnimInstance*> AnimTargets;
	{
		std::lock_guard<std::mutex> Lock(ComponentMutex);
		for (ULuaAnimInstance* Inst : RegisteredAnimInstances)
		{
			if (!Inst) continue;
			const FString& AnimScript = Inst->ScriptFile;
			if (AnimScript.empty()) continue;
			for (const FString& File : ChangedFiles)
			{
				if (File == AnimScript)
				{
					AnimTargets.insert(Inst);
					break;
				}
			}
		}
	}
	for (ULuaAnimInstance* Inst : AnimTargets)
	{
		if (!Inst) continue;
		UE_LOG("[LuaHotReload] Reloading Anim: %s", Inst->ScriptFile.c_str());
		FNotificationManager::Get().AddNotification("Anim Reloaded: " + Inst->ScriptFile, ENotificationType::Success, 3.0f);
		Inst->ReloadScript();
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

	// 3) Game.LuaEventBus — scene 전환 후 이전 Director 구독이 새 world event를 받지 않도록 정리.
	if (sol::object Bus = Loaded["Game.LuaEventBus"]; Bus.valid() && Bus.get_type() == sol::type::table)
	{
		sol::table BusTable = Bus.as<sol::table>();
		sol::object ClearObject = BusTable["Clear"];
		if (ClearObject.valid() && ClearObject.get_type() == sol::type::function)
		{
			sol::protected_function Clear = ClearObject.as<sol::protected_function>();
			sol::protected_function_result Result = Clear();
			if (!Result.valid())
			{
				sol::error Error = Result;
				UE_LOG("[Lua] Game.LuaEventBus.Clear error: %s", Error.what());
			}
		}
	}
}

void FLuaScriptManager::EmitGameEvent_Trigger(const FString& EventName, AActor* Trigger, APawn* Pawn, const FString& TriggerTag)
{
	if (!Lua)
	{
		return;
	}

	sol::object BusObject = RequireLuaModule(*Lua, "Game.LuaEventBus");
	if (!BusObject.valid() || BusObject.get_type() != sol::type::table)
	{
		return;
	}

	sol::table Bus = BusObject.as<sol::table>();
	sol::object EmitObject = Bus["Emit"];
	if (!EmitObject.valid() || EmitObject.get_type() != sol::type::function)
	{
		return;
	}

	sol::protected_function Emit = EmitObject.as<sol::protected_function>();
	sol::protected_function_result Result = Emit(EventName, static_cast<AActor*>(Trigger), static_cast<APawn*>(Pawn), TriggerTag);
	if (!Result.valid())
	{
		sol::error Error = Result;
		UE_LOG("[Lua] Game event '%s' error: %s", EventName.c_str(), Error.what());
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
		const TArray<FString> CandidateScriptFiles = BuildLuaModuleScriptFileCandidates(ModName);
		FString ScriptFile;
		std::wstring WidePath;
		std::error_code EC;
		FString SearchMessage;
		for (const FString& CandidateScriptFile : CandidateScriptFiles)
		{
			const std::wstring CandidateWidePath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(CandidateScriptFile));
			if (std::filesystem::exists(CandidateWidePath, EC))
			{
				ScriptFile = CandidateScriptFile;
				WidePath = CandidateWidePath;
				break;
			}

			SearchMessage += "\n\tno file '";
			SearchMessage += FPaths::ToUtf8(CandidateWidePath);
			SearchMessage += "'";
		}

		if (ScriptFile.empty())
		{
			return sol::make_object(L, SearchMessage);
		}

		FString Content;
		if (!ReadScriptFileContent(ScriptFile, Content))
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

	// 모든 sol::protected_function 호출의 default error handler 를 debug.traceback 으로 설정.
	// 이로써 lua 함수 호출 실패 시 protected_function_result 의 err.what() 에 lua 콜스택
	// (어느 파일, 어느 라인, 어느 함수) 이 포함되어 디버깅이 가능해진다. 미설정 시
	// sol2 는 단순 에러 메시지만 던져 lua 측 stack 정보가 사라진다.
	//sol::function Traceback = (*Lua)["debug"]["traceback"];
	//if (Traceback.valid())
	//{
	//	sol::protected_function::set_default_handler(Traceback);
	//}

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
	FLuaDocRegistry::Get().Reset();

	RegisterLuaHelpers(Lua);
	RegisterCoreBindings(Lua);
	RegisterMathBindings(Lua);
	RegisterActorBindings(Lua);
	RegisterUIBindings(Lua);

	RegisterGeneratedLuaBindings(Lua);

	FLuaDocRegistry::Get().Global("obj", "Actor");
	FLuaDocRegistry::Get().Global("this", "any");
	FLuaDocRegistry::Get().Global("self", "table");

	FLuaDocRegistry::Get().Type("AnimNode");
	FLuaDocRegistry::Get().Type("AnimLib")
		.Method("---@return number\nfunction Anim.get_owner_speed() end")
		.Method("---@return integer\nfunction Anim.get_owner_uuid() end")
		.Method("---@return string\nfunction Anim.get_owner_movement_mode() end")
		.Method("---@return boolean\nfunction Anim.is_owner_falling() end")
		.Method("---@return boolean\nfunction Anim.is_owner_wall_running() end")
		.Method("---@param path string\n---@param section? string\n---@param rate? number\n---@param blendIn? number\n---@param slotName? string\nfunction Anim.play_montage(path, section, rate, blendIn, slotName) end")
		.Method("---@param blendOut? number\n---@param slotName? string\nfunction Anim.stop_montage(blendOut, slotName) end")
		.Method("---@param slotName? string\n---@return boolean\nfunction Anim.is_montage_playing(slotName) end")
		.Method("---@param sectionName string\n---@param slotName? string\nfunction Anim.jump_to_section(sectionName, slotName) end")
		.Method("---@param boneName string\n---@param pitch number\n---@param yaw number\n---@param roll number\n---@param weight? number\nfunction Anim.set_bone_rotation_offset(boneName, pitch, yaw, roll, weight) end")
		.Method("function Anim.clear_bone_rotation_offsets() end")
		.Method("---@return boolean\nfunction Anim.is_left_mouse_pressed() end")
		.Method("---@return boolean\nfunction Anim.is_left_mouse_down() end")
		.Method("---@return boolean\nfunction Anim.is_right_mouse_pressed() end")
		.Method("---@param key integer\n---@return boolean\nfunction Anim.is_key_pressed(key) end")
		.Method("---@param key integer\n---@return boolean\nfunction Anim.is_key_down(key) end")
		.Method("---@param name? string\n---@return AnimNode\nfunction Anim.create_state_machine(name) end")
		.Method("---@param path string\n---@param rate number\n---@param loop boolean\n---@return AnimNode\nfunction Anim.create_sequence_player(path, rate, loop) end")
		.Method("---@param path string\n---@param enable boolean\n---@return boolean\nfunction Anim.set_sequence_force_root_lock(path, enable) end")
		.Method("---@param stateMachine AnimNode\n---@param name string\n---@param subGraph AnimNode\nfunction Anim.sm_add_state(stateMachine, name, subGraph) end")
		.Method("---@param stateMachine AnimNode\n---@param from string\n---@param to string\n---@param condition fun(): boolean\n---@param blendTime number\nfunction Anim.sm_add_transition(stateMachine, from, to, condition, blendTime) end")
		.Method("---@param stateMachine AnimNode\n---@param name string\nfunction Anim.sm_set_initial_state(stateMachine, name) end")
		.Method("---@param root AnimNode\nfunction Anim.set_root_node(root) end")
		.Method("---@param name string\n---@param input AnimNode\n---@return AnimNode\nfunction Anim.create_slot(name, input) end")
		.Method("---@return AnimNode\nfunction Anim.create_ref_pose() end")
		.Method("---@param base AnimNode\n---@param blend AnimNode\n---@param maskRootBone string\n---@return AnimNode\nfunction Anim.create_layered_blend_per_bone(base, blend, maskRootBone) end")
		.Method("---@param initialIndex? integer\n---@param blendTime? number\n---@return AnimNode\nfunction Anim.create_blend_list_by_enum(initialIndex, blendTime) end")
		.Method("---@param blendList AnimNode\n---@param pose AnimNode\nfunction Anim.blend_list_add_pose(blendList, pose) end")
		.Method("---@param blendList AnimNode\n---@param index integer\nfunction Anim.blend_list_set_active(blendList, index) end");
	FLuaDocRegistry::Get().Global("Anim", "AnimLib");

	FLuaDocRegistry::Get().GenerateStubs();
}

FInputSystemSnapshot FLuaScriptManager::GetLuaInputSnapshot()
{
	return UGameViewportClient::MakeCurrentGameInputSnapshot();
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
	{
		Input.set_function("GetKeyDown", [](int VK) { return GetLuaInputSnapshot().WasPressed(VK); });
		Input.set_function("GetKey", [](int VK) { return GetLuaInputSnapshot().IsDown(VK); });
		Input.set_function("GetKeyUp", [](int VK) { return GetLuaInputSnapshot().WasReleased(VK); });
		Input.set_function("GetAxis", [](int InputCode) { return GetLuaInputSnapshot().GetAxisValue(InputCode); });
		Input.set_function("GetGamepadAxis", [](int GamepadIndex, int AxisCode)
		{
			return GetLuaInputSnapshot().GetGamepadAxisValue(GamepadIndex, AxisCode);
		});
		Input.set_function("IsGamepadConnected", [](sol::optional<int> GamepadIndex)
		{
			return GetLuaInputSnapshot().IsGamepadConnected(GamepadIndex.value_or(-1));
		});
		Input.set_function("GetMouseDeltaX", []() { return GetLuaInputSnapshot().MouseDeltaX; });
		Input.set_function("GetMouseDeltaY", []() { return GetLuaInputSnapshot().MouseDeltaY; });
		Input.set_function("GetMouseClientX", []() { return InputSystem::Get().GetMouseClientPos().x; });
		Input.set_function("GetMouseClientY", []() { return InputSystem::Get().GetMouseClientPos().y; });
	}

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

		const float UiViewportWidth = UUIManager::Get().GetViewportWidth();
		const float UiViewportHeight = UUIManager::Get().GetViewportHeight();
		if (UiViewportWidth > 0.0f && UiViewportHeight > 0.0f)
		{
			Result["Width"] = UiViewportWidth;
			Result["Height"] = UiViewportHeight;
			return Result;
		}

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
	Engine.set_function("GetGamma", []()
	{
		return FLuaScriptManager::GetRuntimeGamma();
	});
	Engine.set_function("SetGamma", [](float Value)
	{
		FLuaScriptManager::SetRuntimeGamma(Value);
	});
	Engine.set_function("GetPostProcessSaturation", []()
	{
		return FLuaScriptManager::GetRuntimeSaturation();
	});
	Engine.set_function("SetPostProcessSaturation", [](float Value)
	{
		FLuaScriptManager::SetRuntimeSaturation(Value);
	});
	Engine.set_function("GetMouseSensitivity", []()
	{
		return FLuaScriptManager::GetRuntimeMouseSensitivity();
	});
	Engine.set_function("SetMouseSensitivity", [](float Value)
	{
		FLuaScriptManager::SetRuntimeMouseSensitivity(Value);
	});
	Engine.set_function("Exit", []()
	{
		if (GEngine)
		{
			GEngine->RequestExit();
		}
	});
	Engine.set_function("TransitionToScene", [](const FString& ScenePath)
	{
		if (GEngine)
		{
			GEngine->RequestTransitionToScene(ScenePath);
		}
	});
	Engine.set_function("SetOnEscape", [](sol::protected_function Callback)
	{
		FLuaScriptManager::SetOnEscapePressed(std::move(Callback));
	});

	// Game — gameplay framework 공용 진입점. 실제 게임 규칙은 Lua Director/EventBus가 담당합니다.
	sol::table Game = Lua.create_named_table("Game");
	Game.set_function("GetPlayerController", []() -> APlayerController*
	{
		return (GEngine && GEngine->GetWorld()) ? GEngine->GetWorld()->GetFirstPlayerController() : nullptr;
	});
	Game.set_function("GetPlayerPawn", []() -> APawn*
	{
		APlayerController* PC = (GEngine && GEngine->GetWorld()) ? GEngine->GetWorld()->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPossessedPawn() : nullptr;
	});
	Game.set_function("PossessPawnByName", [](const FString& PawnName) -> bool
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return false;
		}

		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		if (!PC)
		{
			return false;
		}

		for (AActor* Actor : GEngine->GetWorld()->GetActors())
		{
			if (!Actor || Actor->GetFName().ToString() != PawnName)
			{
				continue;
			}

			// Lua의 World.FindActorByName은 Actor로 반환되므로, Pawn 전환은 C++에서 안전하게 캐스팅합니다.
			APawn* Pawn = Cast<APawn>(Actor);
			if (!Pawn)
			{
				return false;
			}

			PC->Possess(Pawn);
			return true;
		}

		return false;
	});
	Game.set_function("GetCameraManager", []() -> APlayerCameraManager*
	{
		APlayerController* PC = (GEngine && GEngine->GetWorld()) ? GEngine->GetWorld()->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPlayerCameraManager() : nullptr;
	});
	Game.set_function("GetCurrentSceneName", []() -> FString
	{
		if (!GEngine)
		{
			return {};
		}

		const FWorldContext* Context = GEngine->GetWorldContextFromHandle(GEngine->GetActiveWorldHandle());
		if (Context && !Context->ContextName.empty() && Context->ContextName != "PIE")
		{
			return Context->ContextName;
		}
		return FProjectSettings::Get().Game.StartLevelName;
	});
	Game.set_function("TransitionToScene", [](const FString& ScenePath)
	{
		if (GEngine)
		{
			GEngine->RequestTransitionToScene(ScenePath);
		}
	});
	Game.set_function("Pause", []()
	{
		if (GEngine)
		{
			if (UWorld* World = GEngine->GetWorld())
			{
				World->SetPaused(true);
			}
		}
	});
	Game.set_function("Resume", []()
	{
		if (GEngine)
		{
			if (UWorld* World = GEngine->GetWorld())
			{
				World->SetPaused(false);
			}
		}
	});
	Game.set_function("IsPaused", []()
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
	Game.set_function("Log", [](sol::variadic_args Args)
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
		UE_LOG("[Game] %s", Message.c_str());
	});

	sol::protected_function_result GameAliasResult = Lua.safe_script(R"(
Game = Game or {}
local function __GameEventBus()
    return require("Game.LuaEventBus")
end
function Game.On(...)
    return __GameEventBus().On(...)
end
function Game.Off(...)
    return __GameEventBus().Off(...)
end
function Game.Emit(...)
    return __GameEventBus().Emit(...)
end
function Game.RestartLevel()
    local sceneName = Game.GetCurrentSceneName()
    if sceneName ~= nil and sceneName ~= "" then
        Game.TransitionToScene(sceneName)
    end
end
Engine.TransitionToScene = Game.TransitionToScene
Engine.PauseGame = Game.Pause
Engine.ResumeGame = Game.Resume
Engine.IsPaused = Game.IsPaused
)");
	if (!GameAliasResult.valid())
	{
		sol::error Error = GameAliasResult;
		UE_LOG("[Lua] Game namespace bootstrap error: %s", Error.what());
	}

	sol::table Key = Lua.create_named_table("Key");
	Key["W"] = static_cast<int32>('W');
	Key["A"] = static_cast<int32>('A');
	Key["S"] = static_cast<int32>('S');
	Key["D"] = static_cast<int32>('D');
	Key["Q"] = static_cast<int32>('Q');
	Key["E"] = static_cast<int32>('E');
	Key["R"] = static_cast<int32>('R');
	Key["Space"] = VK_SPACE;
	Key["Enter"] = VK_RETURN;
	Key["Ctrl"] = VK_CONTROL;
	Key["Control"] = VK_CONTROL;
	Key["LeftCtrl"] = VK_LCONTROL;
	Key["RightCtrl"] = VK_RCONTROL;
	Key["Shift"] = VK_SHIFT;
	Key["LeftShift"] = VK_LSHIFT;
	Key["RightShift"] = VK_RSHIFT;
	Key["Escape"] = VK_ESCAPE;
	Key["F1"] = VK_F1;
	Key["F2"] = VK_F2;
	Key["F3"] = VK_F3;
	Key["F4"] = VK_F4;
	Key["F5"] = VK_F5;
	Key["F6"] = VK_F6;
	Key["F7"] = VK_F7;
	Key["F8"] = VK_F8;
	Key["F9"] = VK_F9;
	Key["F10"] = VK_F10;
	Key["F11"] = VK_F11;
	Key["F12"] = VK_F12;
	Key["MouseLeft"] = VK_LBUTTON;
	Key["MouseRight"] = VK_RBUTTON;
	Key["MouseMiddle"] = VK_MBUTTON;
	Key["MouseXButton1"] = VK_XBUTTON1;
	Key["MouseXButton2"] = VK_XBUTTON2;
	Key["GamepadA"] = InputCodes::GamepadA;
	Key["GamepadB"] = InputCodes::GamepadB;
	Key["GamepadX"] = InputCodes::GamepadX;
	Key["GamepadY"] = InputCodes::GamepadY;
	Key["GamepadLeftShoulder"] = InputCodes::GamepadLeftShoulder;
	Key["GamepadRightShoulder"] = InputCodes::GamepadRightShoulder;
	Key["GamepadBack"] = InputCodes::GamepadBack;
	Key["GamepadStart"] = InputCodes::GamepadStart;
	Key["GamepadLeftThumb"] = InputCodes::GamepadLeftThumb;
	Key["GamepadRightThumb"] = InputCodes::GamepadRightThumb;
	Key["GamepadDPadUp"] = InputCodes::GamepadDPadUp;
	Key["GamepadDPadDown"] = InputCodes::GamepadDPadDown;
	Key["GamepadDPadLeft"] = InputCodes::GamepadDPadLeft;
	Key["GamepadDPadRight"] = InputCodes::GamepadDPadRight;
	Key["GamepadLeftTrigger"] = InputCodes::GamepadLeftTrigger;
	Key["GamepadRightTrigger"] = InputCodes::GamepadRightTrigger;

	sol::table Axis = Lua.create_named_table("Axis");
	Axis["GamepadLeftX"] = InputCodes::GamepadLeftX;
	Axis["GamepadLeftY"] = InputCodes::GamepadLeftY;
	Axis["GamepadRightX"] = InputCodes::GamepadRightX;
	Axis["GamepadRightY"] = InputCodes::GamepadRightY;
	Axis["GamepadLeftTrigger"] = InputCodes::GamepadLeftTriggerAxis;
	Axis["GamepadRightTrigger"] = InputCodes::GamepadRightTriggerAxis;

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
	CameraManager.set_function("SetViewTargetWithBlend", [](AActor* Target, float BlendTime)
	{
		if (!GEngine || !GEngine->GetWorld() || !Target) return;

		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			PC->SetViewTargetWithBlend(Target, BlendTime);
		}
	});
	// ActiveCamera 컴포넌트 단위 blend — 같은 액터 내 1인칭/3인칭 같은 별개 카메라
	// 컴포넌트 사이 부드럽게 전환. BlendTime 미지정 시 0 (즉시 swap).
	CameraManager.set_function("SetActiveCameraWithBlend", [](UCameraComponent* NewCamera, sol::optional<float> BlendTime)
	{
		if (!GEngine || !GEngine->GetWorld() || !NewCamera) return;
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (Manager)
		{
			Manager->SetActiveCameraWithBlend(NewCamera, BlendTime.value_or(0.0f));
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
	CameraManager.set_function("StartCameraShakeAsset", [](const FString& AssetPath, sol::optional<float> Scale)
	{
		if (!GEngine || !GEngine->GetWorld()) return;
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (Manager)
		{
			Manager->StartCameraShakeAsset(AssetPath, Scale.value_or(1.0f));
		}
	});
	CameraManager.set_function("StopAllCameraShakes", [](sol::optional<bool> bImmediately)
	{
		if (!GEngine || !GEngine->GetWorld()) return;
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		APlayerCameraManager* Manager = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (Manager)
		{
			// 컷씬 스킵/카메라 전환 시 남아 있는 흔들림을 즉시 정리하기 위한 Lua 진입점입니다.
			Manager->StopAllCameraShakes(bImmediately.value_or(true));
		}
	});

	sol::table AudioManager = Lua.create_named_table("AudioManager");
	AudioManager.set_function("Load", [](const FString& SoundName, const FString& Path, sol::optional<bool> bLoop)
	{
		return FAudioManager::Get().LoadAudio(SoundName, Path, bLoop.value_or(false));
	});
	AudioManager.set_function("Play", [](const FString& SoundName, float Volume)
	{
		return FAudioManager::Get().PlayAudio(SoundName, Volume);
	});
	AudioManager.set_function("Stop", [](const FString& SoundName)
	{
		return FAudioManager::Get().StopAudio(SoundName);
	});
	AudioManager.set_function("PlayOneShot", [](const FString& EventName)
	{
		return FAudioManager::Get().PlayOneShot(EventName);
	});
	AudioManager.set_function("PlayOneShotAt", [](const FString& EventName, float X, float Y, float Z)
	{
		return FAudioManager::Get().PlayOneShotAt(EventName, FVector(X, Y, Z));
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
		return FAudioManager::Get().PlayLoop(SoundName, LoopName, Volume.value_or(1.0f), Pitch.value_or(1.0f));
	});
	AudioManager.set_function("SetLoopState", [](const FString& LoopName, const FString& SoundName, bool bShouldPlay, sol::optional<float> Volume, sol::optional<float> Pitch)
	{
		return FAudioManager::Get().SetLoopState(LoopName, SoundName, bShouldPlay, Volume.value_or(1.0f), Pitch.value_or(1.0f));
	});
	AudioManager.set_function("StopLoop", [](const FString& LoopName)
	{
		FAudioManager::Get().StopLoop(LoopName);
	});
	AudioManager.set_function("FadeOutLoop", [](const FString& LoopName, sol::optional<float> FadeMilliseconds)
	{
		FAudioManager::Get().FadeOutLoop(LoopName, FadeMilliseconds.value_or(200.0f));
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
	AudioManager.set_function("PlayEvent", [](const FString& EventName)
	{
		return FAudioManager::Get().PlayEvent(EventName);
	});
	AudioManager.set_function("PlayEventAt", [](const FString& EventName, float X, float Y, float Z)
	{
		return FAudioManager::Get().PlayEventAt(EventName, FVector(X, Y, Z));
	});
	AudioManager.set_function("StopEvent", [](const FString& EventName)
	{
		FAudioManager::Get().StopEvent(EventName);
	});
	AudioManager.set_function("FadeOutEvent", [](const FString& EventName, sol::optional<float> FadeMilliseconds)
	{
		FAudioManager::Get().FadeOutEvent(EventName, FadeMilliseconds.value_or(120.0f));
	});
	AudioManager.set_function("SetBusVolume", [](const FString& BusName, float Volume)
	{
		FAudioManager::Get().SetBusVolume(BusName, Volume);
	});
	AudioManager.set_function("GetBusVolume", [](const FString& BusName)
	{
		return FAudioManager::Get().GetBusVolume(BusName);
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

	FLuaDocRegistry::Get().Type("Vector")
		.Property("X", "number")
		.Property("Y", "number")
		.Property("Z", "number")
		.Method("---@param x? number\n---@param y? number\n---@param z? number\n---@return Vector\nfunction Vector.new(x, y, z) end")
		.Method("---@return number\nfunction Vector:Length() end")
		.Method("---@return nil\nfunction Vector:Normalize() end")
		.Method("---@return Vector\nfunction Vector:Normalized() end")
		.Method("---@param other Vector\n---@return number\nfunction Vector:Dot(other) end")
		.Method("---@param other Vector\n---@return Vector\nfunction Vector:Cross(other) end")
		.Method("---@param a Vector\n---@param b Vector\n---@return number\nfunction Vector.Distance(a, b) end")
		.Method("---@param a Vector\n---@param b Vector\n---@return number\nfunction Vector.DistSquared(a, b) end")
		.Method("---@param a Vector\n---@param b Vector\n---@param alpha number\n---@return Vector\nfunction Vector.Lerp(a, b, alpha) end")
		.Method("---@return Vector\nfunction Vector.Zero() end")
		.Method("---@return Vector\nfunction Vector.One() end")
		.Method("---@return Vector\nfunction Vector.Up() end")
		.Method("---@return Vector\nfunction Vector.Forward() end")
		.Method("---@return Vector\nfunction Vector.Right() end");

	Lua.new_usertype<FTransform>("Transform",
		sol::constructors<FTransform()>(),
		"Location", &FTransform::Location,
		"Rotation", sol::property(
			[](const FTransform& Transform)
	{
		return Transform.GetRotator().ToVector();
	},
			[](FTransform& Transform, const FVector& Rotation)
	{
		Transform.SetRotation(FRotator(Rotation));
	}
		),
		"Scale", &FTransform::Scale,
		"New", [](const FVector& Location, const FVector& Rotation, const FVector& Scale)
	{
		return FTransform(Location, Rotation, Scale);
	},
		"Identity", []()
	{
		return FTransform();
	});

	FLuaDocRegistry::Get().Type("Transform")
		.Property("Location", "Vector")
		.Property("Rotation", "Vector")
		.Property("Scale", "Vector")
		.Method("---@return Transform\nfunction Transform.new() end")
		.Method("---@param location Vector\n---@param rotation Vector\n---@param scale Vector\n---@return Transform\nfunction Transform.New(location, rotation, scale) end")
		.Method("---@return Transform\nfunction Transform.Identity() end");
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

	FLuaDocRegistry::Get().Type("ActionComponent")
		.Method("---@param duration number\nfunction ActionComponent:HitStop(duration) end")
		.Method("---@param direction Vector\n---@param distance number\n---@param duration number\nfunction ActionComponent:Knockback(direction, distance, duration) end")
		.Method("function ActionComponent:StopAllActions() end");

	Lua.new_usertype<UFloatingPawnMovementComponent>("FloatingPawnMovementComponent",
		"SetMoveInput", &UFloatingPawnMovementComponent::SetMoveInput,
		"SetLookInput", &UFloatingPawnMovementComponent::SetLookInput);

	FLuaDocRegistry::Get().Type("FloatingPawnMovementComponent")
		.Method("---@param input Vector\nfunction FloatingPawnMovementComponent:SetMoveInput(input) end")
		.Method("---@param input Vector\nfunction FloatingPawnMovementComponent:SetLookInput(input) end");

	Lua.new_usertype<UCharacterMovementComponent>("CharacterMovementComponent",
		"GetVelocity", [](UCharacterMovementComponent& Component)
		{
			return Component.GetVelocity();
		},
		"GetSpeed", [](UCharacterMovementComponent& Component)
		{
			return Component.GetSpeed();
		},
		"IsWalking", [](UCharacterMovementComponent& Component)
		{
			return Component.IsWalking();
		},
		"IsFalling", [](UCharacterMovementComponent& Component)
		{
			return Component.IsFalling();
		},
		"IsWallRunning", [](UCharacterMovementComponent& Component)
		{
			return Component.IsWallRunning();
		},
		"WasAirJumpConsumedThisFrame", [](UCharacterMovementComponent& Component)
		{
			return Component.WasAirJumpConsumedThisFrame();
		},
		"IsSprinting", [](UCharacterMovementComponent& Component)
		{
			return Component.IsSprinting();
		},
		"IsCrouching", [](UCharacterMovementComponent& Component)
		{
			return Component.IsCrouching();
		},
		"GetMovementModeName", [](UCharacterMovementComponent& Component)
		{
			return FString(Component.GetMovementModeName());
		},
		"GetMaxWalkSpeed", [](UCharacterMovementComponent& Component)
		{
			return Component.MaxWalkSpeed;
		},
		"SetMaxWalkSpeed", [](UCharacterMovementComponent& Component, float Value)
		{
			Component.MaxWalkSpeed = (std::max)(0.0f, Value);
		},
		"GetSprintSpeedMultiplier", [](UCharacterMovementComponent& Component)
		{
			return Component.SprintSpeedMultiplier;
		},
		"SetSprintSpeedMultiplier", [](UCharacterMovementComponent& Component, float Value)
		{
			Component.SprintSpeedMultiplier = (std::max)(0.0f, Value);
		},
		"GetWallRunMaxSpeed", [](UCharacterMovementComponent& Component)
		{
			return Component.WallRunMaxSpeed;
		},
		"SetWallRunMaxSpeed", [](UCharacterMovementComponent& Component, float Value)
		{
			Component.WallRunMaxSpeed = (std::max)(0.0f, Value);
		},
		"IsCrouching", [](UCharacterMovementComponent& Component)
		{
			return Component.IsCrouching();
		});

	FLuaDocRegistry::Get().Type("CharacterMovementComponent")
		.Method("---@return Vector\nfunction CharacterMovementComponent:GetVelocity() end")
		.Method("---@return number\nfunction CharacterMovementComponent:GetSpeed() end")
		.Method("---@return boolean\nfunction CharacterMovementComponent:IsWalking() end")
		.Method("---@return boolean\nfunction CharacterMovementComponent:IsFalling() end")
		.Method("---@return boolean\nfunction CharacterMovementComponent:IsWallRunning() end")
		.Method("---@return boolean\nfunction CharacterMovementComponent:WasAirJumpConsumedThisFrame() end")
		.Method("---@return boolean\nfunction CharacterMovementComponent:IsSprinting() end")
		.Method("---@return boolean\nfunction CharacterMovementComponent:IsCrouching() end")
		.Method("---@return string\nfunction CharacterMovementComponent:GetMovementModeName() end")
		.Method("---@return number\nfunction CharacterMovementComponent:GetMaxWalkSpeed() end")
		.Method("---@param value number\nfunction CharacterMovementComponent:SetMaxWalkSpeed(value) end")
		.Method("---@return number\nfunction CharacterMovementComponent:GetSprintSpeedMultiplier() end")
		.Method("---@param value number\nfunction CharacterMovementComponent:SetSprintSpeedMultiplier(value) end")
		.Method("---@return number\nfunction CharacterMovementComponent:GetWallRunMaxSpeed() end")
		.Method("---@param value number\nfunction CharacterMovementComponent:SetWallRunMaxSpeed(value) end")
		.Method("---@return boolean\nfunction CharacterMovementComponent:IsCrouching() end");

	Lua.new_usertype<UVehicleMovementComponent4W>("VehicleMovementComponent4W",
		"SetDriveInput", &UVehicleMovementComponent4W::SetDriveInput);
	FLuaDocRegistry::Get().Type("VehicleMovementComponent4W")
		.Method("---@param throttle number\n---@param brake number\n---@param steer number\n---@param reverse boolean\nfunction VehicleMovementComponent4W:SetDriveInput(throttle, brake, steer, reverse) end");

	Lua.new_usertype<UVehicleMovementComponentTank>("VehicleMovementComponentTank");

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
		"AddLocalRotation", [](USceneComponent& Component, const FVector& DeltaRotation)
	{
		// Lua uses Vector(X=Roll, Y=Pitch, Z=Yaw) in degrees. Convert only the
		// per-shot delta to a quat, then compose it inside the component.
		// This avoids the GetRotation -> Euler add -> SetRotation accumulation path.
		Component.AddLocalRotation(FRotator(DeltaRotation));
	},

		// 부모 기준 상대 위치 — 동일한 메시를 4개 깐 바퀴 같은 케이스에서 앞/뒤 구분 등
		// 위치 기반 필터링에 쓰인다. 월드 위치는 위 "Location" 프로퍼티 참고.
		"RelativeLocation", sol::property(
		[](USceneComponent& Component) { return Component.GetRelativeLocation(); },
		[](USceneComponent& Component, const FVector& V) { Component.SetRelativeLocation(V); }
		),
		"RelativeScale", sol::property(
		[](USceneComponent& Component) { return Component.GetRelativeScale(); },
		[](USceneComponent& Component, const FVector& V) { Component.SetRelativeScale(V); }
		),
		"GetRelativeScale", [](USceneComponent& Component)
	{
		return Component.GetRelativeScale();
	},
		"SetRelativeScale", [](USceneComponent& Component, const FVector& Scale)
	{
		Component.SetRelativeScale(Scale);
	}
	);

	FLuaDocRegistry::Get().Type("SceneComponent")
		.Property("Location", "Vector")
		.Property("Rotation", "Vector")
		.Property("RelativeLocation", "Vector")
		.Property("RelativeScale", "Vector")
		.Property("Forward", "Vector")
		.Property("Right", "Vector")
		.Property("Up", "Vector")
		.Method("---@return Vector\nfunction SceneComponent:GetLocation() end")
		.Method("---@param location Vector\nfunction SceneComponent:SetLocation(location) end")
		.Method("---@return Vector\nfunction SceneComponent:GetRotation() end")
		.Method("---@param rotation Vector\nfunction SceneComponent:SetRotation(rotation) end")
		.Method("---@return Vector\nfunction SceneComponent:GetRelativeScale() end")
		.Method("---@param scale Vector\nfunction SceneComponent:SetRelativeScale(scale) end")
		.Method("---@param deltaRotation Vector # Vector(X=Roll, Y=Pitch, Z=Yaw), degrees; internally composed as quaternion.\nfunction SceneComponent:AddLocalRotation(deltaRotation) end");

	Lua.new_usertype<ULightComponentBase>("LightComponentBase",
		sol::base_classes, sol::bases<USceneComponent>(),
		"GetIntensity", &ULightComponentBase::GetIntensity,
		"SetIntensity", &ULightComponentBase::SetIntensity);

	FLuaDocRegistry::Get().Type("LightComponentBase", "SceneComponent")
		.Method("---@return number\nfunction LightComponentBase:GetIntensity() end")
		.Method("---@param value number\nfunction LightComponentBase:SetIntensity(value) end");

	Lua.new_usertype<UPointLightComponent>("PointLightComponent",
		sol::base_classes, sol::bases<ULightComponentBase, USceneComponent>(),
		"GetAttenuationRadius", &UPointLightComponent::GetAttenuationRadius,
		"SetAttenuationRadius", &UPointLightComponent::SetAttenuationRadius);

	FLuaDocRegistry::Get().Type("PointLightComponent", "LightComponentBase")
		.Method("---@return number\nfunction PointLightComponent:GetAttenuationRadius() end")
		.Method("---@param value number\nfunction PointLightComponent:SetAttenuationRadius(value) end");

	Lua.new_usertype<USpotLightComponent>("SpotLightComponent",
		sol::base_classes, sol::bases<UPointLightComponent, ULightComponentBase, USceneComponent>(),
		"GetOuterConeAngle", &USpotLightComponent::GetOuterConeAngle);

	FLuaDocRegistry::Get().Type("SpotLightComponent", "PointLightComponent")
		.Method("---@return number\nfunction SpotLightComponent:GetOuterConeAngle() end");

	Lua.new_usertype<UPrimitiveComponent>("PrimitiveComponent",
		sol::base_classes, sol::bases<USceneComponent>(),
		"SetOutline", [](UPrimitiveComponent& Component, bool bEnabled)
		{
			AActor* OwnerActor = Component.GetOwner();
			UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
			if (!World)
			{
				return;
			}
			World->GetScene().SetProxyOutlineOnly(Component.GetSceneProxy(), bEnabled);
		});

	FLuaDocRegistry::Get().Type("PrimitiveComponent", "SceneComponent")
		.Method("---@param enabled boolean\nfunction PrimitiveComponent:SetOutline(enabled) end");

	// 메시 에셋 경로로 컴포넌트 식별 가능하게 노출. 자동 생성된 FName ("UStaticMeshComponent_41")
	// 은 월드 초기화 순서에 따라 카운터가 달라져 빌드별로 매칭이 깨질 수 있다. 메시 경로는
	// 씬 파일에 명시 저장되므로 deterministic.
	Lua.new_usertype<UStaticMeshComponent>("StaticMeshComponent",
		sol::base_classes, sol::bases<UPrimitiveComponent, USceneComponent>(),
		"MeshPath", sol::property([](UStaticMeshComponent& C) { return C.GetStaticMeshPath(); }),
		"GetMeshPath", [](UStaticMeshComponent& C) { return C.GetStaticMeshPath(); });

	FLuaDocRegistry::Get().Type("StaticMeshComponent", "PrimitiveComponent")
		.Property("MeshPath", "string")
		.Method("---@return string\nfunction StaticMeshComponent:GetMeshPath() end");

	Lua.new_usertype<UParticleSystemComponent>("ParticleSystemComponent",
		sol::base_classes, sol::bases<UPrimitiveComponent, USceneComponent>(),
		"SetVectorParameter", [](UParticleSystemComponent& Component, const FString& ParameterName, const FVector& Value)
	{
		Component.SetVectorParameter(ParameterName, Value);
	},
		"Activate", &UParticleSystemComponent::Activate,
		"Deactivate", &UParticleSystemComponent::Deactivate,
		"ResetSystem", &UParticleSystemComponent::ResetSystem,
		"SetParticleScaleMultiplier", &UParticleSystemComponent::SetParticleScaleMultiplier,
		"GetParticleScaleMultiplier", &UParticleSystemComponent::GetParticleScaleMultiplier,
		"SetEmitterSpawningEnabled", &UParticleSystemComponent::SetEmitterSpawningEnabled);

	FLuaDocRegistry::Get().Type("ParticleSystemComponent", "PrimitiveComponent")
		.Method("---@param parameterName string\n---@param value Vector\nfunction ParticleSystemComponent:SetVectorParameter(parameterName, value) end")
		.Method("function ParticleSystemComponent:Activate() end")
		.Method("function ParticleSystemComponent:Deactivate() end")
		.Method("function ParticleSystemComponent:ResetSystem() end")
		.Method("---@param scale number\nfunction ParticleSystemComponent:SetParticleScaleMultiplier(scale) end")
		.Method("---@return number\nfunction ParticleSystemComponent:GetParticleScaleMultiplier() end")
		.Method("---@param enabled boolean\nfunction ParticleSystemComponent:SetEmitterSpawningEnabled(enabled) end");

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

	FLuaDocRegistry::Get().Type("HitResult")
		.Property("HitComponent", "PrimitiveComponent?")
		.Property("HitActor", "Actor?")
		.Property("Distance", "number")
		.Property("PenetrationDepth", "number")
		.Property("WorldHitLocation", "Vector")
		.Property("WorldNormal", "Vector")
		.Property("ImpactNormal", "Vector")
		.Property("FaceIndex", "integer")
		.Property("bHit", "boolean");

	Lua.new_usertype<UCameraComponent>("CameraComponent",
		sol::base_classes, sol::bases<USceneComponent>(),

		"GetFOV", [](UCameraComponent& C) { return C.GetFOV(); },
		"SetFOV", [](UCameraComponent& C, float InFOV) { C.SetFOV(InFOV); },

		"SetDOFEnabled", [](UCameraComponent& C, bool bEnabled)
		{
			C.GetMutableDepthOfFieldSettings().bEnabled = bEnabled;
		},
		"GetDOFEnabled", [](UCameraComponent& C)
		{
			return C.GetDepthOfFieldSettings().bEnabled;
		},
		"SetDOFFocusDistance", [](UCameraComponent& C, float Distance)
		{
			C.GetMutableDepthOfFieldSettings().FocusDistance = Distance;
		},
		"GetDOFFocusDistance", [](UCameraComponent& C)
		{
			return C.GetDepthOfFieldSettings().FocusDistance;
		},
		"SetDOFFStop", [](UCameraComponent& C, float FStop)
		{
			C.GetMutableDepthOfFieldSettings().FStop = FStop;
		},
		"GetDOFFStop", [](UCameraComponent& C)
		{
			return C.GetDepthOfFieldSettings().FStop;
		},
		"ProjectWorldToScreen", [](UCameraComponent& C, const FVector& WorldLocation) -> sol::table
		{
			sol::table Result = FLuaScriptManager::GetState().create_table();
			Result["Valid"] = false;
			Result["InFront"] = false;
			Result["X"] = 0.0f;
			Result["Y"] = 0.0f;
			Result["NdcX"] = 0.0f;
			Result["NdcY"] = 0.0f;
			Result["NdcZ"] = 0.0f;
			Result["Depth"] = 0.0f;
			Result["ViewportWidth"] = 0.0f;
			Result["ViewportHeight"] = 0.0f;

			float ViewportWidth = UUIManager::Get().GetViewportWidth();
			float ViewportHeight = UUIManager::Get().GetViewportHeight();
			if ((ViewportWidth <= 0.0f || ViewportHeight <= 0.0f) && GEngine)
			{
				if (FWindowsWindow* Window = GEngine->GetWindow())
				{
					ViewportWidth = static_cast<float>(Window->GetWidth());
					ViewportHeight = static_cast<float>(Window->GetHeight());
				}
			}

			Result["ViewportWidth"] = ViewportWidth;
			Result["ViewportHeight"] = ViewportHeight;
			if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
			{
				return Result;
			}

			FMinimalViewInfo POV;
			C.GetCameraView(0.0f, POV);

			const FVector ToPoint = WorldLocation - POV.Location;
			const float Depth = ToPoint.Dot(POV.Rotation.GetForwardVector());
			const FVector Ndc = POV.CalculateViewProjectionMatrix().TransformPositionWithW(WorldLocation);

			const bool bFinite =
				std::isfinite(Ndc.X) && std::isfinite(Ndc.Y) && std::isfinite(Ndc.Z) &&
				std::isfinite(Depth);
			const bool bInFront = Depth > POV.NearClip;

			Result["Valid"] = bFinite && bInFront;
			Result["InFront"] = bInFront;
			Result["NdcX"] = Ndc.X;
			Result["NdcY"] = Ndc.Y;
			Result["NdcZ"] = Ndc.Z;
			Result["Depth"] = Depth;
			Result["X"] = (Ndc.X * 0.5f + 0.5f) * ViewportWidth;
			Result["Y"] = (1.0f - (Ndc.Y * 0.5f + 0.5f)) * ViewportHeight;
			return Result;
		}
	);

	FLuaDocRegistry::Get().Type("CameraComponent", "SceneComponent")
		.Method("---@return number\nfunction CameraComponent:GetFOV() end")
		.Method("---@param fov number # radians\nfunction CameraComponent:SetFOV(fov) end")
		.Method("---@param enabled boolean\nfunction CameraComponent:SetDOFEnabled(enabled) end")
		.Method("---@return boolean\nfunction CameraComponent:GetDOFEnabled() end")
		.Method("---@param distance number\nfunction CameraComponent:SetDOFFocusDistance(distance) end")
		.Method("---@return number\nfunction CameraComponent:GetDOFFocusDistance() end")
		.Method("---@param fstop number\nfunction CameraComponent:SetDOFFStop(fstop) end")
		.Method("---@return number\nfunction CameraComponent:GetDOFFStop() end")
		.Method("---@param worldLocation Vector\n---@return table # { Valid:boolean, InFront:boolean, X:number, Y:number, NdcX:number, NdcY:number, NdcZ:number, Depth:number, ViewportWidth:number, ViewportHeight:number }\nfunction CameraComponent:ProjectWorldToScreen(worldLocation) end");

	Lua.new_usertype<USkinnedMeshComponent>("SkinnedMeshComponent",
		sol::base_classes, sol::bases<UPrimitiveComponent, USceneComponent>());

	FLuaDocRegistry::Get().Type("SkinnedMeshComponent", "PrimitiveComponent");

	auto Actor = FLuaDocRegistry::Get().BindType<AActor>(Lua, "Actor");
	Actor
		.ReadonlyProperty("UUID", "integer", [](AActor& Actor) { return Actor.GetUUID(); })
		.ReadonlyProperty("Name", "string", [](AActor& Actor) { return Actor.GetFName().ToString(); })
		.Property("Location", "Vector",
			[](AActor& Actor) { return Actor.GetActorLocation(); },
			[](AActor& Actor, const FVector& Location) { Actor.SetActorLocation(Location); })
		.Property("Rotation", "Vector",
			[](AActor& Actor) { return Actor.GetActorRotation().ToVector(); },
			[](AActor& Actor, const FVector& Rotation) { Actor.SetActorRotation(FRotator(Rotation)); })
		.Property("Scale", "Vector",
			[](AActor& Actor) { return Actor.GetActorScale(); },
			[](AActor& Actor, const FVector& Scale) { Actor.SetActorScale(Scale); })
		.ReadonlyProperty("Forward", "Vector", [](AActor& Actor) { return Actor.GetActorForward(); })
		.ReadonlyProperty("Right", "Vector", [](AActor& Actor) { return Actor.GetActorRight(); })
		.Method("AddWorldOffset",
			"---@param offset Vector\nfunction Actor:AddWorldOffset(offset) end",
			[](AActor& Actor, const FVector& Offset) { Actor.AddActorWorldOffset(Offset); })
		.Method("Destroy",
			"function Actor:Destroy() end",
			[](AActor& Actor)
			{
				if (UWorld* W = Actor.GetWorld())
				{
					W->DestroyActor(&Actor);
				}
			})
		.Method("IsValid",
			"---@return boolean\nfunction Actor:IsValid() end",
			[](AActor* Actor) { return Actor != nullptr && IsAliveObject(Actor); })
		.Method("GetFloatingPawnMovement",
			"---@return FloatingPawnMovementComponent?\nfunction Actor:GetFloatingPawnMovement() end",
			[](AActor& Actor) { return Actor.GetComponentByClass<UFloatingPawnMovementComponent>(); })
		.Method("GetCharacterMovement",
			"---@return CharacterMovementComponent?\nfunction Actor:GetCharacterMovement() end",
			[](AActor& Actor) { return Actor.GetComponentByClass<UCharacterMovementComponent>(); })
		.Method("GetCharacterAutoInputWASD",
			"---@return boolean\nfunction Actor:GetCharacterAutoInputWASD() end",
			[](AActor& Actor)
			{
				ACharacter* Character = Cast<ACharacter>(&Actor);
				return Character ? Character->bAutoInputWASD : false;
			})
		.Method("GetCharacterAutoInputMouseLook",
			"---@return boolean\nfunction Actor:GetCharacterAutoInputMouseLook() end",
			[](AActor& Actor)
			{
				ACharacter* Character = Cast<ACharacter>(&Actor);
				return Character ? Character->bAutoInputMouseLook : false;
			})
		.Method("SetCharacterAutoInput",
			"---@param wasd boolean\n---@param mouseLook boolean\n---@return boolean\nfunction Actor:SetCharacterAutoInput(wasd, mouseLook) end",
			[](AActor& Actor, bool bWASD, bool bMouseLook)
			{
				ACharacter* Character = Cast<ACharacter>(&Actor);
				if (!Character)
				{
					return false;
				}

				// 컷씬 중 배경 Pawn이 입력을 소비하지 않도록 자동 입력 처리만 임시로 전환합니다.
				Character->bAutoInputWASD = bWASD;
				Character->bAutoInputMouseLook = bMouseLook;
				return true;
			})
		.Method("GetVehicleMovement",
			"---@return VehicleMovementComponent4W?\nfunction Actor:GetVehicleMovement() end",
			[](AActor& Actor) { return Actor.GetComponentByClass<UVehicleMovementComponent4W>(); })
		.Method("GetTankVehicleMovement",
			"---@return VehicleMovementComponentTank?\nfunction Actor:GetTankVehicleMovement() end",
			[](AActor& Actor) { return Actor.GetComponentByClass<UVehicleMovementComponentTank>(); })
		.Method("GetCamera",
			"---@return CameraComponent?\nfunction Actor:GetCamera() end",
			[](AActor& Actor) { return Actor.GetComponentByClass<UCameraComponent>(); })
		.Method("GetActionComponent",
			"---@return ActionComponent?\nfunction Actor:GetActionComponent() end",
			[](AActor& Actor) { return Actor.GetComponentByClass<UActionComponent>(); })
		.Method("GetInputComponent",
			"---@return InputComponent?\nfunction Actor:GetInputComponent() end",
			[](AActor& Actor) { return Actor.GetComponentByClass<UInputComponent>(); })
		.Method("GetRootPrimitiveComponent",
			"---@return PrimitiveComponent?\nfunction Actor:GetRootPrimitiveComponent() end",
			[](AActor& Actor) -> UPrimitiveComponent* { return Cast<UPrimitiveComponent>(Actor.GetRootComponent()); })
		.Method("GetPrimitiveComponent",
			"---@return PrimitiveComponent?\nfunction Actor:GetPrimitiveComponent() end",
			[](AActor& Actor) -> UPrimitiveComponent* { return Actor.GetComponentByClass<UPrimitiveComponent>(); })
		.Method("GetSkeletalMesh",
			"---@return SkeletalMeshComponent?\nfunction Actor:GetSkeletalMesh() end",
			[](AActor& Actor) -> USkeletalMeshComponent* { return Actor.GetComponentByClass<USkeletalMeshComponent>(); })
		.Method("GetStaticMesh",
			"---@return StaticMeshComponent?\nfunction Actor:GetStaticMesh() end",
			[](AActor& Actor) -> UStaticMeshComponent* { return Actor.GetComponentByClass<UStaticMeshComponent>(); })
		.Method("GetParticleSystem",
			"---@return ParticleSystemComponent?\nfunction Actor:GetParticleSystem() end",
			[](AActor& Actor) -> UParticleSystemComponent* { return Actor.GetComponentByClass<UParticleSystemComponent>(); })
		.Method("GetPointLight",
			"---@return PointLightComponent?\nfunction Actor:GetPointLight() end",
			[](AActor& Actor) -> UPointLightComponent* { return Actor.GetComponentByClass<UPointLightComponent>(); })
		.Method("GetSpotLight",
			"---@return SpotLightComponent?\nfunction Actor:GetSpotLight() end",
			[](AActor& Actor) -> USpotLightComponent* { return Actor.GetComponentByClass<USpotLightComponent>(); })
		.Method("SetOutline",
			"---@param enabled boolean\nfunction Actor:SetOutline(enabled) end",
			[](AActor& Actor, bool bEnabled)
			{
				UWorld* World = Actor.GetWorld();
				if (!World)
				{
					return;
				}
				for (UPrimitiveComponent* PrimitiveComponent : Actor.GetPrimitiveComponents())
				{
					if (!PrimitiveComponent)
					{
						continue;
					}
					World->GetScene().SetProxyOutlineOnly(PrimitiveComponent->GetSceneProxy(), bEnabled);
				}
			})
		.Method("GetPrimitiveComponentByName",
			"---@param name string\n---@return PrimitiveComponent?\nfunction Actor:GetPrimitiveComponentByName(name) end",
			[](AActor& Actor, const FString& ComponentName) -> UPrimitiveComponent*
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
			})
		.Method("GetComponentByName",
			"---@param name string\n---@return SceneComponent?\nfunction Actor:GetComponentByName(name) end",
			[](AActor& Actor, const FString& ComponentName) -> USceneComponent*
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
			});

	Lua.new_usertype<APawn>("Pawn",
		sol::base_classes, sol::bases<AActor>(),
		"IsPossessed", &APawn::IsPossessed,
		"GetController", &APawn::GetController,
		"SetAutoPossessPlayer", &APawn::SetAutoPossessPlayer,
		"GetAutoPossessPlayer", &APawn::GetAutoPossessPlayer,
		"GetInputComponent", &APawn::GetInputComponent);

	FLuaDocRegistry::Get().Type("Pawn", "Actor")
		.Method("---@return boolean\nfunction Pawn:IsPossessed() end")
		.Method("---@return PlayerController?\nfunction Pawn:GetController() end")
		.Method("---@param enabled boolean\nfunction Pawn:SetAutoPossessPlayer(enabled) end")
		.Method("---@return boolean\nfunction Pawn:GetAutoPossessPlayer() end")
		.Method("---@return InputComponent?\nfunction Pawn:GetInputComponent() end");

	Lua.new_usertype<APlayerController>("PlayerController",
		sol::base_classes, sol::bases<AActor>(),
		"GetPossessedPawn", &APlayerController::GetPossessedPawn,
		"Possess", &APlayerController::Possess,
		"UnPossess", &APlayerController::UnPossess,
		"GetPlayerCameraManager", &APlayerController::GetPlayerCameraManager,
		"SetViewTargetWithBlend", [](APlayerController& Controller, AActor* ViewTarget, sol::optional<float> BlendTime)
	{
		Controller.SetViewTargetWithBlend(ViewTarget, BlendTime.value_or(0.0f));
	});

	FLuaDocRegistry::Get().Type("PlayerController", "Actor")
		.Method("---@return Pawn?\nfunction PlayerController:GetPossessedPawn() end")
		.Method("---@param pawn Pawn\nfunction PlayerController:Possess(pawn) end")
		.Method("function PlayerController:UnPossess() end")
		.Method("---@return PlayerCameraManager?\nfunction PlayerController:GetPlayerCameraManager() end")
		.Method("---@param viewTarget Actor\n---@param blendTime? number\nfunction PlayerController:SetViewTargetWithBlend(viewTarget, blendTime) end");

	Lua.new_usertype<APlayerCameraManager>("PlayerCameraManager",
		sol::base_classes, sol::bases<AActor>(),
		"GetActiveCamera", &APlayerCameraManager::GetActiveCamera,
		"GetPossessedCamera", &APlayerCameraManager::GetPossessedCamera,
		"GetViewTarget", &APlayerCameraManager::GetViewTarget,
		"GetPendingViewTarget", &APlayerCameraManager::GetPendingViewTarget,
		"ToggleActiveCameraForActor", sol::overload(
			[](APlayerCameraManager& Manager, const FString& ActorName, sol::optional<float> BlendTime)
	{
		return Manager.ToggleActiveCameraForActor(ActorName, BlendTime.value_or(0.0f));
	},
			[](APlayerCameraManager& Manager, const AActor* Actor, sol::optional<float> BlendTime)
	{
		return Manager.ToggleActiveCameraForActor(Actor, BlendTime.value_or(0.0f));
	}));

	FLuaDocRegistry::Get().Type("PlayerCameraManager", "Actor")
		.Method("---@return CameraComponent?\nfunction PlayerCameraManager:GetActiveCamera() end")
		.Method("---@return CameraComponent?\nfunction PlayerCameraManager:GetPossessedCamera() end")
		.Method("---@return Actor?\nfunction PlayerCameraManager:GetViewTarget() end")
		.Method("---@return Actor?\nfunction PlayerCameraManager:GetPendingViewTarget() end")
		.Method("---@param actor string|Actor\n---@param blendTime? number\n---@return boolean\nfunction PlayerCameraManager:ToggleActiveCameraForActor(actor, blendTime) end");

	// UInputComponent — Pawn::GetInputComponent 로 얻어 lua 에서 직접 매핑/binding 추가 가능.
	// 예 (BeginPlay 안):
	//   local input = obj:GetInputComponent()
	//   input:ClearAllMappingsAndBindings()
	//   input:AddActionMapping("Jump", 0x20)   -- VK_SPACE = 0x20
	//   input:BindAction("Jump", "Pressed", function() print("jump!") end)
	Lua.new_usertype<UInputComponent>("InputComponent",
		"AddAxisMapping", [](UInputComponent& Self, const FString& Name, int InputCode, sol::optional<float> Scale)
		{
			Self.AddAxisMapping(Name, InputCode, Scale.value_or(1.0f));
		},
		"AddActionMapping", &UInputComponent::AddActionMapping,
		"BindAxis", [](UInputComponent& Self, const FString& Name, sol::protected_function Cb)
		{
			Self.BindAxis(Name, [Cb](float V)
			{
				auto R = Cb(V);
				if (!R.valid()) { sol::error e = R; UE_LOG("[Lua] BindAxis cb error: %s", e.what()); }
			});
		},
		"BindAction", [](UInputComponent& Self, const FString& Name, const FString& EventStr, sol::protected_function Cb)
		{
			const EInputEvent Ev = (EventStr == "Released") ? EInputEvent::Released : EInputEvent::Pressed;
			Self.BindAction(Name, Ev, [Cb]()
			{
				auto R = Cb();
				if (!R.valid()) { sol::error e = R; UE_LOG("[Lua] BindAction cb error: %s", e.what()); }
			});
		},
		"ClearBindings", &UInputComponent::ClearBindings,
		"ClearMappings", &UInputComponent::ClearMappings,
		"ClearAllMappingsAndBindings", &UInputComponent::ClearAllMappingsAndBindings);

	FLuaDocRegistry::Get().Type("InputComponent")
		.Method("---@param name string\n---@param key integer\n---@param scale? number\nfunction InputComponent:AddAxisMapping(name, key, scale) end")
		.Method("---@param name string\n---@param key integer\nfunction InputComponent:AddActionMapping(name, key) end")
		.Method("---@param name string\n---@param callback fun(value: number)\nfunction InputComponent:BindAxis(name, callback) end")
		.Method("---@param name string\n---@param event 'Pressed'|'Released'\n---@param callback fun()\nfunction InputComponent:BindAction(name, event, callback) end")
		.Method("function InputComponent:ClearBindings() end")
		.Method("function InputComponent:ClearMappings() end")
		.Method("function InputComponent:ClearAllMappingsAndBindings() end");

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
	World.set_function("SpawnParticleSystem",
		[](const FString& ParticlePath, const FVector& Location, sol::optional<FVector> Rotation) -> AActor*
	{
		if (!GEngine) return nullptr;
		UWorld* W = GEngine->GetWorld();
		if (!W) return nullptr;

		UParticleSystem* ParticleSystem = FParticleSystemManager::Get().Load(ParticlePath);
		if (!ParticleSystem)
		{
			UE_LOG("[Lua] World.SpawnParticleSystem failed to load: %s", ParticlePath.c_str());
			return nullptr;
		}

		AActor* Actor = W->SpawnActor<AActor>();
		if (!Actor) return nullptr;

		UParticleSystemComponent* ParticleComponent = Actor->AddComponent<UParticleSystemComponent>();
		if (!ParticleComponent)
		{
			W->DestroyActor(Actor);
			return nullptr;
		}

		Actor->SetRootComponent(ParticleComponent);
		Actor->SetActorLocation(Location);
		if (Rotation.has_value())
		{
			Actor->SetActorRotation(Rotation.value());
		}

		ParticleComponent->SetTemplate(ParticleSystem);
		ParticleComponent->ResetSystem();
		ParticleComponent->SetEmitterSpawningEnabled(true);
		ParticleComponent->Activate();
		return Actor;
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
	World.set_function("RaycastSkeletalMesh",
		[](const FVector& Start, const FVector& Dir, float MaxDist,
		   sol::optional<AActor*> IgnoreActor) -> sol::object
	{
		if (!GEngine || !GEngine->GetWorld()) return sol::lua_nil;
		FHitResult Hit;
		const bool bHit = GEngine->GetWorld()->PhysicsRaycast(
			Start, Dir, MaxDist, Hit,
			ECollisionChannel::SkeletalMesh,
			IgnoreActor.value_or(nullptr));
		if (!bHit) return sol::lua_nil;
		return sol::make_object(FLuaScriptManager::GetState(), Hit);
	});
	World.set_function("RaycastWorldStatic",
		[](const FVector& Start, const FVector& Dir, float MaxDist,
		   sol::optional<AActor*> IgnoreActor) -> sol::object
	{
		if (!GEngine || !GEngine->GetWorld()) return sol::lua_nil;
		FHitResult Hit;
		const bool bHit = GEngine->GetWorld()->PhysicsRaycast(
			Start, Dir, MaxDist, Hit,
			ECollisionChannel::WorldStatic,
			IgnoreActor.value_or(nullptr));
		if (!bHit) return sol::lua_nil;
		return sol::make_object(FLuaScriptManager::GetState(), Hit);
	});
	World.set_function("RaycastPrimitive",
		[](const FVector& Start, const FVector& Dir, float MaxDist,
		   sol::optional<AActor*> IgnoreActor) -> sol::object
	{
		if (!GEngine || !GEngine->GetWorld()) return sol::lua_nil;
		FRay Ray;
		Ray.Origin = Start;
		Ray.Direction = Dir;
		FHitResult Hit;
		AActor* HitActor = nullptr;
		const bool bHit = GEngine->GetWorld()->RaycastPrimitives(Ray, Hit, HitActor, IgnoreActor.value_or(nullptr));
		if (!bHit || Hit.Distance > MaxDist) return sol::lua_nil;
		Hit.HitActor = HitActor;
		return sol::make_object(FLuaScriptManager::GetState(), Hit);
	});

	FLuaDocRegistry::Get().Type("WorldLib")
		.Method("---@param className string\n---@return Actor?\nfunction World.SpawnActor(className) end")
		.Method("---@param particlePath string\n---@param location Vector\n---@param rotation? Vector\n---@return Actor?\nfunction World.SpawnParticleSystem(particlePath, location, rotation) end")
		.Method("---@param actorName string\n---@return Actor?\nfunction World.FindActorByName(actorName) end")
		.Method("---@param className string\n---@return Actor?\nfunction World.FindFirstActorByClass(className) end")
		.Method("---@param tag string\n---@return Actor?\nfunction World.FindFirstActorByTag(tag) end")
		.Method("---@param tag string\n---@return Actor[]\nfunction World.FindActorsByTag(tag) end")
		.Method("---@param start Vector\n---@param dir Vector\n---@param maxDist number\n---@param ignoreActor? Actor\n---@return HitResult?\nfunction World.RaycastSkeletalMesh(start, dir, maxDist, ignoreActor) end")
		.Method("---@param start Vector\n---@param dir Vector\n---@param maxDist number\n---@param ignoreActor? Actor\n---@return HitResult?\nfunction World.RaycastWorldStatic(start, dir, maxDist, ignoreActor) end")
		.Method("---@param start Vector\n---@param dir Vector\n---@param maxDist number\n---@param ignoreActor? Actor\n---@return HitResult?\nfunction World.RaycastPrimitive(start, dir, maxDist, ignoreActor) end");
	FLuaDocRegistry::Get().Global("World", "WorldLib");

	sol::table Debug = Lua.create_named_table("Debug");
	Debug.set_function("DrawLine",
		[](const FVector& A, const FVector& B,
		   int R, int G, int Bcol, sol::optional<float> Duration)
	{
		if (!GEngine || !GEngine->GetWorld()) return;
		DrawDebugLine(GEngine->GetWorld(), A, B,
			FColor(R, G, Bcol), Duration.value_or(0.0f));
	});
	Debug.set_function("DrawSphere",
		[](const FVector& Center, float Radius,
		   int R, int G, int Bcol, sol::optional<float> Duration,
		   sol::optional<int> Segments)
	{
		if (!GEngine || !GEngine->GetWorld()) return;
		DrawDebugSphere(GEngine->GetWorld(), Center, Radius,
			Segments.value_or(12),
			FColor(R, G, Bcol), Duration.value_or(0.0f));
	});

	FLuaDocRegistry::Get().Type("DebugLib")
		.Method("---@param a Vector\n---@param b Vector\n---@param r integer\n---@param g integer\n---@param b_ integer\n---@param duration? number\nfunction Debug.DrawLine(a, b, r, g, b_, duration) end")
		.Method("---@param center Vector\n---@param radius number\n---@param r integer\n---@param g integer\n---@param b integer\n---@param duration? number\n---@param segments? integer\nfunction Debug.DrawSphere(center, radius, r, g, b, duration, segments) end");
	FLuaDocRegistry::Get().Global("Debug", "DebugLib");

	Lua.new_usertype<USkeletalMeshComponent>("SkeletalMeshComponent",
		sol::base_classes, sol::bases<USkinnedMeshComponent, UPrimitiveComponent, USceneComponent>(),

		"PlayAnimationByPath", [](USkeletalMeshComponent& C, const FString& AnimationPath, sol::optional<bool> bLooping)
	{
		if (AnimationPath.empty() || AnimationPath == "None")
		{
			UE_LOG("[Lua] PlayAnimationByPath skipped: empty animation path.");
			return false;
		}

		UAnimSequence* Sequence = FAnimationManager::Get().LoadAnimation(AnimationPath);
		if (!Sequence)
		{
			UE_LOG("[Lua] PlayAnimationByPath failed to load: %s", AnimationPath.c_str());
			return false;
		}

		C.PlayAnimation(Sequence, bLooping.value_or(false));
		return true;
	},

		"SetSimulatePhysics", [](USkeletalMeshComponent& C, bool bEnable)
	{
		C.SetSimulatePhysics(bEnable);
	},

		"IsSimulatingPhysics", [](USkeletalMeshComponent& C)
	{
		return C.IsSimulatingPhysics();
	},

		"SetPhysicsBlendWeight", [](USkeletalMeshComponent& C, float InWeight)
	{
		C.SetPhysicsBlendWeight(InWeight);
	},

		"GetPhysicsBlendWeight", [](USkeletalMeshComponent& C)
	{
		return C.GetPhysicsBlendWeight();
	},

		"GetBoneSocketLocation", [](USkeletalMeshComponent& C, const FString& BoneName, const FVector& LocalOffset)
	{
		FTransform SocketWorld;
		if (C.GetBoneSocketWorldTransform(
			BoneName,
			FTransform(LocalOffset, FQuat::Identity, FVector::OneVector),
			SocketWorld))
		{
			return SocketWorld.Location;
		}

		return FVector::ZeroVector;
	},

		"GetBoneSocketRotation", [](USkeletalMeshComponent& C, const FString& BoneName, const FVector& LocalOffset)
	{
		FTransform SocketWorld;
		if (C.GetBoneSocketWorldTransform(
			BoneName,
			FTransform(LocalOffset, FQuat::Identity, FVector::OneVector),
			SocketWorld))
		{
			return SocketWorld.Rotation.ToRotator().ToVector();
		}

		return FVector::ZeroVector;
	},

		// 본 스케일 조작 — Vector(0,0,0) 로 설정하면 해당 본과 자식 메시 섹션이 사라지는 효과.
		// ADS 시 시야를 가리는 v_cro 같은 인게임 메시 파츠를 임시로 숨길 때 쓴다.
		"SetBoneScale", [](USkeletalMeshComponent& C, const FString& BoneName, const FVector& Scale)
	{
		const int32 BoneIndex = C.FindBoneIndex(BoneName);
		if (BoneIndex < 0) return false;
		C.SetBoneScaleByIndex(BoneIndex, Scale);
		return true;
	},

		"ResetBoneEditPose", [](USkeletalMeshComponent& C)
	{
		C.ResetBoneEditPose();
	},

		"GetBoneScale", [](USkeletalMeshComponent& C, const FString& BoneName)
	{
		const int32 BoneIndex = C.FindBoneIndex(BoneName);
		if (BoneIndex < 0) return FVector::OneVector;
		return C.GetBoneScaleByIndex(BoneIndex);
	}
	);

	FLuaDocRegistry::Get().Type("SkeletalMeshComponent", "PrimitiveComponent")
		.Method("---@param animationPath string\n---@param looping? boolean\n---@return boolean\nfunction SkeletalMeshComponent:PlayAnimationByPath(animationPath, looping) end")
		.Method("---@param enabled boolean\nfunction SkeletalMeshComponent:SetSimulatePhysics(enabled) end")
		.Method("---@return boolean\nfunction SkeletalMeshComponent:IsSimulatingPhysics() end")
		.Method("---@param weight number\nfunction SkeletalMeshComponent:SetPhysicsBlendWeight(weight) end")
		.Method("---@return number\nfunction SkeletalMeshComponent:GetPhysicsBlendWeight() end")
		.Method("---@param boneName string\n---@param localOffset Vector\n---@return Vector\nfunction SkeletalMeshComponent:GetBoneSocketLocation(boneName, localOffset) end")
		.Method("---@param boneName string\n---@param localOffset Vector\n---@return Vector\nfunction SkeletalMeshComponent:GetBoneSocketRotation(boneName, localOffset) end")
		.Method("---@param boneName string\n---@param scale Vector\n---@return boolean\nfunction SkeletalMeshComponent:SetBoneScale(boneName, scale) end")
		.Method("---@return nil\nfunction SkeletalMeshComponent:ResetBoneEditPose() end")
		.Method("---@param boneName string\n---@return Vector\nfunction SkeletalMeshComponent:GetBoneScale(boneName) end");

	// 게임 특화 usertype/enum/global 은 Game 모듈의
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
		"bind_event", [](UUserWidget& Widget, const FString& ElementId, const FString& EventName, sol::protected_function Callback)
	{
		Widget.BindEvent(ElementId, EventName, Callback);
	},
		"SetText", &UUserWidget::SetText,
		"set_text", &UUserWidget::SetText,
		"SetProperty", &UUserWidget::SetProperty,
		"set_property", &UUserWidget::SetProperty,
		"SetAttribute", &UUserWidget::SetAttribute,
		"set_attribute", &UUserWidget::SetAttribute,
		"SetWantsMouse", &UUserWidget::SetWantsMouse,
		"WantsMouse", &UUserWidget::WantsMouse);

	sol::table UI = Lua.create_named_table("UI");
	UI.set_function("CreateWidget", [](const FString& DocumentPath)
	{
		return UUIManager::Get().CreateWidget(nullptr, DocumentPath);
	});
}
