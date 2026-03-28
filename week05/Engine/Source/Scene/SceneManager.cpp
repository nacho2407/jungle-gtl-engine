#include "SceneManager.h"
#include "Scene/Scene.h"
#include "World/World.h"
#include "Object/ObjectFactory.h"
#include "Renderer/Renderer.h"
#include "Component/CameraComponent.h"
#include "Camera/Camera.h"

FSceneManager::~FSceneManager()
{
	Release();
}

bool FSceneManager::Initialize(float AspectRatio, ESceneType StartupSceneType, CRenderer* InRenderer)
{
	Renderer = InRenderer;

	FWorldContext* StartupContext = &GameWorldContext;
	FString ContextName = "GameScene";

	if (StartupSceneType == ESceneType::Editor)
	{
		StartupContext = &EditorWorldContext;
		ContextName = "EditorScene";
	}

	if (!CreateWorldContext(*StartupContext, ContextName, StartupSceneType, AspectRatio))
	{
		return false;
	}

	ActiveWorldContext = StartupContext;
	return true;
}

void FSceneManager::Release()
{
	ActiveWorldContext = nullptr;

	for (std::unique_ptr<FEditorWorldContext>& PreviewContext : PreviewWorldContexts)
	{
		if (PreviewContext)
		{
			DestroyWorldContext(*PreviewContext);
		}
	}
	PreviewWorldContexts.clear();

	DestroyWorldContext(EditorWorldContext);
	DestroyWorldContext(GameWorldContext);
	Renderer = nullptr;
}

// ===== World Context 생성/파괴 =====

bool FSceneManager::CreateWorldContext(FWorldContext& OutContext, const FString& ContextName,
	ESceneType WorldType, float AspectRatio, bool bDefaultScene)
{
	OutContext.ContextName = ContextName;
	OutContext.WorldType = WorldType;

	OutContext.World = FObjectFactory::ConstructObject<UWorld>(nullptr, ContextName);
	if (!OutContext.World)
	{
		return false;
	}

	OutContext.World->SetWorldType(WorldType);

	if (bDefaultScene)
	{
		OutContext.World->InitializeWorld(AspectRatio, Renderer ? Renderer->GetDevice() : nullptr);
	}
	else
	{
		OutContext.World->InitializeWorld(AspectRatio);
	}

	return true;
}

void FSceneManager::DestroyWorldContext(FWorldContext& Context)
{
	if (Context.World)
	{
		Context.World->CleanupWorld();
		delete Context.World;
	}
	Context.Reset();
}

void FSceneManager::DestroyWorldContext(FEditorWorldContext& Context)
{
	if (Context.World)
	{
		Context.World->CleanupWorld();
		delete Context.World;
	}
	Context.Reset();
}

// ===== 하위 호환 Scene 접근자 =====

UScene* FSceneManager::GetActiveScene() const
{
	UWorld* World = GetActiveWorld();
	return World ? World->GetScene() : nullptr;
}

UScene* FSceneManager::GetEditorScene() const
{
	return EditorWorldContext.World ? EditorWorldContext.World->GetScene() : nullptr;
}

UScene* FSceneManager::GetGameScene() const
{
	return GameWorldContext.World ? GameWorldContext.World->GetScene() : nullptr;
}

UScene* FSceneManager::GetPreviewScene(const FString& ContextName) const
{
	const FEditorWorldContext* Context = FindPreviewWorld(ContextName);
	if (Context && Context->World)
	{
		return Context->World->GetScene();
	}
	return nullptr;
}

// ===== Activate =====

bool FSceneManager::ActivatePreviewScene(const FString& ContextName)
{
	FEditorWorldContext* PreviewContext = FindPreviewWorld(ContextName);
	if (PreviewContext == nullptr)
	{
		return false;
	}

	ActiveWorldContext = PreviewContext;
	return true;
}

// ===== Selected Actor =====

FEditorWorldContext* FSceneManager::GetActiveEditorContext()
{
	if (ActiveWorldContext == &EditorWorldContext)
	{
		return &EditorWorldContext;
	}

	for (const std::unique_ptr<FEditorWorldContext>& Context : PreviewWorldContexts)
	{
		if (Context && Context.get() == ActiveWorldContext)
		{
			return Context.get();
		}
	}

	return nullptr;
}

