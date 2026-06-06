#include "LuaAnimInstance.h"

#include "Animation/AnimationManager.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Montage/AnimMontage.h"
#include "Animation/StateMachine/AnimState.h"
#include "Animation/PoseContext.h"
#include "Animation/Nodes/AnimNode_BlendListByEnum.h"
#include "Animation/Nodes/AnimNode_LayeredBlendPerBone.h"
#include "Animation/Nodes/AnimNode_RefPose.h"
#include "Animation/Nodes/AnimNode_Slot.h"
#include "Animation/Nodes/AnimNode_StateMachine.h"
#include "Animation/Nodes/AnimNode_SequencePlayer.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Core/Types/PropertyTypes.h"
#include "GameFramework/AActor.h"
#include "Input/InputSystem.h"
#include "Lua/LuaScriptManager.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Viewport/GameViewportClient.h"

#include <Windows.h>

#include <cstring>
#include <cmath>
ULuaAnimInstance::~ULuaAnimInstance()
{
	FLuaScriptManager::UnregisterAnimInstance(this);
	ClearGraph();
}

// ──────────────────────────────────────────────
// Lifecycle
// ──────────────────────────────────────────────
void ULuaAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	ClearGraph();

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

	DispatchLuaInit();

	// AnimGraph RootNode 확인 — lua 가 Anim.set_root_node 명시 호출 + 트리 안에 Slot 노드 박기
	// 책임은 사용자. 자동 wrap 안 함 (이전 자동 wrap 가드가 트리 깊이 검사 안 해서 같은
	// SlotName 의 Slot 이 중복 박혀 MontageInstance Tick 이 2× 진행되는 버그 — Character 의
	// 명시 wrap 패턴과 통일).
	// 안 했으면 (lua 가 set_root_node 호출 안 함) — RefPose fallback + 경고.
	if (!GetRootNode())
	{
		UE_LOG("[LuaAnimInstance] init() 가 Anim.set_root_node 호출 안 함 — ref pose fallback.");
		SetRootNode(MakeNode<FAnimNode_RefPose>());
	}

	// Hot-reload 등록 — .lua 파일 변경 시 FLuaScriptManager 가 ReloadScript 호출.
	// 이미 등록된 경우 set-like 보장 (manager 측).
	FLuaScriptManager::RegisterAnimInstance(this);
}

void ULuaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// 사용자 변수 갱신 — UE 본가 NativeUpdate 패턴.
	// lua 의 update(self, dt) 가 self.Speed 같은 변수를 갱신하면 같은 frame 의 RootNode->Update
	// 에서 condition 람다가 즉시 사용.
	if (LuaUpdate.valid())
	{
		auto R = LuaUpdate(LuaSelf, DeltaSeconds);
		if (!R.valid())
		{
			sol::error Err = R;
			UE_LOG("[LuaAnimInstance] update() error: %s", Err.what());
		}
	}
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


void ULuaAnimInstance::PostEvaluatePose(FPoseContext& Output)
{
	ApplyLuaMorphOverrides(Output);
}

void ULuaAnimInstance::EnsureLuaMorphWeightStorage()
{
	USkeletalMesh* Mesh = GetSkeletalMesh();
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const size_t MorphCount = Asset ? Asset->MorphTargets.size() : 0;

	if (LuaMorphWeights.size() != MorphCount)
	{
		LuaMorphWeights.assign(MorphCount, 0.0f);
	}

	if (LuaMorphOverrideMask.size() != MorphCount)
	{
		LuaMorphOverrideMask.assign(MorphCount, 0);
		bLuaMorphOverrideEnabled = false;
	}
}

void ULuaAnimInstance::ApplyLuaMorphOverrides(FPoseContext& Output) const
{
	if (!bLuaMorphOverrideEnabled || LuaMorphWeights.empty() || LuaMorphWeights.size() != LuaMorphOverrideMask.size())
	{
		return;
	}

	if (Output.MorphWeights.size() < LuaMorphWeights.size())
	{
		Output.MorphWeights.resize(LuaMorphWeights.size(), 0.0f);
	}

	for (size_t Index = 0; Index < LuaMorphWeights.size(); ++Index)
	{
		if (LuaMorphOverrideMask[Index] != 0)
		{
			Output.MorphWeights[Index] = LuaMorphWeights[Index];
		}
	}
}

