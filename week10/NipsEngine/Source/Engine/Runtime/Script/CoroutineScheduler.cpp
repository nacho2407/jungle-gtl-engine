#include "CoroutineScheduler.h"

#include "Core/Containers/String.h"
#include "Core/Logging/Log.h"
#include "Math/Utils.h"

void FCoroutineScheduler::StartCoroutine(sol::function Function)
{
    FCoroutine NewCoroutine = MakeCoroutine(Function);
    
    if (bUpdating)
    {
        PendingCoroutines.push_back(std::move(NewCoroutine));
    }
    else
    {
        Coroutines.push_back(std::move(NewCoroutine));
    }
}

void FCoroutineScheduler::Tick(float DeltaTime)
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
    sol::state_view ThreadState = Thread.state();
    
    sol::coroutine Routine(ThreadState, Function);
    
    FCoroutine NewCoroutine = {};
    NewCoroutine.Thread = Thread;   //  Lua 실행 상태 들고 있음
    NewCoroutine.Routine = Routine; //  Routine을 통해 Resume
    NewCoroutine.WaitType = ECoroutineWaitType::None;
    NewCoroutine.RemainingSeconds = 0.f;
    NewCoroutine.RemainingFrames = 0;
    NewCoroutine.bFinished = false;
    
    return NewCoroutine;
}

void FCoroutineScheduler::Resume(FCoroutine& Coroutine)
{
    sol::protected_function_result Result = Coroutine.Routine();
    
    //  에러 확인
    if (!Result.valid())
    {
        sol::error Error = Result;
        
        UE_LOG_ERROR("[Coroutine Error] Coroutine is not valid - %s", Error.what());
        
        Coroutine.bFinished = true;
        return;
    }
    
    //  함수가 끝난 것 (코루틴 끝)
    if (Coroutine.Routine.status() == sol::call_status::ok)
    {
        Coroutine.bFinished = true;
        return;
    }
    
    ProcessYieldResult(Coroutine, Result);
}

//  Lua 쪽의 wait 자체를 C++로 바꿔주는 함수
void FCoroutineScheduler::ProcessYieldResult(FCoroutine& Coroutine, const sol::protected_function_result& Result)
{
    if (Result.return_count() <= 0)
    {
        Coroutine.WaitType = ECoroutineWaitType::None;
        return;
    }
    
    sol::object YieldObject = Result.get<sol::object>();
    
    if (!YieldObject.is<sol::table>())
    {
        Coroutine.WaitType = ECoroutineWaitType::None;
        return;
    }
    
    sol::table Table = YieldObject.as<sol::table>();
    
    FString Type = Table["type"].get_or<FString>("");
    
    if (Type == "seconds")
    {
        float seconds = Table["value"].get_or(0.0f);
        
        Coroutine.WaitType = ECoroutineWaitType::Seconds;
        Coroutine.RemainingSeconds = seconds;
    }
    else if (Type == "frames")
    {
        int seconds = Table["value"].get_or(0);
        
        Coroutine.WaitType = ECoroutineWaitType::Frames;
        Coroutine.RemainingFrames = seconds;
    }
    else
    {
        Coroutine.WaitType = ECoroutineWaitType::None;
    }
}
