#include "GameEngine.h"

#include "ViewportClient.h"
#include "World/World.h"

bool FGameEngine::InitializeWorlds()
{
	const float AspectRatio = GetWindowAspectRatio();
	return CreateWorldContext("GameScene", EWorldType::Game, AspectRatio, true) != nullptr;
}

std::unique_ptr<IViewportClient> FGameEngine::CreateViewportClient()
{
	return std::make_unique<FGameViewportClient>();
}

void FGameEngine::TickWorlds(float DeltaTime)
{
	if (UWorld* GameWorld = GetGameWorld())
	{
		GameWorld->Tick(DeltaTime);
	}
}
