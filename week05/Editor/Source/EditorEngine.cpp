#include "EditorEngine.h"

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/ConsoleVariableManager.h"
#include "Core/Engine.h"
#include "Debug/EngineLog.h"
#include "Asset/ObjManager.h"
#include "Object/ObjectFactory.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Scene/Scene.h"
#include "UI/EditorViewportClient.h"
#include "UI/PreviewViewportClient.h"
#include "World/World.h"

namespace
{
	constexpr const char* PreviewSceneContextName = "PreviewScene";

	const TArray<FWorldContext*>& GetEmptyPreviewWorldContexts()
	{
		static TArray<FWorldContext*> EmptyPreviewWorldContexts;
		return EmptyPreviewWorldContexts;
	}

	void InitializeDefaultPreviewScene(FEditorEngine* Engine)
	{
		if (Engine == nullptr)
		{
			return;
		}

		FWorldContext* PreviewContext = Engine->CreatePreviewWorldContext(PreviewSceneContextName, 1280, 720);
		if (PreviewContext == nullptr || PreviewContext->World == nullptr)
		{
			return;
		}

		UWorld* PreviewWorld = PreviewContext->World;
		if (PreviewWorld->GetActors().empty())
		{
			/*AActor* PreviewActor = PreviewWorld->SpawnActor<AActor>("PreviewCube");
			if (PreviewActor)
			{
				UStaticMeshComponent* PreviewComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>(PreviewActor);
				PreviewActor->AddOwnedComponent(PreviewComponent);
				PreviewActor->SetRootComponent(PreviewComponent);

				PreviewComponent->SetStaticMesh(FObjManager::GetPrimitiveCube());
				PreviewActor->SetActorLocation({ 0.0f, 0.0f, 0.0f });
			}*/
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
	FEngineLog::Get().SetCallback({});

	if (GetViewportClient() == PreviewViewportClient.get())
	{
		SetViewportClient(nullptr);
	}

	PreviewViewportClient.reset();
	CameraSubsystem.Shutdown();
	SelectionSubsystem.Shutdown();
	ReleaseEditorWorlds();

	FEngine::Shutdown();
}

void FEditorEngine::SetSelectedActor(AActor* InActor)
{
	SelectionSubsystem.SetSelectedActor(InActor);
}

AActor* FEditorEngine::GetSelectedActor() const
{
	return SelectionSubsystem.GetSelectedActor();
}

void FEditorEngine::ActivateEditorScene()
{
	ActiveEditorWorldContext = (EditorWorldContext && EditorWorldContext->World) ? EditorWorldContext : nullptr;
}

bool FEditorEngine::ActivatePreviewScene(const FString& ContextName)
{
	FWorldContext* PreviewContext = FindPreviewWorld(ContextName);
	if (PreviewContext == nullptr)
	{
		return false;
	}

	ActiveEditorWorldContext = PreviewContext;
	return true;
}

UScene* FEditorEngine::GetEditorScene() const
{
	return (EditorWorldContext && EditorWorldContext->World) ? EditorWorldContext->World->GetScene() : nullptr;
}

UScene* FEditorEngine::GetPreviewScene(const FString& ContextName) const
{
	const FWorldContext* Context = FindPreviewWorld(ContextName);
	return (Context && Context->World) ? Context->World->GetScene() : nullptr;
}

UWorld* FEditorEngine::GetEditorWorld() const
{
	return EditorWorldContext ? EditorWorldContext->World : nullptr;
}

const TArray<FWorldContext*>& FEditorEngine::GetPreviewWorldContexts() const
{
	return PreviewWorldContexts.empty() ? GetEmptyPreviewWorldContexts() : PreviewWorldContexts;
}

FWorldContext* FEditorEngine::CreatePreviewWorldContext(const FString& ContextName, int32 Width, int32 Height)
{
	if (ContextName.empty())
	{
		return nullptr;
	}

	if (FWorldContext* ExistingContext = FindPreviewWorld(ContextName))
	{
		return ExistingContext;
	}

	const float AspectRatio = (Height > 0) ? (static_cast<float>(Width) / static_cast<float>(Height)) : 1.0f;
	FWorldContext* PreviewContext = CreateWorldContext(ContextName, EWorldType::Preview, AspectRatio, false);
	if (!PreviewContext)
	{
		return nullptr;
	}

	PreviewWorldContexts.push_back(PreviewContext);
	return PreviewContext;
}

UScene* FEditorEngine::GetScene() const
{
	return GetActiveScene();
}

UScene* FEditorEngine::GetActiveScene() const
{
	UWorld* ActiveWorld = GetActiveWorld();
	return ActiveWorld ? ActiveWorld->GetScene() : nullptr;
}

UWorld* FEditorEngine::GetActiveWorld() const
{
	return ActiveEditorWorldContext ? ActiveEditorWorldContext->World : FEngine::GetActiveWorld();
}

const FWorldContext* FEditorEngine::GetActiveWorldContext() const
{
	return ActiveEditorWorldContext ? ActiveEditorWorldContext : FEngine::GetActiveWorldContext();
}

void FEditorEngine::HandleResize(int32 Width, int32 Height)
{
	FEngine::HandleResize(Width, Height);

	if (Width == 0 || Height == 0)
	{
		return;
	}

	UpdateEditorWorldAspectRatio(static_cast<float>(Width) / static_cast<float>(Height));
}

void FEditorEngine::PreInitialize()
{
	// 에디터 UI가 올라오기 전에 DPI와 로그 연결만 먼저 준비한다.
	ImGui_ImplWin32_EnableDpiAwareness();

	FEngineLog::Get().SetCallback([this](const char* Msg)
	{
		EditorUI.GetConsole().AddLog("%s", Msg);
	});
}

void FEditorEngine::BindHost(FWindowsWindow* InMainWindow)
{
	// 실제 UI/뷰포트 생성은 뒤 단계에서 하고, 여기서는 창 참조만 저장한다.
	EditorUI.SetupWindow(InMainWindow);
}

bool FEditorEngine::InitializeWorlds(int32 Width, int32 Height)
{
	return InitEditorWorlds(Width, Height);
}

bool FEditorEngine::InitializeMode()
{
	// 에디터 전용 초기화는 규약상 이 단계에서만 수행한다.
	if (!InitEditorPreview())
	{
		return false;
	}

	InitEditorConsole();

	if (!InitEditorCamera())
	{
		return false;
	}

	InitEditorViewportRouting();
	return true;
}

void FEditorEngine::FinalizeInitialize()
{
	// 모드 전용 초기화가 모두 끝난 뒤 마지막 상태를 기록한다.
	UE_LOG("EditorEngine initialized");
}

void FEditorEngine::Tick(float DeltaTime)
{
	CameraSubsystem.Tick(GetActiveWorld(), GetScene(), DeltaTime);
	SyncViewportClient();
}

void FEditorEngine::TickWorlds(float DeltaTime)
{
	if (UWorld* ActiveWorld = GetActiveWorld())
	{
		ActiveWorld->Tick(DeltaTime);
	}
}

std::unique_ptr<IViewportClient> FEditorEngine::CreateViewportClient()
{
	return std::make_unique<FEditorViewportClient>(EditorUI);
}

FEditorViewportController* FEditorEngine::GetViewportController()
{
	return CameraSubsystem.GetViewportController();
}

bool FEditorEngine::InitEditorPreview()
{
	// 에디터가 항상 접근 가능한 기본 프리뷰 월드와 프리뷰 뷰포트를 준비한다.
	InitializeDefaultPreviewScene(this);
	PreviewViewportClient = std::make_unique<FPreviewViewportClient>(EditorUI, PreviewSceneContextName);
	return PreviewViewportClient != nullptr;
}

void FEditorEngine::InitEditorConsole()
{
	FConsoleVariableManager& CVM = FConsoleVariableManager::Get();

	// 현재 등록된 콘솔 변수/명령을 UI 자동완성 목록에 반영한다.
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
}

bool FEditorEngine::InitEditorCamera()
{
	// 에디터 카메라는 월드가 준비된 뒤에만 생성할 수 있다.
	return CameraSubsystem.Initialize(GetActiveWorld(), GetInputManager(), GetEnhancedInputManager());
}

void FEditorEngine::InitEditorViewportRouting()
{
	// 초기 활성 월드가 Editor/Preview 중 무엇인지에 따라 적절한 뷰포트를 고른다.
	SyncViewportClient();
}

bool FEditorEngine::InitEditorWorlds(int32 Width, int32 Height)
{
	const float AspectRatio = (Height > 0)
		? (static_cast<float>(Width) / static_cast<float>(Height))
		: 1.0f;

	EditorWorldContext = CreateWorldContext("EditorScene", EWorldType::Editor, AspectRatio, true);
	if (!EditorWorldContext)
	{
		return false;
	}

	ActivateEditorScene();
	return true;
}

void FEditorEngine::ReleaseEditorWorlds()
{
	ActiveEditorWorldContext = nullptr;

	for (FWorldContext* PreviewContext : PreviewWorldContexts)
	{
		DestroyWorldContext(PreviewContext);
	}
	PreviewWorldContexts.clear();

	DestroyWorldContext(EditorWorldContext);
	EditorWorldContext = nullptr;
}

FWorldContext* FEditorEngine::FindPreviewWorld(const FString& ContextName)
{
	for (FWorldContext* Context : PreviewWorldContexts)
	{
		if (Context && Context->ContextName == ContextName)
		{
			return Context;
		}
	}

	return nullptr;
}

const FWorldContext* FEditorEngine::FindPreviewWorld(const FString& ContextName) const
{
	for (const FWorldContext* Context : PreviewWorldContexts)
	{
		if (Context && Context->ContextName == ContextName)
		{
			return Context;
		}
	}

	return nullptr;
}

void FEditorEngine::UpdateEditorWorldAspectRatio(float AspectRatio)
{
	UpdateWorldAspectRatio(EditorWorldContext ? EditorWorldContext->World : nullptr, AspectRatio);

	for (FWorldContext* PreviewContext : PreviewWorldContexts)
	{
		UpdateWorldAspectRatio(PreviewContext ? PreviewContext->World : nullptr, AspectRatio);
	}
}

void FEditorEngine::SyncViewportClient()
{
	if (!GetActiveWorldContext())
	{
		return;
	}

	IViewportClient* TargetViewportClient = ViewportClient.get();
	const FWorldContext* ActiveSceneContext = GetActiveWorldContext();
	if (ActiveSceneContext && ActiveSceneContext->WorldType == EWorldType::Preview && PreviewViewportClient)
	{
		TargetViewportClient = PreviewViewportClient.get();
	}

	if (GetViewportClient() != TargetViewportClient)
	{
		SetViewportClient(TargetViewportClient);
	}
}
