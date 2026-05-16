#include "LuaAnimInstance.h"

#include "Animation/AnimationManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimState.h"
#include "Animation/AnimationStateMachine.h"
#include "Animation/PoseContext.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Log.h"
#include "Core/PropertyTypes.h"
#include "Lua/LuaScriptManager.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"

#include <cstring>

IMPLEMENT_CLASS(ULuaAnimInstance, UAnimInstance)

ULuaAnimInstance::~ULuaAnimInstance()
{
	FLuaScriptManager::UnregisterAnimInstance(this);
	ClearFSM();
}

// ──────────────────────────────────────────────
// Lifecycle
// ──────────────────────────────────────────────
void ULuaAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (ScriptFile.empty() || ScriptFile == "None")
	{
		UE_LOG("[LuaAnimInstance] ScriptFile not set.");
		return;
	}

	sol::state& Lua = FLuaScriptManager::GetState();

	// per-instance env — globals 상속 (math, print, Anim 모듈 등은 globals 아래 또는 여기서 install).
	Env = sol::environment(Lua, sol::create, Lua.globals());

	// self table — lua 변수 저장소.
	LuaSelf = Lua.create_table();
	Env["self"] = LuaSelf;

	// Anim.* 모듈 binding 설치 (this 캡처 — env 와 같은 lifetime).
	InstallBindings();

	// 한글 경로 호환 — wide 로 읽어 safe_script (string overload).
	FString Content;
	if (!FLuaScriptManager::ReadScriptFileContent(ScriptFile, Content))
	{
		UE_LOG("[LuaAnimInstance] Failed to read script: %s", ScriptFile.c_str());
		return;
	}
	const FString ResolvedPath = FLuaScriptManager::ResolveScriptPath(ScriptFile);
	auto Result = Lua.safe_script(Content, Env, sol::script_pass_on_error, ResolvedPath);
	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("[LuaAnimInstance] Script load error %s: %s", ScriptFile.c_str(), Err.what());
		return;
	}

	// 함수 cache.
	LuaInit     = Env["init"];
	LuaUpdate   = Env["update"];
	LuaOnNotify = Env["on_notify"];

	// FSM 생성 (init 안에서 lua 가 Anim.register_* 호출하면 FSM 에 적재됨).
	FSM = UObjectManager::Get().CreateObject<UAnimationStateMachine>(this);

	DispatchLuaInit();

	// Hot-reload 등록 — .lua 파일 변경 시 FLuaScriptManager 가 ReloadScript 호출.
	// 이미 등록된 경우 set-like 보장 (manager 측).
	FLuaScriptManager::RegisterAnimInstance(this);
}

void ULuaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (LuaUpdate.valid())
	{
		auto R = LuaUpdate(LuaSelf, DeltaSeconds);
		if (!R.valid())
		{
			sol::error Err = R;
			UE_LOG("[LuaAnimInstance] update() error: %s", Err.what());
		}
	}

	if (FSM) FSM->Tick(this, DeltaSeconds);
}

void ULuaAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	if (FSM) FSM->Evaluate(this, Output);
	else     Super::EvaluateAnimation(Output);
}

void ULuaAnimInstance::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
	if (!LuaOnNotify.valid()) return;

	auto R = LuaOnNotify(LuaSelf, Notify.NotifyName.ToString());
	if (!R.valid())
	{
		sol::error Err = R;
		UE_LOG("[LuaAnimInstance] on_notify() error: %s", Err.what());
	}
}

void ULuaAnimInstance::ReloadScript()
{
	ClearFSM();
	NativeInitializeAnimation();
}

void ULuaAnimInstance::ClearFSM()
{
	if (FSM)
	{
		UObjectManager::Get().DestroyObject(FSM);
		FSM = nullptr;
	}
	LuaInit     = sol::nil;
	LuaUpdate   = sol::nil;
	LuaOnNotify = sol::nil;
	LuaSelf     = sol::lua_nil;
	Env         = sol::environment();
}

void ULuaAnimInstance::DispatchLuaInit()
{
	if (!LuaInit.valid()) return;
	auto R = LuaInit(LuaSelf);
	if (!R.valid())
	{
		sol::error Err = R;
		UE_LOG("[LuaAnimInstance] init() error: %s", Err.what());
	}
}

