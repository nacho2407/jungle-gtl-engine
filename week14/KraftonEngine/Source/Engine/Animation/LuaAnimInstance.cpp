#include "LuaAnimInstance.h"

#include "Animation/AnimationManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimState.h"
#include "Animation/AnimationStateMachine.h"
#include "Animation/PoseContext.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Log.h"
#include "Core/PropertyTypes.h"
#include "GameFramework/AActor.h"
#include "Input/InputSystem.h"
#include "Lua/LuaScriptManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <Windows.h>

#include <cstring>
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

	// .uasset 없이 즉시 데모 가능하도록 mock 시퀀스 (Phase 3 의 UAnimSequence::CreateMock* 재활용).
	// MockType = "sway" | "wave".
	Anim.set_function("register_mock_state",
		[this](std::string Name, std::string MockType, float Duration, float AmpDeg, float Rate, bool Loop)
		{
			Lua_RegisterMockState(Name, MockType, Duration, AmpDeg, Rate, Loop);
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

	// Owner actor 의 UCharacterMovementComponent::GetSpeed 노출 — FSM condition 안에서
	// self.Speed = Anim.get_owner_speed() 식으로 movement 시뮬레이션 결과를 그대로 사용.
	// movement 컴포넌트 없으면 0 (안전 fallback — lua 측 분기 불필요).
	Anim.set_function("get_owner_speed",
		[this]() -> float
		{
			if (!OwningComponent) return 0.0f;
			AActor* Owner = OwningComponent->GetOwner();
			if (!Owner) return 0.0f;
			UCharacterMovementComponent* Move = Owner->GetComponentByClass<UCharacterMovementComponent>();
			return Move ? Move->GetSpeed() : 0.0f;
		});

	// Owner 의 movement mode — "Walking" / "Falling" / "" (movement 없음).
	Anim.set_function("get_owner_movement_mode",
		[this]() -> std::string
		{
			if (!OwningComponent) return "";
			AActor* Owner = OwningComponent->GetOwner();
			if (!Owner) return "";
			UCharacterMovementComponent* Move = Owner->GetComponentByClass<UCharacterMovementComponent>();
			if (!Move) return "";
			return Move->IsFalling() ? "Falling" : "Walking";
		});

	// is_owner_falling — 편의 bool. Jump/Fall 전이 condition 에 자연.
	Anim.set_function("is_owner_falling",
		[this]() -> bool
		{
			if (!OwningComponent) return false;
			AActor* Owner = OwningComponent->GetOwner();
			if (!Owner) return false;
			UCharacterMovementComponent* Move = Owner->GetComponentByClass<UCharacterMovementComponent>();
			return Move ? Move->IsFalling() : false;
		});

	// ── Montage 트리거 (lua → AnimInstance) ──
	//   Anim.play_montage(path)                       — section/rate/blend default.
	//   Anim.play_montage(path, "SectionName")        — section 지정.
	//   Anim.play_montage(path, "SectionName", 1.5)   — section + rate.
	//   PlayMontage 는 새 montage 들어오면 즉시 교체 (BlendIn 으로 자연 전환).
	Anim.set_function("play_montage",
		[this](std::string Path, sol::object Section, sol::object Rate, sol::object BlendIn)
		{
			if (Path.empty()) return;
			UAnimMontage* M = FAnimationManager::Get().LoadMontage(Path);
			if (!M)
			{
				UE_LOG("[LuaAnimInstance] play_montage: failed to load '%s'", Path.c_str());
				return;
			}
			FName SectionName = FName::None;
			if (Section.is<std::string>())
			{
				const std::string SecStr = Section.as<std::string>();
				if (!SecStr.empty()) SectionName = FName(SecStr);
			}
			const float PlayRate    = Rate.is<float>()    ? Rate.as<float>()    : 1.0f;
			const float BlendInTime = BlendIn.is<float>() ? BlendIn.as<float>() : -1.0f;
			PlayMontage(M, SectionName, PlayRate, BlendInTime);
		});

	Anim.set_function("stop_montage",
		[this](sol::object BlendOut)
		{
			const float BlendOutTime = BlendOut.is<float>() ? BlendOut.as<float>() : -1.0f;
			StopMontage(BlendOutTime);
		});

	Anim.set_function("is_montage_playing",
		[this]() -> bool { return IsMontagePlaying(); });

	Anim.set_function("jump_to_section",
		[this](std::string Name)
		{
			if (!Name.empty()) Montage_JumpToSection(FName(Name));
		});

	// ── Input edge detection — lua FSM/montage trigger 용 ──
	//   GetKeyDown == "이번 frame 에서 새로 눌림". 매 frame 호출 안전.
	Anim.set_function("is_left_mouse_pressed",
		[]() -> bool { return InputSystem::Get().GetKeyDown(VK_LBUTTON); });
	Anim.set_function("is_right_mouse_pressed",
		[]() -> bool { return InputSystem::Get().GetKeyDown(VK_RBUTTON); });
	Anim.set_function("is_key_pressed",
		[](int VK) -> bool { return InputSystem::Get().GetKeyDown(VK); });
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

void ULuaAnimInstance::Lua_RegisterMockState(const std::string& Name, const std::string& MockType,
                                             float Duration, float AmplitudeDeg, float Rate, bool Loop)
{
	if (!FSM) return;
	USkeletalMesh* Mesh = GetSkeletalMesh();
	if (!Mesh)
	{
		UE_LOG("[LuaAnimInstance] register_mock_state '%s' — no SkeletalMesh", Name.c_str());
		return;
	}

	UAnimSequence* Seq = nullptr;
	if (MockType == "sway")
	{
		Seq = UAnimSequence::CreateMockSwaySequence(Mesh, /*BoneIdx*/0, Duration, AmplitudeDeg);
	}
	else if (MockType == "wave")
	{
		Seq = UAnimSequence::CreateMockWaveSequence(Mesh, Duration, AmplitudeDeg);
	}
	else
	{
		UE_LOG("[LuaAnimInstance] register_mock_state '%s' — unknown MockType '%s' (use \"sway\" or \"wave\")",
			Name.c_str(), MockType.c_str());
		return;
	}

	UAnimState* S = UObjectManager::Get().CreateObject<UAnimState>(this);
	S->StateName = FName(Name.c_str());
	S->Sequence  = Seq;
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
void ULuaAnimInstance::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);
	if (!PropertyName) return;

	if (std::strcmp(PropertyName, "Script File") == 0)
	{
		ReloadScript();
	}
}

void ULuaAnimInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	// PIE Duplicate / Scene save 시 Component 가 이 함수를 호출해 buffer 라운드트립.
	// 새 인스턴스의 NativeInitializeAnimation 보다 먼저 호출되므로 init 안에서 ScriptFile 살아있음.
	Ar << ScriptFile;
}
