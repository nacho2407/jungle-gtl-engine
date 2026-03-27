#include "GameEngine.h"

#include "ViewportClient.h"

std::unique_ptr<IViewportClient> FGameEngine::CreateViewportClient()
{
	return std::make_unique<CGameViewportClient>();
}
