#include "Game/GameEngine.h"

#include "Game/GameRenderPipeline.h"
#include "Game/GameMode/GameModeCarGame.h"
#include "Game/Lua/GameLuaBindings.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Lua/LuaScriptManager.h"
#include "Viewport/Viewport.h"
#include "Viewport/GameViewportClient.h"
#include "Serialization/SceneSaveManager.h"
#include "GameFramework/World.h"
#include "GameFramework/GameModeBase.h"
#include "Object/UClass.h"
#include "Core/ProjectSettings.h"
#include "Core/Log.h"

IMPLEMENT_CLASS(UGameEngine, UEngine)

void UGameEngine::Init(FWindowsWindow* InWindow)
{
	UEngine::Init(InWindow);

	// Engine 측 LuaScriptManager 가 일반 바인딩만 등록하므로, 여기서 game-특화
	// usertype/enum/global 을 추가 등록. 어떤 LuaScriptComponent 의 BeginPlay 보다도 앞서야 함.
	RegisterGameLuaBindings(FLuaScriptManager::GetState());

	FProjectSettings::Get().LoadFromFile(FProjectSettings::GetDefaultPath());

	StandaloneViewport = new FViewport();
	StandaloneViewport->Initialize(
		Renderer.GetFD3DDevice().GetDevice(),
		static_cast<uint32>(InWindow->GetWidth()),
		static_cast<uint32>(InWindow->GetHeight()));

	GameViewportClient = UObjectManager::Get().CreateObject<UGameViewportClient>();
	GameViewportClient->SetOwnerWindow(InWindow->GetHWND());

	FRect ViewportRect{ 0, 0, static_cast<float>(InWindow->GetWidth()), static_cast<float>(InWindow->GetHeight()) };
	GameViewportClient->SetCursorClipRect(ViewportRect);
	GameViewportClient->BeginGameSession(StandaloneViewport);
	GameViewportClient->SetInputPossessed(true);

	LoadStartLevel();

	SetRenderPipeline(std::make_unique<FGameRenderPipeline>(this, Renderer));
}

void UGameEngine::Shutdown()
{
	if (StandaloneViewport)
	{
		delete StandaloneViewport;
		StandaloneViewport = nullptr;
	}

	UEngine::Shutdown();
}

void UGameEngine::Tick(float DeltaTime)
{
	UEngine::Tick(DeltaTime);

	InputSystem& Input = InputSystem::Get();
	const FInputSystemSnapshot InputSnapshot = Input.MakeSnapshot();

	if (GameViewportClient)
	{
		GameViewportClient->ProcessInput(InputSnapshot, DeltaTime);
	}
}

void UGameEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);

	if (StandaloneViewport)
	{
		StandaloneViewport->RequestResize(Width, Height);
	
		FRect ViewportRect{ 0, 0, static_cast<float>(Width), static_cast<float>(Height) };
		GameViewportClient->SetCursorClipRect(ViewportRect);
	}
}

void UGameEngine::LoadStartLevel()
{
	const FString& StartLevel = FProjectSettings::Get().Game.StartLevelName;
	if (StartLevel.empty())
	{
		UE_LOG("[GameEngine] No StartLevelName set in ProjectSettings");
		return;
	}

	std::filesystem::path ScenePath = std::filesystem::path(FSceneSaveManager::GetSceneDirectory())
		/ (FPaths::ToWide(StartLevel) + FSceneSaveManager::SceneExtension);
	FString FilePath = FPaths::ToUtf8(ScenePath.wstring());

	if (!LoadSceneFromPath(FilePath))
	{
		UE_LOG("Failed to load start level: %s", StartLevel.c_str());
	}
}

bool UGameEngine::LoadSceneFromPath(const FString& InScenePath)
{
	FWorldContext LoadContext;
	FPerspectiveCameraData CameraData;
	FSceneSaveManager::LoadSceneFromJSON(InScenePath, LoadContext, CameraData);

	if (!LoadContext.World)
	{
		return false;
	}

	LoadContext.WorldType = EWorldType::Game;
	LoadContext.World->SetWorldType(EWorldType::Game);

	// GameMode 주입 — World::BeginPlay가 이걸 보고 GameMode/GameState/PC를 spawn한다.
	// ProjectSettings 우선, 없거나 잘못된 이름이면 AGameModeCarGame으로 fallback.
	UClass* GMClass = AGameModeBase::ResolveClassFromProjectSettings(AGameModeCarGame::StaticClass());
	LoadContext.World->SetGameModeClass(GMClass);

	WorldList.push_back(LoadContext);
	SetActiveWorld(LoadContext.ContextHandle);


	return true;
}
