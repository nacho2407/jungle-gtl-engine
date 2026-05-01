#pragma once
#include <sol/coroutine.hpp>
#include <sol/thread.hpp>

enum class ECoroutineWaitType
{
    None,
    Seconds,
    Frames,
};

struct FCoroutine
{
    sol::thread Thread;
    sol::coroutine Routine;
    
    ECoroutineWaitType WaitType = ECoroutineWaitType::None;
    
    float RemainingSeconds = 0.f;
    int RemainingFrames = 0;
    
    bool bFinished = false;
};
