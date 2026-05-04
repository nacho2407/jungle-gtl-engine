#include "LuaActionLibrary.h"

#include "Component/CameraComponent.h"
#include "Core/CoroutineScheduler/LuaCoroutineScheduler.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "LuaScript/LuaGameObjectProxy.h"
#include "Platform/Paths.h"
#include "Serialization/SceneSaveManager.h"

#include <filesystem>

namespace LuaActionLibrary
{
    namespace
    {
        FString PendingSceneLoadPath;

        UCameraComponent* FindCameraInWorld(UWorld* World)
        {
            if (!World)
            {
                return nullptr;
            }

            UCameraComponent* FirstCamera = nullptr;
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
                        if (Camera->IsMainCamera())
                        {
                            return Camera;
                        }

                        if (!FirstCamera)
                        {
                            FirstCamera = Camera;
                        }
                    }
                }
            }

            return FirstCamera;
        }

        UCameraComponent* CreateFallbackCamera(UWorld* World, const FPerspectiveCameraData* CameraData)
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

            return Camera;
        }

        void EnsureActiveCamera(UWorld* World, const FPerspectiveCameraData* CameraData)
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

        std::filesystem::path ResolveSceneLoadPath(const FString& ScenePath)
        {
            if (ScenePath.empty())
            {
                return {};
            }

            std::filesystem::path RawPath = FPaths::ToPath(ScenePath);
            if (!RawPath.has_extension())
            {
                RawPath += L".Scene";
            }

            if (RawPath.is_absolute())
            {
                return RawPath.lexically_normal();
            }

            if (!RawPath.has_parent_path())
            {
                return (std::filesystem::path(FPaths::SceneDir()) / RawPath).lexically_normal();
            }

            return (std::filesystem::path(FPaths::RootDir()) / RawPath).lexically_normal();
        }
    }

    void Wait(float WaitTime, sol::this_state CurrentState)
    {
        lua_State* L = CurrentState;
        lua_pushthread(L);
        sol::thread CurrentThread(L, -1);
        lua_pop(L, 1);

        FLuaCoroutineScheduler::Get().AddTimeTask(CurrentThread, WaitTime);
    }

    void MoveTo(FLuaGameObjectProxy* Proxy, float TargetX, float TargetY, sol::this_state CurrentState)
    {
        if (!Proxy || !Proxy->IsValid()) return;

        FVector NewLoc = Proxy->GetLocation();
        NewLoc.X = TargetX;
        NewLoc.Y = TargetY;
        Proxy->SetLocation(NewLoc);

        lua_State* L = CurrentState;
        lua_pushthread(L);
        sol::thread CurrentThread(L, -1);
        lua_pop(L, 1);

        auto Condition = [Proxy]() -> bool {
            if (!Proxy || !Proxy->IsValid()) return true;
            return Proxy->Velocity.LengthSquared() <= 0.01f;
            };

        FLuaCoroutineScheduler::Get().AddConditionTask(CurrentThread, Condition);
    }

    void WaitUntilMoveDone(FLuaGameObjectProxy* Proxy, sol::this_state CurrentState)
    {
        if (!Proxy || !Proxy->IsValid()) return;

        lua_State* L = CurrentState;
        lua_pushthread(L);
        sol::thread CurrentThread(L, -1);
        lua_pop(L, 1);

        auto Condition = [Proxy]() -> bool {
            if (!Proxy || !Proxy->IsValid()) return true;
            return Proxy->Velocity.LengthSquared() <= 0.01f;
            };

        FLuaCoroutineScheduler::Get().AddConditionTask(CurrentThread, Condition);
    }

    bool RequestSceneLoad(const FString& ScenePath)
    {
        const std::filesystem::path FullPath = ResolveSceneLoadPath(ScenePath);
        if (FullPath.empty())
        {
            UE_LOG(Lua, Warning, "Scene load request failed: scene path is empty.");
            return false;
        }

        if (!std::filesystem::exists(FullPath))
        {
            UE_LOG(Lua, Warning, "Scene load request failed. Scene not found: %s", FPaths::FromPath(FullPath).c_str());
            return false;
        }

        PendingSceneLoadPath = FPaths::FromPath(FullPath);
        return true;
    }

    bool ProcessPendingSceneLoad()
    {
        if (PendingSceneLoadPath.empty() || !GEngine)
        {
            return false;
        }

        const FString ScenePath = PendingSceneLoadPath;
        PendingSceneLoadPath.clear();

        FWorldContext* CurrentContext = GEngine->GetWorldContextFromHandle(GEngine->GetActiveWorldHandle());
        const FName TargetHandle = CurrentContext ? CurrentContext->ContextHandle : FName("Game");
        const FString TargetName = CurrentContext ? CurrentContext->ContextName : FString("Game");
        const EWorldType TargetWorldType = CurrentContext ? CurrentContext->WorldType : EWorldType::Game;

        FWorldContext LoadedContext;
        FPerspectiveCameraData CameraData;
        FSceneSaveManager::LoadSceneFromJSON(ScenePath, LoadedContext, CameraData);
        if (!LoadedContext.World)
        {
            UE_LOG(Lua, Warning, "Failed to load scene: %s", ScenePath.c_str());
            return false;
        }

        LoadedContext.WorldType = TargetWorldType;
        LoadedContext.ContextHandle = TargetHandle;
        LoadedContext.ContextName = TargetName;
        LoadedContext.World->SetWorldType(TargetWorldType);

        GEngine->DestroyWorldContext(TargetHandle);
        GEngine->GetWorldList().push_back(LoadedContext);
        GEngine->SetActiveWorld(TargetHandle);
        EnsureActiveCamera(LoadedContext.World, CameraData.bValid ? &CameraData : nullptr);

        if (TargetWorldType == EWorldType::Game || TargetWorldType == EWorldType::PIE)
        {
            LoadedContext.World->BeginPlay();
        }

        return true;
    }

    void Bind(sol::state& Lua)
    {
        Lua.set_function("Wait", sol::yielding(&LuaActionLibrary::Wait));
        Lua.set_function("MoveTo", sol::yielding(&LuaActionLibrary::MoveTo));
        Lua.set_function("WaitUntilMoveDone", sol::yielding(&LuaActionLibrary::WaitUntilMoveDone));
        Lua.set_function("LoadScene", &LuaActionLibrary::RequestSceneLoad);
    }
}
