#include "GameReleaseEngine.h"

#include "Component/CameraComponent.h"
#include "Component/LightComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Logging/LogOutputDevice.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Materials/MaterialManager.h"
#include "Mesh/ObjManager.h"
#include "Platform/Paths.h"
#include "Runtime/WindowsWindow.h"
#include "Serialization/SceneSaveManager.h"
#include "Viewport/GameViewportClient.h"

#include <filesystem>

IMPLEMENT_CLASS(UGameReleaseEngine, UEngine)

void UGameReleaseEngine::PreloadDefaultObjAssets(ID3D11Device* Device)
{
    if (!Device)
    {
        return;
    }

    const TArray<FMeshAssetListItem>& ObjFiles = FObjManager::GetAvailableObjFiles();
    for (const FMeshAssetListItem& Item : ObjFiles)
    {
        if (Item.FullPath.rfind(FPaths::ContentRelativePath("Models/_Basic/"), 0) != 0)
        {
            continue;
        }

        FObjManager::LoadObjStaticMesh(Item.FullPath, Device, false);
    }
}

FWorldContext& UGameReleaseEngine::CreateFallbackGameWorld()
{
    FWorldContext& WorldContext = CreateWorldContext(EWorldType::Game, FName("Game"), "Game");
    WorldContext.World->InitWorld();
    WorldContext.World->SetWorldType(EWorldType::Game);
    return WorldContext;
}

FWorldContext& UGameReleaseEngine::LoadStartupWorld()
{
    const std::wstring StartupScenePath = FPaths::Combine(FPaths::SceneDir(), L"Default.Scene");
    if (!std::filesystem::exists(StartupScenePath))
    {
        UE_LOG(Engine, Warning, "Startup scene not found. Falling back to empty game world.");
        return CreateFallbackGameWorld();
    }

    FWorldContext LoadedContext;
    FPerspectiveCameraData CameraData;
    FSceneSaveManager::LoadSceneFromJSON(FPaths::ToUtf8(StartupScenePath), LoadedContext, CameraData);
    if (!LoadedContext.World)
    {
        UE_LOG(Engine, Warning, "Failed to load startup scene. Falling back to empty game world.");
        return CreateFallbackGameWorld();
    }

    LoadedContext.WorldType = EWorldType::Game;
    LoadedContext.ContextHandle = FName("Game");
    LoadedContext.ContextName = "Game";
    LoadedContext.World->SetWorldType(EWorldType::Game);

    WorldList.push_back(LoadedContext);
    FWorldContext& WorldContext = WorldList.back();
    RefreshLoadedWorldForGame(WorldContext.World);
    EnsureActiveCamera(WorldContext.World, CameraData.bValid ? &CameraData : nullptr);
    return WorldContext;
}

UCameraComponent* UGameReleaseEngine::FindCameraInWorld(UWorld* World) const
{
    if (!World)
    {
        return nullptr;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor)
        {
            continue;
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
            {
                return Camera;
            }
        }
    }

    return nullptr;
}

UCameraComponent* UGameReleaseEngine::CreateFallbackCamera(UWorld* World, const FPerspectiveCameraData* CameraData)
{
    if (!World)
    {
        return nullptr;
    }

    AActor* CameraActor = World->SpawnActor<AActor>();
    if (!CameraActor)
    {
        return nullptr;
    }

    UCameraComponent* Camera = UObjectManager::Get().CreateObject<UCameraComponent>(CameraActor);
    if (!Camera)
    {
        return nullptr;
    }
    CameraActor->RegisterComponent(Camera);
    CameraActor->SetRootComponent(Camera);

    if (CameraData)
    {
        FCameraState CameraState = Camera->GetCameraState();
        CameraState.FOV = CameraData->FOV;
        CameraState.NearZ = CameraData->NearClip;
        CameraState.FarZ = CameraData->FarClip;
        Camera->SetCameraState(CameraState);
        CameraActor->SetActorLocation(CameraData->Location);
        CameraActor->SetActorRotation(CameraData->Rotation);
    }
    else
    {
        CameraActor->SetActorLocation(FVector(-500.0f, -500.0f, 300.0f));
        Camera->LookAt(FVector(0.0f, 0.0f, 0.0f));
    }

    if (GameViewport.GetWidth() > 0 && GameViewport.GetHeight() > 0)
    {
        Camera->OnResize(static_cast<int32>(GameViewport.GetWidth()), static_cast<int32>(GameViewport.GetHeight()));
    }

    return Camera;
}

