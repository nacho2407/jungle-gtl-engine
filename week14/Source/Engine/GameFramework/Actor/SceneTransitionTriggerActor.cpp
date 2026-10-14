#include "GameFramework/Actor/SceneTransitionTriggerActor.h"

#include "Component/Shape/BoxComponent.h"
#include "Core/Logging/Log.h"
#include "Core/Types/EngineTypes.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Lua/LuaScriptManager.h"
#include "Math/Matrix.h"
#include "Runtime/Engine.h"

#include <algorithm>
#include <cmath>

void ASceneTransitionTriggerActor::InitDefaultComponents()
{
	BoxComponent = AddComponent<UBoxComponent>();
	SetRootComponent(BoxComponent);
	BoxComponent->SetBoxExtent(FVector(1.0f, 1.0f, 1.0f));
	// PhysX 미사용 — 매 Tick 에서 직접 OBB 포함 검사. 다른 액터/Ragdoll 과의 물리적 충돌
	// 없이 통과시키고 가시 영역만 갖는다.
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxComponent->SetGenerateOverlapEvents(false);
	BoxComponent->SetSimulatePhysics(false);
	BoxComponent->SetEnableGravity(false);
}

void ASceneTransitionTriggerActor::PostDuplicate()
{
	Super::PostDuplicate();
	BoxComponent = Cast<UBoxComponent>(GetRootComponent());
	bCountingDown = false;
	bConsumed = false;
	bFadeOutStarted = false;
	bLoadingScreenShown = false;
	ElapsedSinceEnter = 0.0f;
	MissingRequirementDialogueTimer = 0.0f;
}

void ASceneTransitionTriggerActor::BeginPlay()
{
	// 코드/씬 양쪽 경로 모두 보정 — 직렬화 값이 들어왔어도 PhysX 가 끼지 않도록 collision off.
	if (!GetRootComponent())
	{
		InitDefaultComponents();
	}
	if (!BoxComponent)
	{
		BoxComponent = Cast<UBoxComponent>(GetRootComponent());
	}
	if (BoxComponent)
	{
		BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoxComponent->SetGenerateOverlapEvents(false);
		BoxComponent->SetSimulatePhysics(false);
	}

	Super::BeginPlay();
}

void ASceneTransitionTriggerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bConsumed || !BoxComponent)
	{
		return;
	}

	if (MissingRequirementDialogueTimer > 0.0f)
	{
		MissingRequirementDialogueTimer -= DeltaTime;
		if (MissingRequirementDialogueTimer <= 0.0f)
		{
			MissingRequirementDialogueTimer = 0.0f;
			HideMissingRequirementDialogue();
		}
	}

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPossessedPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const bool bInside = IsPawnInsideBox(Pawn);

	if (!bCountingDown)
	{
		if (bInside)
		{
			if (!HasRequiredPickedUpItem())
			{
				ShowMissingRequirementDialogue();
				return;
			}
			bCountingDown = true;
			ElapsedSinceEnter = 0.0f;
			// 트리거 진입 즉시 fade-out 을 시작해 전환 입력에 바로 반응합니다.
			BeginFadeOut(PC->GetPlayerCameraManager());
		}
		return;
	}

	// fade-out 시작 후엔 이미 시각적으로 commit 됐으므로 박스 이탈과 무관하게 그대로 진행.
	if (!bInside && !bFadeOutStarted)
	{
		bCountingDown = false;
		ElapsedSinceEnter = 0.0f;
		return;
	}

	ElapsedSinceEnter += DeltaTime;

	// 타임라인: trigger 즉시 fade-out → loading screen 최소 표시 → transition + 새 scene fade-in.
	if (!bLoadingScreenShown && ElapsedSinceEnter >= FadeOutDuration)
	{
		ShowLoadingScreen();
	}

	const float TotalTimeToFire = FadeOutDuration + LoadingScreenDuration;
	if (ElapsedSinceEnter >= TotalTimeToFire)
	{
		FireTransition();
	}
}