// ──────────────────────────────────────────────
// Anim.* 모듈 binding (lua 에서 호출하는 진입점)
// ──────────────────────────────────────────────
void ULuaAnimInstance::InstallBindings()
{
	// Anim 모듈 — env 안에 namespace table 생성.
	// this 캡처: env 가 ULuaAnimInstance 멤버이므로 lifetime 일치.
	sol::table Anim = Env.create_named("Anim");

	Anim.set_function("register_state",
		[this](std::string Name, std::string Path, float Rate, bool Loop)
		{
			Lua_RegisterState(Name, Path, Rate, Loop);
		});

	Anim.set_function("register_transition",
		[this](std::string From, std::string To, sol::protected_function Cond, float BlendTime)
		{
			Lua_RegisterTransition(From, To, std::move(Cond), BlendTime);
		});

	Anim.set_function("set_initial_state",
		[this](std::string Name)
		{
			Lua_SetInitialState(Name);
		});

	Anim.set_function("request_transition",
		[this](std::string Name, float BlendDuration)
		{
			Lua_RequestTransition(Name, BlendDuration);
		});

	Anim.set_function("get_current_state",
		[this]() { return Lua_GetCurrentState(); });
}

// ──────────────────────────────────────────────
// Lua → C++ 진입점 구현
// ──────────────────────────────────────────────
void ULuaAnimInstance::Lua_RegisterState(const std::string& Name, const std::string& AnimPath, float Rate, bool Loop)
{
	if (!FSM)
	{
		UE_LOG("[LuaAnimInstance] register_state called before FSM ready: %s", Name.c_str());
		return;
	}

	UAnimSequence* Sequence = nullptr;
	if (!AnimPath.empty() && AnimPath != "None")
	{
		Sequence = FAnimationManager::Get().LoadAnimation(AnimPath);
		if (!Sequence)
		{
			UE_LOG("[LuaAnimInstance] register_state '%s' — anim not found: %s (ref pose fallback)",
				Name.c_str(), AnimPath.c_str());
		}
	}

	UAnimState* S = UObjectManager::Get().CreateObject<UAnimState>(this);
	S->StateName = FName(Name.c_str());
	S->Sequence  = Sequence;
	S->PlayRate  = Rate;
	S->bLooping  = Loop;
	FSM->RegisterState(S);
}

void ULuaAnimInstance::Lua_RegisterTransition(const std::string& From, const std::string& To,
                                              sol::protected_function Cond, float BlendTime)
{
	if (!FSM)
	{
		UE_LOG("[LuaAnimInstance] register_transition called before FSM ready: %s -> %s",
			From.c_str(), To.c_str());
		return;
	}

	FStateTransition T;
	T.From      = (From == "AnyState" || From.empty()) ? FName::None : FName(From.c_str());
	T.To        = FName(To.c_str());
	T.BlendTime = BlendTime;

	// sol::protected_function 은 lua function ref 를 들고 다님 (복사 가능).
	// 람다는 FSM 의 transitions 배열 안에 산다 → ULuaAnimInstance 가 살아있는 동안 안전.
	// ULuaAnimInstance 소멸 시 ClearFSM → FSM 도 destroy → 람다 정리.
	T.Condition = [Cond](UAnimInstance*) -> bool
	{
		auto R = Cond();
		if (!R.valid())
		{
			sol::error Err = R;
			UE_LOG("[LuaAnimInstance] transition condition error: %s", Err.what());
			return false;
		}
		return R.get<bool>();
	};

	FSM->RegisterTransition(T);
}

void ULuaAnimInstance::Lua_SetInitialState(const std::string& Name)
{
	if (FSM) FSM->SetInitialState(FName(Name.c_str()));
}

void ULuaAnimInstance::Lua_RequestTransition(const std::string& Name, float BlendDuration)
{
	if (FSM) FSM->RequestTransition(FName(Name.c_str()), BlendDuration);
}

std::string ULuaAnimInstance::Lua_GetCurrentState() const
{
	if (!FSM) return "";
	return FSM->GetCurrentStateName().ToString();
}

// ──────────────────────────────────────────────
// Editor 통합
// ──────────────────────────────────────────────
void ULuaAnimInstance::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	Super::GetEditableProperties(OutProps);

	FPropertyDescriptor ScriptProp;
	ScriptProp.Name          = "Script File";
	ScriptProp.Type          = EPropertyType::ObjectRef;
	ScriptProp.Category      = "Animation|Lua";
	ScriptProp.ValuePtr      = &ScriptFile;
	ScriptProp.AssetTypeName = "LuaAnimScript";
	OutProps.push_back(ScriptProp);
}

void ULuaAnimInstance::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);
	if (!PropertyName) return;

	if (std::strcmp(PropertyName, "Script File") == 0)
	{
		ReloadScript();
	}
}
