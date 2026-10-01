#pragma once

#include "Core/Singleton.h"
#include "LuaScript/LuaIncludes.h"
#include <vector>
#include <functional>

#include "Core/Logging/LogMacros.h"

struct FCoroutineTask
{
    sol::thread Thread;

    float WaitTime = 0.0f;
    std::function<bool()> WaitCondition = nullptr;
};

class FLuaCoroutineScheduler : public TSingleton<FLuaCoroutineScheduler>
{
    friend class TSingleton<FLuaCoroutineScheduler>;

public:
    void AddTimeTask(sol::thread InThread, float Time)
    {
        FCoroutineTask Task;
        Task.Thread = InThread;
        Task.WaitTime = Time;
        Tasks.push_back(Task);
    }

    void AddConditionTask(sol::thread InThread, std::function<bool()> Condition)
    {
        FCoroutineTask Task;
        Task.Thread = InThread;
        Task.WaitCondition = Condition;
        Tasks.push_back(Task);
    }

    void Update(float DeltaTime)
    {
        for (auto It = Tasks.begin(); It != Tasks.end();)
        {
            bool bShouldResume = false;

            if (It->WaitCondition)
            {
                bShouldResume = It->WaitCondition();
            }
            else
            {
                It->WaitTime -= DeltaTime;
                bShouldResume = (It->WaitTime <= 0.0f);
            }

            if (bShouldResume)
            {
                lua_State* ThreadState = It->Thread.thread_state();

                int ResumeResult = lua_resume(ThreadState, nullptr, 0);

                if (ResumeResult != 0 && ResumeResult != LUA_YIELD)
                {
                    const char* ErrorMsg = lua_tostring(ThreadState, -1);
                    UE_LOG(Lua, Error, "Coroutine Error: %s", ErrorMsg);
                }

                It = Tasks.erase(It);
            }
            else
            {
                ++It;
            }
        }
    }

    void Clear() { Tasks.clear(); }

private:
    FLuaCoroutineScheduler() = default;
    ~FLuaCoroutineScheduler() = default;

    std::list<FCoroutineTask> Tasks;
};