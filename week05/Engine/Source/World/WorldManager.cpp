#include "WorldManager.h"

#include "Scene/Scene.h"
#include "World/World.h"
#include "Object/ObjectFactory.h"
#include "Renderer/Renderer.h"
#include "Component/CameraComponent.h"
#include "Camera/Camera.h"

FWorldManager::~FWorldManager()
{
	Release();
}

bool FWorldManager::Initialize(float AspectRatio, ESceneType StartupSceneType, CRenderer* InRenderer)
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

void FWorldManager::Release()
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

bool FWorldManager::CreateWorldContext(FWorldContext& OutContext, const FString& ContextName,
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

void FWorldManager::DestroyWorldContext(FWorldContext& Context)
{
	if (Context.World)
	{
		Context.World->CleanupWorld();
		delete Context.World;
	}
	Context.Reset();
}

void FWorldManager::DestroyWorldContext(FEditorWorldContext& Context)
{
	if (Context.World)
	{
		Context.World->CleanupWorld();
		delete Context.World;
	}
	Context.Reset();
}

UScene* FWorldManager::GetActiveScene() const
{
	UWorld* World = GetActiveWorld();
	return World ? World->GetScene() : nullptr;
}

UScene* FWorldManager::GetEditorScene() const
{
	return EditorWorldContext.World ? EditorWorldContext.World->GetScene() : nullptr;
}

UScene* FWorldManager::GetGameScene() const
{
	return GameWorldContext.World ? GameWorldContext.World->GetScene() : nullptr;
}

UScene* FWorldManager::GetPreviewScene(const FString& ContextName) const
{
	const FEditorWorldContext* Context = FindPreviewWorld(ContextName);
	if (Context && Context->World)
	{
		return Context->World->GetScene();
	}
	return nullptr;
}

bool FWorldManager::ActivatePreviewScene(const FString& ContextName)
{
	FEditorWorldContext* PreviewContext = FindPreviewWorld(ContextName);
	if (PreviewContext == nullptr)
	{
		return false;
	}

	ActiveWorldContext = PreviewContext;
	return true;
}

FEditorWorldContext* FWorldManager::FindPreviewWorld(const FString& ContextName)
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

const FEditorWorldContext* FWorldManager::FindPreviewWorld(const FString& ContextName) const
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

FEditorWorldContext* FWorldManager::CreatePreviewWorldContext(const FString& ContextName, int32 WindowWidth, int32 WindowHeight)
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

bool FWorldManager::DestroyPreviewWorld(const FString& ContextName)
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

void FWorldManager::OnResize(int32 Width, int32 Height)
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
