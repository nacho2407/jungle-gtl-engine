#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "Object/UClass.h"
#include "Core/Log.h"
#include "Core/ProjectSettings.h"

IMPLEMENT_CLASS(AGameModeBase, AActor)

AGameModeBase::AGameModeBase()
{
	// 기본값 — 서브클래스 생성자가 더 구체 클래스로 덮어쓸 수 있다.
	GameStateClass = AGameStateBase::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
}

void AGameModeBase::BeginPlay()
{
	AActor::BeginPlay();

	// GameState spawn — World 경유로 등록되어 BeginPlay/Tick에 편입된다.
	if (UWorld* World = GetWorld())
	{
		UClass* StateClass = GameStateClass ? GameStateClass : AGameStateBase::StaticClass();
		AActor* Spawned = World->SpawnActorByClass(StateClass);
		GameState = Cast<AGameStateBase>(Spawned);
	}
}

void AGameModeBase::EndPlay()
{
	GameState = nullptr;
	PlayerController = nullptr;
	AActor::EndPlay();
}

void AGameModeBase::StartMatch()
{
	// PlayerController spawn — Editor 월드에선 GameMode 자체가 안 만들어지므로 안전.
	if (UWorld* World = GetWorld())
	{
		UClass* PCClass = PlayerControllerClass ? PlayerControllerClass : APlayerController::StaticClass();
		AActor* Spawned = World->SpawnActorByClass(PCClass);
		PlayerController = Cast<APlayerController>(Spawned);
	}

	AutoPossessFirstPawn();
}

void AGameModeBase::EndMatch()
{
	if (PlayerController)
	{
		PlayerController->UnPossess();
	}
}

UClass* AGameModeBase::ResolveClassFromProjectSettings(UClass* InDefault)
{
	UClass* Result = InDefault;
	const FString& ConfiguredName = FProjectSettings::Get().Game.GameModeClassName;
	if (ConfiguredName.empty())
	{
		return Result;
	}

	UClass* Found = UClass::FindByName(ConfiguredName.c_str());
	if (Found && Found->IsA(AGameModeBase::StaticClass()))
	{
		return Found;
	}

	UE_LOG("[GameMode] GameModeClassName '%s' not found or not a AGameModeBase subclass — using default %s",
		ConfiguredName.c_str(), Result ? Result->GetName() : "(null)");
	return Result;
}

void AGameModeBase::AutoPossessFirstPawn()
{
	if (!PlayerController) return;

	UWorld* World = GetWorld();
	if (!World) return;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn) continue;
		if (!Pawn->GetAutoPossessPlayer()) continue;

		PlayerController->Possess(Pawn);
		UE_LOG("[GameMode] Auto-possessed Pawn: %s", Pawn->GetName().c_str());
		return;
	}

	// 매칭 Pawn 없음 — PC만 살아있고 PossessedPawn은 nullptr.
}
