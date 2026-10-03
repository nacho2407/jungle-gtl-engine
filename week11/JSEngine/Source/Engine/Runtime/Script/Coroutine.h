#pragma once
#include <sol/function.hpp>
#include <sol/thread.hpp>

enum class ECoroutineWaitType
{
    None,
    Seconds,
    UnscaledSeconds,
    Frames,
};

struct FCoroutine
{
    sol::thread Thread;
    lua_State* ThreadState = nullptr;
    
    ECoroutineWaitType WaitType = ECoroutineWaitType::None;
    
    float RemainingSeconds = 0.f;
    int RemainingFrames = 0;
    
    bool bFinished = false;
};
