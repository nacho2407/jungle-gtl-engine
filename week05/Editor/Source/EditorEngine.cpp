#include "EditorEngine.h"

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "Actor/Actor.h"
#include "Actor/SkySphereActor.h"
#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Component/CubeComponent.h"
#include "Core/ConsoleVariableManager.h"
#include "Core/Engine.h"
#include "Debug/EngineLog.h"
#include "Object/ObjectFactory.h"
#include "Pawn/EditorCameraPawn.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Scene/Scene.h"
#include "UI/EditorViewportClient.h"
#include "UI/PreviewViewportClient.h"
#include "World/World.h"

namespace
{
	constexpr const char* PreviewSceneContextName = "PreviewScene";

	void InitializeDefaultPreviewScene(FEngine* Engine)
	{
		if (Engine == nullptr)
		{
			return;
		}

		FEditorWorldContext* PreviewContext = Engine->CreatePreviewWorldContext(PreviewSceneContextName, 1280, 720);
		if (PreviewContext == nullptr || PreviewContext->World == nullptr)
		{
			return;
		}

		UWorld* PreviewWorld = PreviewContext->World;
		if (PreviewWorld->GetActors().empty())
		{
			AActor* PreviewActor = PreviewWorld->SpawnActor<AActor>("PreviewCube");
			if (PreviewActor)
			{
				UCubeComponent* PreviewComponent = FObjectFactory::ConstructObject<UCubeComponent>(PreviewActor);
				PreviewActor->AddOwnedComponent(PreviewComponent);
				PreviewActor->SetActorLocation({ 0.0f, 0.0f, 0.0f });
			}
		}

		if (UCameraComponent* PreviewCamera = PreviewWorld->GetActiveCameraComponent())
		{
			PreviewCamera->GetCamera()->SetPosition({ -8.0f, -8.0f, 6.0f });
			PreviewCamera->GetCamera()->SetRotation(45.0f, -20.0f);
			PreviewCamera->SetFov(50.0f);
		}
	}
}

FEditorEngine::~FEditorEngine() = default;

void FEditorEngine::Shutdown()
{
	if (GetViewportClient() == PreviewViewportClient.get())
	{
		SetViewportClient(nullptr);
	}

	// EditorPawn은 Scene 소유가 아니므로 엔진 종료 전에 직접 정리한다.
	if (EditorPawn)
	{
		EditorPawn->Destroy();
		EditorPawn = nullptr;
	}

	PreviewViewportClient.reset();

	// ViewportController가 입력 시스템을 참조하므로 엔진 종료 전에 먼저 정리한다.
	ViewportController.Cleanup();

	FEngine::Shutdown();
	HostWindow = nullptr;
	SelectedActor = nullptr;
}

void FEditorEngine::SetSelectedActor(AActor* InActor)
{
	SelectedActor = InActor;
}

AActor* FEditorEngine::GetSelectedActor() const
{
	if (SelectedActor && SelectedActor->IsPendingDestroy())
	{
		return nullptr;
	}

	return SelectedActor.Get();
}

void FEditorEngine::PreInitialize()
{
	ImGui_ImplWin32_EnableDpiAwareness();

	FEngineLog::Get().SetCallback([this](const char* Msg)
	{
		EditorUI.GetConsole().AddLog("%s", Msg);
	});
}

void FEditorEngine::OnHostWindowReady(FWindowsWindow* InMainWindow)
{
	HostWindow = InMainWindow;
}

void FEditorEngine::PostInitialize()
{
	InitializeDefaultPreviewScene(this);
	PreviewViewportClient = std::make_unique<CPreviewViewportClient>(EditorUI, HostWindow, PreviewSceneContextName);

	FConsoleVariableManager& CVM = FConsoleVariableManager::Get();

	// 현재 등록된 콘솔 명령 이름을 UI 자동완성 목록에 등록한다.
	CVM.GetAllNames([this](const FString& Name)
	{
		EditorUI.GetConsole().RegisterCommand(Name.c_str());
	});

	EditorUI.GetConsole().SetCommandHandler([](const char* CommandLine)
	{
		FString Result;
		if (FConsoleVariableManager::Get().Execute(CommandLine, Result))
		{
			FEngineLog::Get().Log("%s", Result.c_str());
		}
		else
		{
			FEngineLog::Get().Log("[error] Unknown command: '%s'", CommandLine);
		}
	});

	// EditorPawn은 Scene에 자동 등록되지 않으므로 FEditorEngine이 직접 소유한다.
	EditorPawn = FObjectFactory::ConstructObject<AEditorCameraPawn>(nullptr, "EditorCameraPawn");
	GetActiveWorld()->SetActiveCameraComponent(EditorPawn->GetCameraComponent());
	ViewportController.Initialize(
		EditorPawn->GetCameraComponent(),
		GetInputManager(),
		GetEnhancedInputManager());

	SyncViewportClient();
	UE_LOG("EditorEngine initialized");
}

void FEditorEngine::Tick(float DeltaTime)
{
	// 에디터 씬에서는 EditorPawn 카메라가 항상 활성 카메라가 되도록 유지한다.
	if (EditorPawn && GetScene() && GetScene()->IsEditorScene())
	{
		UCameraComponent* EditorCamera = EditorPawn->GetCameraComponent();
		if (GetActiveWorld()->GetActiveCameraComponent() != EditorCamera)
		{
			GetActiveWorld()->SetActiveCameraComponent(EditorCamera);
		}
	}

	ViewportController.Tick(DeltaTime);
	SyncViewportClient();
}

std::unique_ptr<IViewportClient> FEditorEngine::CreateViewportClient()
{
	return std::make_unique<CEditorViewportClient>(EditorUI, HostWindow);
}

CEditorViewportController* FEditorEngine::GetViewportController()
{
	return &ViewportController;
}

void FEditorEngine::SyncViewportClient()
{
	if (!GetActiveWorldContext())
	{
		return;
	}

	IViewportClient* TargetViewportClient = ViewportClient.get();
	const FWorldContext* ActiveSceneContext = GetActiveWorldContext();
	if (ActiveSceneContext && ActiveSceneContext->WorldType == ESceneType::Preview && PreviewViewportClient)
	{
		TargetViewportClient = PreviewViewportClient.get();
	}

	if (GetViewportClient() != TargetViewportClient)
	{
		SetViewportClient(TargetViewportClient);
	}
}
