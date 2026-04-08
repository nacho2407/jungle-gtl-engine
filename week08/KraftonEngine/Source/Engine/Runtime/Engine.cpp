#include "Engine/Runtime/Engine.h"

#include "Platform/Paths.h"
#include "Profiling/Stats.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Resource/ResourceManager.h"
#include "Render/Pipeline/DefaultRenderPipeline.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Mesh/ObjManager.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"

DEFINE_CLASS(UEngine, UObject)

UEngine* GEngine = nullptr;

void UEngine::Init(FWindowsWindow* InWindow)
{
	Window = InWindow;

	// 싱글턴 초기화 순서 보장
	FNamePool::Get();
	FObjectFactory::Get();

	InputSystem::Get().SetOwnerWindow(Window->GetHWND());
	Renderer.Create(Window->GetHWND());

	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
	FMeshBufferManager::Get().Initialize(Device);
	FResourceManager::Get().LoadFromFile(FPaths::ToUtf8(FPaths::ResourceFilePath()), Device);

	SetRenderPipeline(std::make_unique<FDefaultRenderPipeline>(this, Renderer));
}

void UEngine::Shutdown()
{
	RenderPipeline.reset();
	FResourceManager::Get().ReleaseGPUResources();
	FMeshBufferManager::Get().Release();
	Renderer.Release();
}

void UEngine::BeginPlay()
{
	FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
	if (Context && Context->World)
	{
		if (Context->WorldType == EWorldType::Game || Context->WorldType == EWorldType::PIE)
		{
			Context->World->BeginPlay();
		}
	}
}

void UEngine::Tick(float DeltaTime)
{
	InputSystem::Get().Tick();
	WorldTick(DeltaTime);
	Render(DeltaTime);
}

void UEngine::Render(float DeltaTime)
{
	if (RenderPipeline)
	{
		SCOPE_STAT_CAT("UEngine::Render", "2_Render");
		RenderPipeline->Execute(DeltaTime, Renderer);
	}
}

void UEngine::SetRenderPipeline(std::unique_ptr<IRenderPipeline> InPipeline)
{
	RenderPipeline = std::move(InPipeline);
}

void UEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	if (Width == 0 || Height == 0)
	{
		return;
	}

	Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);
}

void UEngine::WorldTick(float DeltaTime)
{
	SCOPE_STAT_CAT("UEngine::WorldTick", "1_WorldTick");

	// PIE 활성 시 Editor 월드는 sleep (UE 동작과 동일).
	// culling/octree/visibility 갱신을 건너뛰어 50k+ 환경에서 비용 2배를 방지.
	bool bHasPIEWorld = false;
	for (const FWorldContext& Ctx : WorldList)
	{
		if (Ctx.WorldType == EWorldType::PIE && Ctx.World)
		{
			bHasPIEWorld = true;
			break;
		}
	}

	// 월드 타입별 Tick 라우팅 (UE 패턴 참고):
	// - Editor: bNeedsTick 인 액터만 Tick (현재 bTickInEditor 역할)
	// - PIE:    모든 액터 Tick
	// - 기타:   건너뜀
	for (FWorldContext& Ctx : WorldList)
	{
		UWorld* World = Ctx.World;
		if (!World) continue;

		// PIE 활성 시 Editor 월드는 완전히 skip
		if (bHasPIEWorld && Ctx.WorldType == EWorldType::Editor)
		{
			continue;
		}

		// 월드 단위 업데이트 (FlushPrimitive / VisibleProxies / DebugDraw)
		World->Tick(DeltaTime);

		if (Ctx.WorldType == EWorldType::Editor)
		{
			for (AActor* Actor : World->GetActors())
			{
				if (Actor && Actor->bTickInEditor)
				{
					Actor->Tick(DeltaTime);
				}
			}
		}
		else if (Ctx.WorldType == EWorldType::PIE)
		{
			for (AActor* Actor : World->GetActors())
			{
				// 추후 Component 단위 Tick으로 세분화할 때 bNeedsTick이 Actor 자체가 아니라 Component 단위로 옮겨질 수 있다.
				if (Actor && Actor->bNeedsTick)
				{
					Actor->Tick(DeltaTime);
				}
			}
		}
		else if (Ctx.WorldType == EWorldType::Game)
		{
			for (AActor* Actor : World->GetActors())
			{
				if (Actor && Actor->bNeedsTick)
				{
					Actor->Tick(DeltaTime);
				}
			}
		}
	}
}

UWorld* UEngine::GetWorld() const
{
	const FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
	return Context ? Context->World : nullptr;
}

FWorldContext& UEngine::CreateWorldContext(EWorldType Type, const FName& Handle, const FString& Name)
{
	FWorldContext Context;
	Context.WorldType = Type;
	Context.ContextHandle = Handle;
	Context.ContextName = Name.empty() ? Handle.ToString() : Name;
	Context.World = UObjectManager::Get().CreateObject<UWorld>();
	WorldList.push_back(Context);
	return WorldList.back();
}

void UEngine::DestroyWorldContext(const FName& Handle)
{
	for (auto it = WorldList.begin(); it != WorldList.end(); ++it)
	{
		if (it->ContextHandle == Handle)
		{
			it->World->EndPlay();
			UObjectManager::Get().DestroyObject(it->World);
			WorldList.erase(it);
			return;
		}
	}
}

FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle)
{
	for (FWorldContext& Ctx : WorldList)
	{
		if (Ctx.ContextHandle == Handle)
		{
			return &Ctx;
		}
	}
	return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle) const
{
	for (const FWorldContext& Ctx : WorldList)
	{
		if (Ctx.ContextHandle == Handle)
		{
			return &Ctx;
		}
	}
	return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* World)
{
	for (FWorldContext& Ctx : WorldList)
	{
		if (Ctx.World == World)
		{
			return &Ctx;
		}
	}
	return nullptr;
}

void UEngine::SetActiveWorld(const FName& Handle)
{
	ActiveWorldHandle = Handle;
}