const FEditorWorldContext* FSceneManager::GetActiveEditorContext() const
{
	if (ActiveWorldContext == &EditorWorldContext)
	{
		return &EditorWorldContext;
	}

	for (const std::unique_ptr<FEditorWorldContext>& Context : PreviewWorldContexts)
	{
		if (Context && Context.get() == ActiveWorldContext)
		{
			return Context.get();
		}
	}

	return nullptr;
}

void FSceneManager::SetSelectedActor(AActor* InActor)
{
	FEditorWorldContext* ActiveEditorContext = GetActiveEditorContext();
	if (ActiveEditorContext)
	{
		ActiveEditorContext->SelectedActor = InActor;
		return;
	}

	EditorWorldContext.SelectedActor = InActor;
}

AActor* FSceneManager::GetSelectedActor() const
{
	const FEditorWorldContext* ActiveEditorContext = GetActiveEditorContext();
	if (ActiveEditorContext)
	{
		return ActiveEditorContext->SelectedActor;
	}

	return EditorWorldContext.SelectedActor;
}

// ===== Preview =====

FEditorWorldContext* FSceneManager::FindPreviewWorld(const FString& ContextName)
{
	for (const std::unique_ptr<FEditorWorldContext>& Context : PreviewWorldContexts)
	{
		if (Context && Context->ContextName == ContextName)
		{
			return Context.get();
		}
	}
	return nullptr;
}

const FEditorWorldContext* FSceneManager::FindPreviewWorld(const FString& ContextName) const
{
	for (const std::unique_ptr<FEditorWorldContext>& Context : PreviewWorldContexts)
	{
		if (Context && Context->ContextName == ContextName)
		{
			return Context.get();
		}
	}
	return nullptr;
}

FEditorWorldContext* FSceneManager::CreatePreviewWorldContext(const FString& ContextName, int32 WindowWidth, int32 WindowHeight)
{
	if (ContextName.empty())
	{
		return nullptr;
	}

	if (FEditorWorldContext* ExistingContext = FindPreviewWorld(ContextName))
	{
		return ExistingContext;
	}

	std::unique_ptr<FEditorWorldContext> PreviewContext = std::make_unique<FEditorWorldContext>();
	const float AspectRatio = (WindowHeight > 0)
		? (static_cast<float>(WindowWidth) / static_cast<float>(WindowHeight))
		: 1.0f;

	if (!CreateWorldContext(*PreviewContext, ContextName, ESceneType::Preview, AspectRatio, false))
	{
		return nullptr;
	}

	FEditorWorldContext* CreatedContext = PreviewContext.get();
	PreviewWorldContexts.push_back(std::move(PreviewContext));
	return CreatedContext;
}

bool FSceneManager::DestroyPreviewWorld(const FString& ContextName)
{
	for (auto It = PreviewWorldContexts.begin(); It != PreviewWorldContexts.end(); ++It)
	{
		if (*It && (*It)->ContextName == ContextName)
		{
			if (ActiveWorldContext == It->get())
			{
				ActivateEditorScene();
				if (ActiveWorldContext == nullptr)
				{
					ActivateGameScene();
				}
			}

			DestroyWorldContext(*(*It));
			PreviewWorldContexts.erase(It);
			return true;
		}
	}

	return false;
}

// ===== Resize =====

void FSceneManager::OnResize(int32 Width, int32 Height)
{
	if (Width == 0 || Height == 0)
	{
		return;
	}

	const float NewAspect = static_cast<float>(Width) / static_cast<float>(Height);

	auto UpdateAspect = [NewAspect](UWorld* World)
	{
		if (World && World->GetCamera())
		{
			World->GetCamera()->SetAspectRatio(NewAspect);
		}
	};

	UpdateAspect(GameWorldContext.World);
	UpdateAspect(EditorWorldContext.World);

	for (const std::unique_ptr<FEditorWorldContext>& PreviewContext : PreviewWorldContexts)
	{
		if (PreviewContext)
		{
			UpdateAspect(PreviewContext->World);
		}
	}
}
