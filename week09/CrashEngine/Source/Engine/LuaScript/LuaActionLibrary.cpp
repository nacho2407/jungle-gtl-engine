#include "LuaActionLibrary.h"

#include "Core/CoroutineScheduler/LuaCoroutineScheduler.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Runtime/Engine.h"
#include "LuaScript/LuaGameObjectProxy.h"
#include "Platform/Paths.h"
#include "Serialization/SceneSaveManager.h"

#include <filesystem>

namespace LuaActionLibrary
{
    namespace
    {
        FString PendingSceneLoadPath;

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