void ASceneTransitionTriggerActor::BeginFadeOut(APlayerCameraManager* CamMgr)
{
	// fade duration 이 0 이어도 이후 transition 까지의 '진행 중' 상태를 유지하기 위해
	// bFadeOutStarted 는 켠다 — 이 플래그 이후엔 박스 이탈 시 cancel 되지 않는다.
	bFadeOutStarted = true;
	if (!CamMgr || FadeOutDuration <= 0.0f) return;

	// alpha 0 → 1. bHoldWhenFinished=true 라 fade 가 transition 전 짧게 끝나도 검은 화면 유지.
	CamMgr->StartCameraFade(0.0f, 1.0f, FadeOutDuration, FLinearColor::Black(),
		/*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/true);
}

void ASceneTransitionTriggerActor::ShowLoadingScreen()
{
	bLoadingScreenShown = true;
	if (GEngine)
	{
		GEngine->ShowTransitionLoadingScreen();
	}
}

bool ASceneTransitionTriggerActor::IsPawnInsideBox(const APawn* Pawn) const
{
	if (!Pawn || !BoxComponent) return false;

	const FVector PawnLocation = Pawn->GetActorLocation();

	// Box의 world inverse matrix로 local space 좌표를 얻으면 회전/스케일이 자동 반영된다.
	const FMatrix& InvWorld = BoxComponent->GetWorldInverseMatrix();
	const FVector LocalPawn = InvWorld.TransformPositionWithW(PawnLocation);

	// SetBoxExtent로 등록된 unscaled extent — world matrix가 scale을 이미 적용해 주므로
	// local space 에서는 unscaled 값으로 비교한다.
	const FVector Extent = BoxComponent->GetUnscaledBoxExtent();

	return std::abs(LocalPawn.X) <= Extent.X
		&& std::abs(LocalPawn.Y) <= Extent.Y
		&& std::abs(LocalPawn.Z) <= Extent.Z;
}

FString ASceneTransitionTriggerActor::GetEffectiveRequiredPickedUpItemId() const
{
	if (!RequiredPickedUpItemId.empty())
	{
		return RequiredPickedUpItemId;
	}

	if (TargetScene == "FL_Level3" || TargetScene == "FL_Level3.Scene" || TargetScene == "Content/Scene/FL_Level3.Scene")
	{
		return "ssd_drive";
	}

	return "";
}

FString ASceneTransitionTriggerActor::GetEffectiveMissingRequirementDialogue() const
{
	if (!MissingRequirementDialogue.empty())
	{
		return MissingRequirementDialogue;
	}

	if (TargetScene == "FL_Level3" || TargetScene == "FL_Level3.Scene" || TargetScene == "Content/Scene/FL_Level3.Scene")
	{
		return "완벽하게 임무를 수행하기 위해서는 맥커슨을 처치해야 한다.";
	}

	return "";
}

bool ASceneTransitionTriggerActor::HasRequiredPickedUpItem() const
{
	const FString RequiredItemId = GetEffectiveRequiredPickedUpItemId();
	if (RequiredItemId.empty())
	{
		return true;
	}

	sol::state& Lua = FLuaScriptManager::GetState();
	sol::table Globals = Lua.globals();
	sol::object PickedUpItemsObject = Globals["PickedUpItems"];
	if (!PickedUpItemsObject.valid() || PickedUpItemsObject.get_type() != sol::type::table)
	{
		return false;
	}

	sol::table PickedUpItems = PickedUpItemsObject.as<sol::table>();
	sol::object PickedUpObject = PickedUpItems[RequiredItemId.c_str()];
	return PickedUpObject.valid()
		&& PickedUpObject.get_type() == sol::type::boolean
		&& PickedUpObject.as<bool>();
}

void ASceneTransitionTriggerActor::ShowMissingRequirementDialogue()
{
	const FString DialogueText = GetEffectiveMissingRequirementDialogue();
	if (DialogueText.empty() || MissingRequirementDialogueTimer > 0.0f)
	{
		return;
	}

	sol::state& Lua = FLuaScriptManager::GetState();
	sol::protected_function Require = Lua["require"];
	if (!Require.valid())
	{
		return;
	}

	sol::protected_function_result RequireResult = Require("HUD/WeaponHud");
	if (!RequireResult.valid())
	{
		sol::error Error = RequireResult;
		UE_LOG("[SceneTransitionTrigger] WeaponHUD require failed: %s", Error.what());
		return;
	}

	sol::object ModuleObject = RequireResult.get<sol::object>();
	if (!ModuleObject.valid() || ModuleObject.get_type() != sol::type::table)
	{
		return;
	}

	sol::table WeaponHud = ModuleObject.as<sol::table>();
	sol::object ShowObject = WeaponHud["ShowDialogue"];
	if (!ShowObject.valid() || ShowObject.get_type() != sol::type::function)
	{
		return;
	}

	sol::table Config = Lua.create_table();
	Config["width"] = 1120.0f;
	Config["height"] = 54.0f;
	Config["fontSize"] = 21.0f;
	Config["lineHeight"] = 54.0f;
	Config["opacity"] = 1.0f;
	Config["weight"] = 700;

	sol::protected_function ShowDialogue = ShowObject.as<sol::protected_function>();
	sol::protected_function_result ShowResult = ShowDialogue(DialogueText, Config);
	if (!ShowResult.valid())
	{
		sol::error Error = ShowResult;
		UE_LOG("[SceneTransitionTrigger] WeaponHUD ShowDialogue failed: %s", Error.what());
		return;
	}

	MissingRequirementDialogueTimer = (std::max)(0.1f, MissingRequirementDialogueDuration);
}

void ASceneTransitionTriggerActor::HideMissingRequirementDialogue()
{
	sol::state& Lua = FLuaScriptManager::GetState();
	sol::protected_function Require = Lua["require"];
	if (!Require.valid())
	{
		return;
	}

	sol::protected_function_result RequireResult = Require("HUD/WeaponHud");
	if (!RequireResult.valid())
	{
		return;
	}

	sol::object ModuleObject = RequireResult.get<sol::object>();
	if (!ModuleObject.valid() || ModuleObject.get_type() != sol::type::table)
	{
		return;
	}

	sol::table WeaponHud = ModuleObject.as<sol::table>();
	sol::object HideObject = WeaponHud["HideDialogue"];
	if (!HideObject.valid() || HideObject.get_type() != sol::type::function)
	{
		return;
	}

	sol::protected_function HideDialogue = HideObject.as<sol::protected_function>();
	sol::protected_function_result HideResult = HideDialogue();
	if (!HideResult.valid())
	{
		sol::error Error = HideResult;
		UE_LOG("[SceneTransitionTrigger] WeaponHUD HideDialogue failed: %s", Error.what());
	}
}

void ASceneTransitionTriggerActor::FireTransition()
{
	if (bConsumed) return;

	if (TargetScene.empty())
	{
		UE_LOG("[SceneTransitionTrigger] %s — TargetScene 비어 있어 전이 생략",
			GetFName().ToString().c_str());
		if (bTriggerOnce) bConsumed = true;
		bCountingDown = false;
		return;
	}

	if (!GEngine)
	{
		return;
	}

	UE_LOG("[SceneTransitionTrigger] %s -> %s (fadeOut %.2fs, loading %.2fs, fadeIn %.2fs)",
		GetFName().ToString().c_str(), TargetScene.c_str(),
		FadeOutDuration, LoadingScreenDuration, FadeInDuration);

	// 새 scene 의 PlayerCameraManager 가 만들어진 직후 적용될 fade-in 예약.
	// (현재 PC/CamMgr 는 이 호출 뒤 world destroy 와 함께 사라지므로 엔진 측 상태로 보관해야 한다.)
	if (FadeInDuration > 0.0f)
	{
		GEngine->SetPendingFadeIn(FadeInDuration, FLinearColor::Black());
	}

	GEngine->RequestTransitionToScene(TargetScene);

	if (bTriggerOnce)
	{
		bConsumed = true;
	}
	bCountingDown = false;
	bLoadingScreenShown = false;
	ElapsedSinceEnter = 0.0f;
}