void UGameReleaseEngine::EnsureActiveCamera(UWorld* World, const FPerspectiveCameraData* CameraData)
{
    if (!World)
    {
        return;
    }

    UCameraComponent* Camera = FindCameraInWorld(World);
    if (!Camera)
    {
        Camera = CreateFallbackCamera(World, CameraData);
    }

    if (Camera)
    {
        World->SetActiveCamera(Camera);
    }
}

void UGameReleaseEngine::RefreshLoadedWorldForGame(UWorld* World)
{
    if (!World)
    {
        return;
    }

    for (AActor* Actor : World->GetActors())
    {
        if (!Actor)
        {
            continue;
        }

        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
            {
                Primitive->MarkRenderStateDirty();
            }
            else if (ULightComponent* Light = Cast<ULightComponent>(Component))
            {
                Light->DestroyRenderState();
                Light->CreateRenderState();
            }
        }
    }
}

void UGameReleaseEngine::DestroyWorlds()
{
    for (FWorldContext& Context : WorldList)
    {
        if (Context.World)
        {
            Context.World->EndPlay();
            UObjectManager::Get().DestroyObject(Context.World);
            Context.World = nullptr;
        }
    }

    WorldList.clear();
    ActiveWorldHandle = FName::None;
}

void UGameReleaseEngine::Init(FWindowsWindow* InWindow)
{
    UEngine::Init(InWindow);
    
    FObjManager::ScanMeshAssets();
    FObjManager::ScanObjSourceFiles();
    FMaterialManager::Get().ScanMaterialAssets();
    //UE_LOG(EditorEngine, ELogLevel::Debug, "Asset registries scanned.");
    PreloadDefaultObjAssets(Renderer.GetFD3DDevice().GetDevice());
    FObjManager::ScanMeshAssets();
    FMaterialManager::Get().ScanMaterialAssets();

    UGameViewportClient* NewViewportClient = UObjectManager::Get().CreateObject<UGameViewportClient>();
    SetGameViewportClient(NewViewportClient);

    const uint32 ViewportWidth = static_cast<uint32>(InWindow->GetWidth() > 0.0f ? InWindow->GetWidth() : 1920.0f);
    const uint32 ViewportHeight = static_cast<uint32>(InWindow->GetHeight() > 0.0f ? InWindow->GetHeight() : 1080.0f);
    GameViewport.Initialize(Renderer.GetFD3DDevice().GetDevice(), ViewportWidth, ViewportHeight);
    GameViewport.SetClient(NewViewportClient);
    NewViewportClient->SetViewport(&GameViewport);

    FWorldContext& WorldContext = LoadStartupWorld();
    SetActiveWorld(WorldContext.ContextHandle);
    EnsureActiveCamera(WorldContext.World);
}

void UGameReleaseEngine::Shutdown()
{
    UE_LOG(Engine, Info, "Shutting down game release engine.");
    //CloseScene();
    if (GameViewportClient)
    {
        GameViewportClient->SetViewport(nullptr);
        UObjectManager::Get().DestroyObject(GameViewportClient);
        GameViewportClient = nullptr;
    }
    GameViewport.Release();
    DestroyWorlds();
    UEngine::Shutdown();
}

void UGameReleaseEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);
    GameViewport.RequestResize(Width, Height);
}
