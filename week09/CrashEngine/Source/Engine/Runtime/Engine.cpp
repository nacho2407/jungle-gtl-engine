// 런타임 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "Engine/Runtime/Engine.h"

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include "Profiling/Stats.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Resource/ResourceManager.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Mesh/ObjManager.h"
#include "Texture/Texture2D.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Core/TickFunction.h"
#include "Core/Sound/SoundManager.h"
#include "Core/Watcher/DirectoryWatcher.h"
#include "LuaScript/LuaRuntime.h"
#include "LuaScript/LuaScriptManager.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"
#include "Render/Execute/Context/ViewMode/ViewModeSurfaces.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"

DEFINE_CLASS(UEngine, UObject)

UEngine* GEngine = nullptr;

namespace
{
ELevelTick ToLevelTickType(EWorldType WorldType)
{
    switch (WorldType)
    {
    case EWorldType::Editor:
        return ELevelTick::LEVELTICK_ViewportsOnly;
    case EWorldType::PIE:
    case EWorldType::Game:
        return ELevelTick::LEVELTICK_All;
    default:
        return ELevelTick::LEVELTICK_TimeOnly;
    }
}
} // namespace

void UEngine::Init(FWindowsWindow* InWindow)
{
    UE_LOG(Engine, Info, "Initializing runtime engine.");
    Window = InWindow;

    FNamePool::Get();
    FObjectFactory::Get();

    Renderer.Create(Window->GetHWND());
    UE_LOG(Engine, Debug, "Renderer created.");

    ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
    FMeshBufferManager::Get().Initialize(Device);
    FResourceManager::Get().LoadFromFile(FPaths::ToUtf8(FPaths::ResourceFilePath()), Device);
    FSoundManager::Get().Init();

    FLuaRuntime::Get().Initialize();

    std::wstring ScriptsDirWide = FPaths::Combine(FPaths::ContentDir(), L"Scripts");
    FPaths::CreateDir(ScriptsDirWide); // 혹시 Scripts 폴더가 없을 경우 DirectoryWatcher 초기화와 CreateScript가 실패하는 상황을 줄이기
    FDirectoryWatcher::Get().Init(FPaths::ToUtf8(ScriptsDirWide));
    FLuaScriptManager::Get().Init();

    UE_LOG(Engine, Info, "Runtime engine initialization completed.");
}

void UEngine::Shutdown()
{
    UE_LOG(Engine, Info, "Shutting down runtime engine.");

    FLuaScriptManager::Get().Release();
    FDirectoryWatcher::Get().Release();
    FLuaRuntime::Get().Shutdown();

    FSoundManager::Get().Release();
    FResourceManager::Get().ReleaseGPUResources();
    UTexture2D::ReleaseAllGPU();
    FObjManager::ReleaseAllGPU();
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
    FDirectoryWatcher::Get().Tick();
    InputSystem::Get().Tick(Window->IsForeground());
    WorldTick(DeltaTime);
    Render(DeltaTime);
}

void UEngine::Render(float DeltaTime)
{
    SCOPE_STAT_CAT("UEngine::Render", "2_Render");

    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
    {
        static_cast<FShadowMapPass*>(Pass)->BeginShadowFrame();
    }

    RenderTargets.Reset();
    FViewport* Viewport = GameViewportClient ? GameViewportClient->GetViewport() : nullptr;
    ID3D11DeviceContext* DeviceContext = Renderer.GetFD3DDevice().GetDeviceContext();

    UWorld* World = GetWorld();
    UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;
    FScene* Scene = nullptr;
    if (Camera)
    {
        FShowFlags ShowFlags;
        EViewMode ViewMode = EViewMode::Lit_Phong;

        SceneView.SetCameraInfo(Camera);
        SceneView.SetRenderSettings(ViewMode, ShowFlags);
        if (Viewport && DeviceContext)
        {
            if (Viewport->ApplyPendingResize())
            {
                Camera->OnResize(static_cast<int32>(Viewport->GetWidth()), static_cast<int32>(Viewport->GetHeight()));
            }

            Viewport->BeginRender(DeviceContext);
            RenderTargets.SetFromViewport(Viewport);
            SceneView.SetViewportInfo(Viewport);
        }
        else
        {
            Renderer.ReleaseViewModeSurfaces();
        }

        Scene = &World->GetScene();

        Renderer.BeginCollect(SceneView, Scene->GetPrimitiveProxyCount());

        FRenderCollectContext CollectContext = {};
        CollectContext.SceneView = &SceneView;
        CollectContext.Scene = Scene;
        CollectContext.Renderer = &Renderer;
        CollectContext.ViewModePassRegistry = Renderer.GetViewModePassRegistry();
        CollectContext.ActiveViewMode = SceneView.ViewMode;

        Renderer.CollectWorld(World, CollectContext);
        Renderer.CollectDebugRender(*Scene);

    }
    else
    {
        Renderer.ReleaseViewModeSurfaces();
        Renderer.BeginCollect(SceneView);
    }

    {
        FRenderPipelineContext PipelineContext = Renderer.CreatePipelineContext(SceneView, &RenderTargets, Scene);
        if (Viewport && Renderer.GetViewModePassRegistry() &&
            Renderer.GetViewModePassRegistry()->HasConfig(SceneView.ViewMode))
        {
            PipelineContext.ViewMode.Surfaces =
                Renderer.AcquireViewModeSurfaces(Viewport, Viewport->GetWidth(), Viewport->GetHeight());
        }
        Renderer.BuildDrawCommands(PipelineContext);

        Renderer.RenderFrame(ERenderPipelineType::DefaultRootPipeline, PipelineContext);
    }

    if (FRenderPass* Pass = Renderer.GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass))
    {
        static_cast<FShadowMapPass*>(Pass)->EndShadowFrame();
    }
}

void UEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    if (Width == 0 || Height == 0)
    {
        return;
    }

    UE_LOG(Engine, Debug, "Window resized to %ux%u.", Width, Height);
    Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);
}

void UEngine::WorldTick(float DeltaTime)
{
    SCOPE_STAT_CAT("UEngine::WorldTick", "1_WorldTick");

    bool bHasPIEWorld = false;
    for (const FWorldContext& Ctx : WorldList)
    {
        if (Ctx.WorldType == EWorldType::PIE && Ctx.World)
        {
            bHasPIEWorld = true;
            break;
        }
    }

    for (FWorldContext& Ctx : WorldList)
    {
        UWorld* World = Ctx.World;
        if (!World)
            continue;

        if (bHasPIEWorld && Ctx.WorldType == EWorldType::Editor)
        {
            continue;
        }

        const ELevelTick TickType = ToLevelTickType(Ctx.WorldType);

        World->Tick(DeltaTime, TickType);
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
    if (Context.World)
    {
        Context.World->SetWorldType(Type);
    }
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
