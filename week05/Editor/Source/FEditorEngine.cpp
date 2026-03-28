#include "FEditorEngine.h"

#include "imgui_impl_dx11.h"
#include "Viewport/EditorViewportClient.h"
#include "Viewport/PreviewViewportClient.h"
#include "Viewport/Viewport.h"
#include "Platform/Windows/Window.h"
#include "Core/Core.h"
#include "Core/ConsoleVariableManager.h"
#include "Scene/Scene.h"
#include "Actor/Actor.h"

#include "Component/CameraComponent.h"

#include "Component/CubeComponent.h"
#include "Object/ObjectFactory.h"
#include "Debug/EngineLog.h"
#include "World/World.h"
#include "imgui_impl_win32.h"
#include "Pawn/EditorCameraPawn.h"
#include "Camera/Camera.h"
#include "Actor/SkySphereActor.h"
namespace
{
	constexpr const char* PreviewSceneContextName = "PreviewScene";

	void InitializeDefaultPreviewScene(CCore* Core)
	{
		if (Core == nullptr)
		{
			return;
		}
		FEditorWorldContext* PreviewContext = Core->GetSceneManager()->CreatePreviewWorldContext(PreviewSceneContextName, 1280, 720);
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

bool FEditorEngine::Initialize(HINSTANCE hInstance)
{
	ImGui_ImplWin32_EnableDpiAwareness();

	if (!FEngine::Initialize(hInstance, L"Jungle Editor", 1280, 720))
	{
		return false;
	}

	return true;
}

FEditorEngine::~FEditorEngine()
{
	//Shutdown();
}

void FEditorEngine::Shutdown()
{
	if (Core && Core->GetViewportClient() == PreviewViewportClient.get())
	{
		Core->SetViewportClient(nullptr);
	}

	// EditorPawn은 Scene 소속이 아니므로 직접 정리
	if (EditorPawn)
	{
		EditorPawn->Destroy();
		EditorPawn = nullptr;
	}

	PreviewViewportClient.reset();

	// ViewportController가 EnhancedInput을 참조하므로, Engine이 해제하기 전에 정리
	ViewportController.Cleanup();

	FEngine::Shutdown();
}

void FEditorEngine::PreInitialize()
{
	FEngineLog::Get().SetCallback([this](const char* Msg)
		{
			EditorUI.GetConsole().AddLog("%s", Msg);
		});
}

void FEditorEngine::PostInitialize()
{
	InitializeDefaultPreviewScene(Core.get());
	PreviewViewportClient = std::make_unique<CPreviewViewportClient>(EditorUI, MainWindow, PreviewSceneContextName);

	FConsoleVariableManager& CVM = FConsoleVariableManager::Get();

	// TArray<FString> VariableNames; 삭제
	// CVM.GetAllNames(VariableNames); 삭제

	// 이렇게 람다로 바로 받아서 등록하도록 변경합니다.
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
	// EditorPawn은 Scene에 등록하지 않음 — FEditorEngine이 직접 소유
	
	EditorPawn = FObjectFactory::ConstructObject<AEditorCameraPawn>(nullptr, "EditorCameraPawn");
	EditorPawn->Initialize();
	Core->GetActiveWorld()->SetActiveCameraComponent(EditorPawn->GetCameraComponent());
	ViewportController.Initialize(
		EditorPawn->GetCameraComponent(),
		Core->GetInputManager(),
		Core->GetEnhancedInputManager());


	SyncViewportClient();
	UE_LOG("EditorEngine initialized");
}

void FEditorEngine::Tick(float DeltaTime)
{
	// Editor Scene에서는 EditorPawn 카메라가 항상 활성화되도록 보장
	// (ClearActors 후 SceneCameraComponent로 폴백된 경우 복원)
	if (EditorPawn && Core && Core->GetScene() && Core->GetScene()->IsEditorScene())
	{
		UCameraComponent* EditorCamera = EditorPawn->GetCameraComponent();
		if (Core->GetActiveWorld()->GetActiveCameraComponent() != EditorCamera)
		{
			Core->GetActiveWorld()->SetActiveCameraComponent(EditorCamera);
		}
	}

	ViewportController.Tick(DeltaTime);
	SyncViewportClient();
}

std::unique_ptr<IViewportClient> FEditorEngine::CreateViewportClient()
{
	InitializeViewportStorage();

	auto Client = std::make_unique<CEditorViewportClient>(*this, EditorUI, MainWindow);
	EditorViewportClientRaw = Client.get();

	return Client;
}

CEditorViewportController* FEditorEngine::GetViewportController()
{
	return &ViewportController;
}

void FEditorEngine::SyncViewportClient()
{
	if (!Core)
	{
		return;
	}

	IViewportClient* TargetViewportClient = ViewportClient.get();
	const FWorldContext* ActiveSceneContext = Core->GetActiveWorldContext();
	if (ActiveSceneContext && ActiveSceneContext->WorldType == ESceneType::Preview && PreviewViewportClient)
	{
		TargetViewportClient = PreviewViewportClient.get();
	}

	if (Core->GetViewportClient() != TargetViewportClient)
	{
		Core->SetViewportClient(TargetViewportClient);
	}
}

void FEditorEngine::InitializeViewportStorage()
{
	if (!Core || !Core->GetRenderer() || !MainWindow)
	{
		return;
	}

	const int32 Width = MainWindow->GetWidth();
	const int32 Height = MainWindow->GetHeight();

	const int32 HalfW = Width / 2;
	const int32 HalfH = Height / 2;

	if (Viewports.size() != MAX_VIEWPORTS)
	{
		Viewports.resize(MAX_VIEWPORTS);
	}

	Viewports[0].SetRect({ 0, 0, HalfW, HalfH });
	Viewports[1].SetRect({ HalfW, 0, Width - HalfW, HalfH });
	Viewports[2].SetRect({ 0, HalfH, HalfW, Height - HalfH });
	Viewports[3].SetRect({ HalfW, HalfH, Width - HalfW, Height - HalfH });

	ID3D11Device* Device = Core->GetRenderer()->GetDevice();
	for (uint32 i = 0; i < 4; ++i)
	{
		Viewports[i].EnsureResources(Device);
	}
}

FViewport* FEditorEngine::FindViewport(FViewportId Id)
{
	if (!EditorViewportClientRaw)
	{
		return nullptr;
	}

	for (FViewportEntry& Entry : EditorViewportClientRaw->GetEntries())
	{
		if (Entry.Id == Id && Entry.bActive)
		{
			return Entry.Viewport;
		}
	}

	return nullptr;
}
