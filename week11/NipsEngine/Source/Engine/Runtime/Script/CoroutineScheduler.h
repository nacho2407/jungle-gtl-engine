#pragma once
#include "Core/Containers/Array.h"
#include "Runtime/Script/Coroutine.h"

class FCoroutineScheduler
{
public:
    void StartCoroutine(sol::function Function);
    void Tick(float DeltaTime);
    void StopAll();
    
private:
    TArray<FCoroutine> Coroutines;
    TArray<FCoroutine> PendingCoroutines;
    
    bool bUpdating = false;
    bool bStopAllRequested = false;

private:
    FCoroutine MakeCoroutine(sol::function Function);
    
    void Resume(FCoroutine & Coroutine);
    void ProcessYieldResult(FCoroutine & Coroutine, lua_State* ThreadState, int ReturnCount);
    
};
