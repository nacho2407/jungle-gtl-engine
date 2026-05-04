#include "Game/GameEngine.h"

#include "Game/GameRenderPipeline.h"
#include "Game/GameMode/GameModeCarGame.h"
#include "Engine/Runtime/EngineInitHooks.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Lua/LuaScriptManager.h"
#include "Profiling/Timer.h"
#include <windows.h>  // VK_ESCAPE
#include "Viewport/Viewport.h"
#include "Viewport/GameViewportClient.h"
#include "Serialization/SceneSaveManager.h"
#include "GameFramework/World.h"
#include "GameFramework/GameModeBase.h"
#include "Object/UClass.h"
#include "Core/ProjectSettings.h"
#include "Core/Log.h"
#include "Lua/GameLuaBindings.h"

IMPLEMENT_CLASS(UGameEngine, UEngine)

void UGameEngine::Init(FWindowsWindow* InWindow)
{
	UEngine::Init(InWindow);

	// Game 모듈 .cpp 들이 static initializer 로 등록해 둔 init 함수들 일괄 실행.
	// (Lua 바인딩, ActorPlacement 등록 등) — EditorEngine::Init 와 동일 경로.
	FEngineInitHooks::RunAll();

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
	// 게임 세션 종료 — 커서 캡처/clip / raw mouse / GameInputSnapshot 정리.
	// 이거 안 부르면 종료 후에도 시스템 커서가 숨김 상태로 남거나 클립 영역이 잔존해
	// 다른 앱 사용 시 마우스가 안 보이는 증상이 생긴다.
	if (GameViewportClient)
	{
		GameViewportClient->EndGameSession();
	}

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

	// ESC 는 World pause 와 무관하게 동작해야 함 (메뉴 토글 자체가 pause 토글이라
	// component-tick 에 두면 닫는 키 입력을 못 잡는다). 등록된 Lua 콜백을 직접 호출.
	if (InputSnapshot.WasPressed(VK_ESCAPE))
	{
		FLuaScriptManager::FireOnEscapePressed();
	}

	// World->Tick / Render 가 모두 끝난 이후에 transition 처리 — Lua callback 안에서
	// 요청이 들어와도 Tick/Render 흐름이 valid 한 액터/컴포넌트로 진행한 뒤 안전하게 destroy.
	ProcessPendingTransition();
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

FString UGameEngine::ResolveSceneFilePath(const FString& InNameOrPath) const
{
	// 이미 .Scene 확장자가 붙은 풀 경로면 그대로 사용. 그렇지 않으면 SceneDir 기준 상대로 풀어준다.
	std::filesystem::path Input(FPaths::ToWide(InNameOrPath));
	const std::wstring Ext = Input.has_extension() ? Input.extension().wstring() : L"";
	if (Input.is_absolute() && std::filesystem::exists(Input))
	{
		return InNameOrPath;
	}

	const std::wstring SceneDir = FSceneSaveManager::GetSceneDirectory();
	std::filesystem::path Resolved = std::filesystem::path(SceneDir) / Input;
	if (Ext.empty())
	{
		Resolved += FSceneSaveManager::SceneExtension;
	}
	return FPaths::ToUtf8(Resolved.wstring());
}

void UGameEngine::LoadStartLevel()
{
	const FString& StartLevel = FProjectSettings::Get().Game.StartLevelName;
	if (StartLevel.empty())
	{
		UE_LOG("[GameEngine] No StartLevelName set in ProjectSettings");
		return;
	}

	const FString FilePath = ResolveSceneFilePath(StartLevel);
	if (!LoadSceneFromPath(FilePath))
	{
		UE_LOG("Failed to load start level: %s", StartLevel.c_str());
	}
}

void UGameEngine::RequestTransitionToScene(const FString& InScenePath)
{
	PendingScenePath = InScenePath;
	bPendingSceneTransition = true;
}

void UGameEngine::ProcessPendingTransition()
{
	if (!bPendingSceneTransition)
	{
		return;
	}
	bPendingSceneTransition = false;

	const FString ScenePath = std::move(PendingScenePath);
	PendingScenePath.clear();

	// Lua 에서 "Map" 같은 이름만 넘겨도 동작하도록 SceneDir/Map.Scene 으로 풀어준다.
	const FString FilePath = ResolveSceneFilePath(ScenePath);

	// 기존 active world 파괴 — EndPlay → 액터/컴포넌트 destruct → PhysicsScene unique_ptr 해제.
	const FName OldHandle = GetActiveWorldHandle();
	DestroyWorldContext(OldHandle);

	// require 캐시된 lua 모듈 (CoroutineManager / ObjRegistry) 이 보유한 죽은-월드 참조 정리.
	// 안 하면 옛 WalkingPerson 의 Wait(30) 코루틴이 새 월드 Tick 에서 만료되며 freed actor
	// 를 deref → FQuat::ToRotator 크래시.
	FLuaScriptManager::FireWorldReset();

	// 새 scene 로드 — World/Level/PhysicsScene 새로 만들고 WorldList push + SetActiveWorld 까지.
	if (!LoadSceneFromPath(FilePath))
	{
		UE_LOG("[GameEngine] TransitionToScene failed: %s", FilePath.c_str());
		return;
	}

	// BeginPlay — UEngine::BeginPlay 와 동일 흐름.
	if (FWorldContext* Ctx = GetWorldContextFromHandle(GetActiveWorldHandle()))
	{
		if (Ctx->World && (Ctx->WorldType == EWorldType::Game || Ctx->WorldType == EWorldType::PIE))
		{
			Ctx->World->BeginPlay();
		}
	}

	// Timer 리셋 — destroy + load + BeginPlay 가 한 frame 안에서 통째로 일어나면 다음
	// Tick 의 dt 가 그 로드 시간만큼 부풀어 PhysX 가 거대한 step (예: 2~3 초) 을 한 번에
	// integrate → 차량이 바닥을 뚫고 추락 등 tunneling 발생. LastTime 을 지금으로 맞춰
	// 다음 frame 의 dt 를 정상 frame 간격으로 회귀시킨다.
	if (FTimer* T = GetTimer())
	{
		T->Initialize();
	}
}

bool UGameEngine::LoadSceneFromPath(const FString& InScenePath)
{
	FWorldContext LoadContext;
	FPerspectiveCameraData CameraData;

	// LoadSceneFromJSON 에 Game 으로 override 전달 — actor deserialize 전에 World 의
	// WorldType 이 Game 으로 set 되어, EditorOnly billboard 컴포넌트의 SceneProxy 가 안
	// 만들어진다. (이 override 없으면 default Editor 라 빌보드 프록시가 생기고, 뒤에서
	// SetWorldType(Game) 해도 이미 만든 프록시는 안 사라짐 — Game 빌드에서 editor 빌보드
	// 노출되는 버그.)
	const EWorldType GameType = EWorldType::Game;
	FSceneSaveManager::LoadSceneFromJSON(InScenePath, LoadContext, CameraData, &GameType);

	if (!LoadContext.World)
	{
		return false;
	}

	LoadContext.WorldType = EWorldType::Game;
	LoadContext.World->SetWorldType(EWorldType::Game);

	// GameMode 주입 — World::BeginPlay 가 이걸 보고 GameMode/GameState/PC 를 spawn 한다.
	// 우선순위: World->WorldSettings.GameModeClassName (scene 파일이 지정) →
	// ProjectSettings.GameModeClassName → AGameModeCarGame fallback. Scene 별로 다른
	// GameMode 적용 가능 (Intro.Scene 의 AGameModeIntro vs Map.Scene 의 AGameModeCarGame).
	UClass* GMClass = nullptr;
	const FString& SceneGMName = LoadContext.World->GetWorldSettings().GameModeClassName;
	if (!SceneGMName.empty())
	{
		UClass* Found = UClass::FindByName(SceneGMName.c_str());
		if (Found && Found->IsA(AGameModeBase::StaticClass()))
		{
			GMClass = Found;
		}
		else
		{
			UE_LOG("[GameEngine] WorldSettings.GameMode = '%s' 가 알 수 없는 클래스 — ProjectSettings 로 fallback",
				SceneGMName.c_str());
		}
	}
	if (!GMClass)
	{
		GMClass = AGameModeBase::ResolveClassFromProjectSettings(AGameModeCarGame::StaticClass());
	}
	LoadContext.World->SetGameModeClass(GMClass);

	WorldList.push_back(LoadContext);
	SetActiveWorld(LoadContext.ContextHandle);


	return true;
}