void ULuaAnimInstance::ReloadScript()
{
	ClearGraph();
	NativeInitializeAnimation();
}

void ULuaAnimInstance::ClearGraph()
{
	// RootNode 가 OwnedNodes 의 raw 를 가리키는 상태 — dangling 방지 위해 RootNode 먼저 nullptr.
	SetRootNode(nullptr);

	// graph build 로 생성된 노드들 정리 — ReloadScript 시 다시 build 하면 OwnedNodes 가
	// 누적되어 메모리 leak. 매 reload 마다 cleanup.
	OwnedNodes.clear();

	LuaInit     = sol::nil;
	LuaUpdate   = sol::nil;
	LuaOnNotify = sol::nil;
	LuaSelf     = sol::lua_nil;
	Env         = sol::environment();

	LuaMorphWeights.clear();
	LuaMorphOverrideMask.clear();
	bLuaMorphOverrideEnabled = false;
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
			return Move->GetMovementModeName();
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

	// is_owner_wall_running — 벽타기 전용 애니메이션 전이 condition 을 단순하게 쓰기 위한 편의 bool.
	Anim.set_function("is_owner_wall_running",
		[this]() -> bool
		{
			if (!OwningComponent) return false;
			AActor* Owner = OwningComponent->GetOwner();
			if (!Owner) return false;
			UCharacterMovementComponent* Move = Owner->GetComponentByClass<UCharacterMovementComponent>();
			return Move ? Move->IsWallRunning() : false;
		});

	// Slot 인자는 sol::object — None/missing 이면 FName::None (→ 내부에서 DefaultMontageSlot resolve).
	// play_montage / stop_montage / is_montage_playing / jump_to_section 모두 공통 사용.
	auto ResolveSlot = [](const sol::object& SlotObj) -> FName
	{
		if (SlotObj.is<std::string>())
		{
			const std::string Str = SlotObj.as<std::string>();
			if (!Str.empty()) return FName(Str);
		}
		return FName::None;
	};

	// ── Montage 트리거 (lua → AnimInstance) ──
	//   Anim.play_montage(path)                              — DefaultSlot, default 옵션.
	//   Anim.play_montage(path, "SectionName")               — section 지정.
	//   Anim.play_montage(path, "SectionName", 1.5)          — + rate.
	//   Anim.play_montage(path, "SectionName", 1.0, 0.2)     — + blendIn.
	//   Anim.play_montage(path, nil, nil, nil, "UpperBody")  — slot 만 명시.
	Anim.set_function("play_montage",
		[this, ResolveSlot](std::string Path, sol::object Section, sol::object Rate, sol::object BlendIn,
		                    sol::object SlotName)
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
			PlayMontage(M, SectionName, PlayRate, BlendInTime, ResolveSlot(SlotName));
		});

	// ── Montage 제어 API — slot 인자 (마지막 sol::object) ──
	//   Anim.stop_montage(blendOut, slot)
	//   Anim.is_montage_playing(slot)
	//   Anim.jump_to_section(section_name, slot)
	Anim.set_function("stop_montage",
		[this, ResolveSlot](sol::object BlendOut, sol::object SlotName)
		{
			const float BlendOutTime = BlendOut.is<float>() ? BlendOut.as<float>() : -1.0f;
			StopMontage(BlendOutTime, ResolveSlot(SlotName));
		});

	Anim.set_function("is_montage_playing",
		[this, ResolveSlot](sol::object SlotName) -> bool
		{
			return IsMontagePlaying(nullptr, ResolveSlot(SlotName));
		});

	Anim.set_function("jump_to_section",
		[this, ResolveSlot](std::string Name, sol::object SlotName)
		{
			if (!Name.empty()) Montage_JumpToSection(FName(Name), ResolveSlot(SlotName));
		});

	// ── Morph override API ──
	// Sequence 안에 저장된 morph curve 위에 Lua 값이 override 된다.
	// Anim.set_morph_weight("Smile", 1.0)
	// Anim.clear_morph_weight("Smile")
	// Anim.clear_morph_weights()
	Anim.set_function("set_morph_weight",
		[this](std::string Name, float Weight)
		{
			EnsureLuaMorphWeightStorage();

			USkeletalMesh* Mesh = GetSkeletalMesh();
			FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
			if (!Asset || Name.empty())
			{
				return;
			}

			const int32 MorphIndex = Asset->FindMorphTargetIndex(FString(Name));
			if (MorphIndex < 0 || MorphIndex >= static_cast<int32>(LuaMorphWeights.size()))
			{
				UE_LOG("[LuaAnim] set_morph_weight — unknown morph target: %s", Name.c_str());
				return;
			}

			if (!std::isfinite(Weight))
			{
				Weight = 0.0f;
			}

			LuaMorphWeights[MorphIndex] = Weight;
			LuaMorphOverrideMask[MorphIndex] = 1;
			bLuaMorphOverrideEnabled = true;
		});

	Anim.set_function("set_morph_weight_by_index",
		[this](int32 MorphIndex, float Weight)
		{
			EnsureLuaMorphWeightStorage();
			if (MorphIndex < 0 || MorphIndex >= static_cast<int32>(LuaMorphWeights.size()))
			{
				return;
			}

			if (!std::isfinite(Weight))
			{
				Weight = 0.0f;
			}

			LuaMorphWeights[MorphIndex] = Weight;
			LuaMorphOverrideMask[MorphIndex] = 1;
			bLuaMorphOverrideEnabled = true;
		});

	Anim.set_function("get_morph_weight",
		[this](std::string Name) -> float
		{
			EnsureLuaMorphWeightStorage();
			USkeletalMesh* Mesh = GetSkeletalMesh();
			FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
			if (!Asset || Name.empty())
			{
				return 0.0f;
			}

			const int32 MorphIndex = Asset->FindMorphTargetIndex(FString(Name));
			if (MorphIndex < 0 || MorphIndex >= static_cast<int32>(LuaMorphWeights.size()))
			{
				return 0.0f;
			}

			if (LuaMorphOverrideMask[MorphIndex] != 0)
			{
				return LuaMorphWeights[MorphIndex];
			}

			return OwningComponent ? OwningComponent->GetMorphTargetWeightByIndex(MorphIndex) : 0.0f;
		});

	Anim.set_function("clear_morph_weight",
		[this](std::string Name)
		{
			EnsureLuaMorphWeightStorage();
			USkeletalMesh* Mesh = GetSkeletalMesh();
			FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
			if (!Asset || Name.empty())
			{
				return;
			}

			const int32 MorphIndex = Asset->FindMorphTargetIndex(FString(Name));
			if (MorphIndex < 0 || MorphIndex >= static_cast<int32>(LuaMorphOverrideMask.size()))
			{
				return;
			}

			LuaMorphOverrideMask[MorphIndex] = 0;

			bLuaMorphOverrideEnabled = false;
			for (uint8 Mask : LuaMorphOverrideMask)
			{
				if (Mask != 0)
				{
					bLuaMorphOverrideEnabled = true;
					break;
				}
			}
		});

	Anim.set_function("clear_morph_weights",
		[this]()
		{
			LuaMorphWeights.clear();
			LuaMorphOverrideMask.clear();
			bLuaMorphOverrideEnabled = false;
		});

	// ── Input edge detection — lua FSM/montage trigger 용 ──
	//   GetKeyDown == "이번 frame 에서 새로 눌림". 매 frame 호출 안전.
	Anim.set_function("is_left_mouse_pressed",
		[]() -> bool { return UGameViewportClient::MakeCurrentGameInputSnapshot().WasPressed(VK_LBUTTON); });
	Anim.set_function("is_left_mouse_down",
		[]() -> bool { return UGameViewportClient::MakeCurrentGameInputSnapshot().IsDown(VK_LBUTTON); });
	Anim.set_function("is_right_mouse_pressed",
		[]() -> bool { return UGameViewportClient::MakeCurrentGameInputSnapshot().WasPressed(VK_RBUTTON); });
	Anim.set_function("is_key_pressed",
		[](int VK) -> bool { return UGameViewportClient::MakeCurrentGameInputSnapshot().WasPressed(VK); });

	// ── AnimGraph build API (Phase 1.6b) — sub-state-machine / 임의 트리 표현 ──
	// 노드는 UAnimInstance::MakeNode 가 OwnedNodes 에 push 후 raw 반환 — lifetime 은 C++ 가 관리.
	// lua 는 raw pointer 핸들만 들고 다님 (light userdata). 다른 build 함수의 인자로 그대로 전달.
	// sub-graph 가 또 다른 state machine 이면 자연스럽게 sub-state-machine 구성.

	// SM 노드 생성 — name 은 디버그용 (현재는 무시).
	Anim.set_function("create_state_machine",
		[this](sol::object /*Name*/) -> FAnimNode_StateMachine*
		{
			return MakeNode<FAnimNode_StateMachine>();
		});

	// Sequence player 노드 — asset path 로드 + 파라미터 박음. None / 실패 시 ref pose fallback.
	Anim.set_function("create_sequence_player",
		[this](std::string Path, float Rate, bool Loop) -> FAnimNode_SequencePlayer*
		{
			FAnimNode_SequencePlayer* P = MakeNode<FAnimNode_SequencePlayer>();
			if (!Path.empty() && Path != "None")
			{
				P->Sequence = FAnimationManager::Get().LoadAnimation(Path);
				if (!P->Sequence)
				{
					UE_LOG("[LuaAnim] create_sequence_player — anim load failed: %s", Path.c_str());
				}
			}
			P->PlayRate = Rate;
			P->bLooping = Loop;
			return P;
		});

	// SM 에 state 추가 — SubGraphOverride 로 임의 노드를 state 의 sub-graph 로 박음.
	Anim.set_function("sm_add_state",
		[this](FAnimNode_StateMachine* SM, std::string Name, FAnimNode_Base* SubGraph)
		{
			if (!SM || !SubGraph)
			{
				UE_LOG("[LuaAnim] sm_add_state '%s' — null SM or SubGraph", Name.c_str());
				return;
			}
			UAnimState* S = UObjectManager::Get().CreateObject<UAnimState>(this);
			S->StateName        = FName(Name.c_str());
			S->SubGraphOverride = SubGraph;
			SM->RegisterState(S);
		});

	// SM 에 transition 등록. "AnyState" → FName::None (legacy 와 동일).
	Anim.set_function("sm_add_transition",
		[](FAnimNode_StateMachine* SM, std::string From, std::string To,
		   sol::protected_function Cond, float BlendTime)
		{
			if (!SM) return;
			FStateTransition T;
			T.From      = (From == "AnyState" || From.empty()) ? FName::None : FName(From.c_str());
			T.To        = FName(To.c_str());
			T.BlendTime = BlendTime;
			T.Condition = [Cond](UAnimInstance*) -> bool
			{
				auto R = Cond();
				if (!R.valid())
				{
					sol::error Err = R;
					UE_LOG("[LuaAnim] sm_add_transition condition error: %s", Err.what());
					return false;
				}
				return R.get<bool>();
			};
			SM->RegisterTransition(T);
		});

	Anim.set_function("sm_set_initial_state",
		[](FAnimNode_StateMachine* SM, std::string Name)
		{
			if (SM) SM->SetInitialState(FName(Name.c_str()));
		});

	// 트리의 root 박기 — UAnimInstance::SetRootNode 가 Initialize 호출.
	// NativeInitializeAnimation 의 fallback (wrapper FSM 사용) 보다 lua 가 명시 호출하는 게 우선.
	Anim.set_function("set_root_node",
		[this](FAnimNode_Base* Root)
		{
			SetRootNode(Root);
		});

	// Slot 노드 생성 — Montage 진입점. SlotName 이 빈 문자열이면 DefaultSlot 사용.
	// 트리 안에 박아 사용. 예: local slot = Anim.create_slot("UpperBody", base_pose).
	// 보통 자동 wrap (NativeInitializeAnimation 끝) 만으로도 DefaultSlot 은 트리에 박힘 —
	// 명시 호출은 추가 slot (UpperBody 등) 또는 트리 위치 명시할 때 사용.
	Anim.set_function("create_slot",
		[this](std::string Name, FAnimNode_Base* Input) -> FAnimNode_Slot*
		{
			FAnimNode_Slot* Slot = MakeNode<FAnimNode_Slot>();
			Slot->SlotName  = Name.empty() ? DefaultMontageSlot : FName(Name.c_str());
			Slot->InputPose = Input;
			return Slot;
		});

	// Ref pose 노드 — 단순 ref pose 출력. LayeredBlend 의 BlendPose 의 InputPose 로
	// 활용 (slot 비어있을 때 effective weight 0 이라 시각 영향 X).
	Anim.set_function("create_ref_pose",
		[this]() -> FAnimNode_RefPose*
		{
			return MakeNode<FAnimNode_RefPose>();
		});

	// Per-bone layered blend — base + blend + bone mask. root bone name 으로 mask BFS 자동.
	// 예: local layer = Anim.create_layered_blend_per_bone(loco, upper_slot, "Spine")
	//     상반신 (Spine 트리) 만 upper_slot 의 montage 영향, 하반신 loco 그대로.
	Anim.set_function("create_layered_blend_per_bone",
		[this](FAnimNode_Base* Base, FAnimNode_Base* Blend, std::string MaskRootBone)
			-> FAnimNode_LayeredBlendPerBone*
		{
			FAnimNode_LayeredBlendPerBone* Layer = MakeNode<FAnimNode_LayeredBlendPerBone>();
			Layer->BasePose    = Base;
			Layer->BlendPose   = Blend;
			Layer->PerBoneMask = BuildBoneMaskFromRoot(GetSkeletalMesh(), MaskRootBone);
			Layer->BlendWeight = 1.0f;   // 자동 weight 는 Blend 노드의 GetEffectiveBlendWeight 로 결정.
			return Layer;
		});

	// BlendListByEnum — N 개의 입력 pose 중 enum 인덱스로 직접 선택. StateMachine 의 단순화
	// 버전 (transitions 없음). 무장/비무장, 자세 enum 같이 명확한 분기에 사용.
	// 예: local bl = Anim.create_blend_list_by_enum(0, 0.2)
	//     Anim.blend_list_add_pose(bl, unarmed_pose)   -- index 0
	//     Anim.blend_list_add_pose(bl, armed_pose)     -- index 1
	//     Anim.blend_list_set_active(bl, self.WeaponEnum)  -- 매 update
	Anim.set_function("create_blend_list_by_enum",
		[this](sol::object InitialIndex, sol::object BlendTime) -> FAnimNode_BlendListByEnum*
		{
			FAnimNode_BlendListByEnum* B = MakeNode<FAnimNode_BlendListByEnum>();
			B->ActiveChildIndex = InitialIndex.is<int>() ? InitialIndex.as<int>() : 0;
			B->BlendTime        = BlendTime.is<float>()  ? BlendTime.as<float>()  : 0.2f;
			return B;
		});

	// 입력 pose 를 enum 인덱스 순서로 append. 등록 순서 = enum value.
	Anim.set_function("blend_list_add_pose",
		[](FAnimNode_BlendListByEnum* B, FAnimNode_Base* Pose)
		{
			if (!B || !Pose) return;
			B->InputPoses.push_back(Pose);
		});

	// 매 update 에서 외부가 활성 enum 값을 갱신. 변경 감지 시 BlendTime 동안 lerp.
	Anim.set_function("blend_list_set_active",
		[](FAnimNode_BlendListByEnum* B, int32 Idx)
		{
			if (B) B->ActiveChildIndex = Idx;
		});
}

// ──────────────────────────────────────────────
// Editor 통합
// ──────────────────────────────────────────────
void ULuaAnimInstance::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);
	if (!PropertyName) return;

	// EditorPropertyWidget 이 Prop.GetName() (internal C++ 멤버 이름) 을 넘기므로
	// DisplayName "Script File" 이 아닌 internal "ScriptFile" 로 매칭해야 한다.
	if (std::strcmp(PropertyName, "ScriptFile") == 0)
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
