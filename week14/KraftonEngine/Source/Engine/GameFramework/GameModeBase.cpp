#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/World.h"
#include "Object/UClass.h"

IMPLEMENT_CLASS(AGameModeBase, AActor)

AGameModeBase::AGameModeBase()
{
	// 기본값 — 서브클래스 생성자가 더 구체 클래스로 덮어쓸 수 있다.
	GameStateClass = AGameStateBase::StaticClass();
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
	AActor::EndPlay();
}

void AGameModeBase::StartMatch()
{
	// 베이스 — 서브클래스가 페이즈 진입 등을 처리.
}

void AGameModeBase::EndMatch()
{
	// 베이스 — 서브클래스가 종료 처리.
}
