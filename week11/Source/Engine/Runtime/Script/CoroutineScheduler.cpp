#include "CoroutineScheduler.h"

#include "Core/Containers/String.h"
#include "Core/Logging/Log.h"
#include "Math/Utils.h"

#include <algorithm>

namespace
{
    int ResumeLuaThread(lua_State* ThreadState, int ArgCount, int& OutResultCount)
    {
#if SOL_LUA_VERSION_I_ >= 504
        return lua_resume(ThreadState, nullptr, ArgCount, &OutResultCount);
#else
        const int Status = lua_resume(ThreadState, nullptr, ArgCount);
        OutResultCount = std::max(lua_gettop(ThreadState), 0);
        return Status;
#endif
    }
}

void FCoroutineScheduler::StartCoroutine(sol::function Function)
{
    FCoroutine NewCoroutine = MakeCoroutine(Function);
    if (NewCoroutine.bFinished)
    {
        return;
    }
    
    if (bUpdating)
    {
        PendingCoroutines.push_back(std::move(NewCoroutine));
    }
    else
    {
        Coroutines.push_back(std::move(NewCoroutine));
    }
}

void FCoroutineScheduler::Tick(float DeltaTime, float UnscaledDeltaTime)
{
    bUpdating = true;
    
    for (FCoroutine& Coroutine : Coroutines)
    {
        if (Coroutine.bFinished)
            continue;
        
        if (Coroutine.WaitType == ECoroutineWaitType::Seconds)
        {
            Coroutine.RemainingSeconds -= DeltaTime;
            if (Coroutine.RemainingSeconds > MathUtil::Epsilon)
            {
                continue;
            }

            Coroutine.WaitType = ECoroutineWaitType::None;
        }

        if (Coroutine.WaitType == ECoroutineWaitType::UnscaledSeconds)
        {
            Coroutine.RemainingSeconds -= UnscaledDeltaTime;
            if (Coroutine.RemainingSeconds > MathUtil::Epsilon)
            {
                continue;
            }

            Coroutine.WaitType = ECoroutineWaitType::None;
        }
        
        if (Coroutine.WaitType == ECoroutineWaitType::Frames)
        {
            Coroutine.RemainingFrames--;
            
            if (Coroutine.RemainingFrames > 0)
            {
                continue;
            }
            
            Coroutine.WaitType = ECoroutineWaitType::None;
        }
        
        Resume(Coroutine);
    }
    
    bUpdating = false;
    
    if (bStopAllRequested)
    {
        Coroutines.clear();
        PendingCoroutines.clear();
        bStopAllRequested = false;
        return;
    }
    
    std::erase_if(Coroutines, [] (const FCoroutine& Coroutine)
    {
       return Coroutine.bFinished; 
    });
    
    for (FCoroutine& Pending : PendingCoroutines)
    {
        Coroutines.push_back(std::move(Pending));
    }
    
    PendingCoroutines.clear();
}

void FCoroutineScheduler::StopAll()
{
    if (bUpdating)
    {
        bStopAllRequested = true;
        return;
    }

    Coroutines.clear();
    PendingCoroutines.clear();
}

//  코루틴 생성
FCoroutine FCoroutineScheduler::MakeCoroutine(sol::function Function)
{
    sol::state_view Lua(Function.lua_state());
    sol::thread Thread = sol::thread::create(Lua.lua_state());
    lua_State* ThreadState = Thread.thread_state();
    
    FCoroutine NewCoroutine = {};
    NewCoroutine.Thread = Thread;   //  Lua 실행 상태 들고 있음
    NewCoroutine.ThreadState = ThreadState;
    NewCoroutine.WaitType = ECoroutineWaitType::None;
    NewCoroutine.RemainingSeconds = 0.f;
    NewCoroutine.RemainingFrames = 0;
    NewCoroutine.bFinished = false;

    if (ThreadState == nullptr || !lua_checkstack(ThreadState, 8))
    {
        UE_LOG_ERROR("[Coroutine Error] Failed to allocate Lua coroutine stack.");
        NewCoroutine.bFinished = true;
        return NewCoroutine;
    }

    Function.push(ThreadState);
    
    return NewCoroutine;
}

void FCoroutineScheduler::Resume(FCoroutine& Coroutine)
{
    lua_State* ThreadState = Coroutine.ThreadState;
    if (ThreadState == nullptr)
    {
        Coroutine.bFinished = true;
        return;
    }

    if (!lua_checkstack(ThreadState, 8))
    {
        UE_LOG_ERROR("[Coroutine Error] Lua coroutine stack overflow before resume.");
        lua_settop(ThreadState, 0);
        Coroutine.bFinished = true;
        return;
    }

    int ResultCount = 0;
    const int Status = ResumeLuaThread(ThreadState, 0, ResultCount);

    if (Status != 0 && Status != LUA_YIELD)
    {
        const char* ErrorMessage = lua_tostring(ThreadState, -1);
        UE_LOG_ERROR("[Coroutine Error] %s", ErrorMessage ? ErrorMessage : "unknown coroutine error");
        lua_settop(ThreadState, 0);
        Coroutine.bFinished = true;
        return;
    }

    if (Status == 0)
    {
        lua_settop(ThreadState, 0);
        Coroutine.bFinished = true;
        return;
    }

    ProcessYieldResult(Coroutine, ThreadState, ResultCount);
    lua_settop(ThreadState, 0);
}

//  Lua 쪽의 wait 자체를 C++로 바꿔주는 함수
void FCoroutineScheduler::ProcessYieldResult(FCoroutine& Coroutine, lua_State* ThreadState, int ReturnCount)
{
    if (ThreadState == nullptr || ReturnCount <= 0)
    {
        Coroutine.WaitType = ECoroutineWaitType::None;
        return;
    }
    
    const int FirstResultIndex = lua_gettop(ThreadState) - ReturnCount + 1;
    if (!lua_istable(ThreadState, FirstResultIndex))
    {
        Coroutine.WaitType = ECoroutineWaitType::None;
        return;
    }
    
    lua_getfield(ThreadState, FirstResultIndex, "type");
    const char* TypeString = lua_tostring(ThreadState, -1);
    const FString Type = TypeString ? TypeString : "";
    lua_pop(ThreadState, 1);
    
    if (Type == "seconds")
    {
        lua_getfield(ThreadState, FirstResultIndex, "value");
        const float seconds = static_cast<float>(lua_tonumber(ThreadState, -1));
        lua_pop(ThreadState, 1);
        
        Coroutine.WaitType = ECoroutineWaitType::Seconds;
        Coroutine.RemainingSeconds = seconds;
    }
    else if (Type == "unscaled_seconds")
    {
        lua_getfield(ThreadState, FirstResultIndex, "value");
        const float seconds = static_cast<float>(lua_tonumber(ThreadState, -1));
        lua_pop(ThreadState, 1);

        Coroutine.WaitType = ECoroutineWaitType::UnscaledSeconds;
        Coroutine.RemainingSeconds = seconds;
    }
    else if (Type == "frames")
    {
        lua_getfield(ThreadState, FirstResultIndex, "value");
        const int frames = static_cast<int>(lua_tointeger(ThreadState, -1));
        lua_pop(ThreadState, 1);
        
        Coroutine.WaitType = ECoroutineWaitType::Frames;
        Coroutine.RemainingFrames = frames;
    }
    else
    {
        Coroutine.WaitType = ECoroutineWaitType::None;
    }
}
