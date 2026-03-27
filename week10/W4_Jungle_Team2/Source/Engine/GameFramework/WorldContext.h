#pragma once
#include "Core/CoreTypes.h"
#include "Object/FName.h"

class UWorld;

enum class EWorldType : uint32
{
    Editor,    // Editor mode — no BeginPlay
    Game,      // Game mode — BeginPlay/Tick active
    PIE,       // Play In Editor (future use)
};

struct FWorldContext
{
    EWorldType WorldType = EWorldType::Editor;
    UWorld* World = nullptr;
    FString ContextName;
    FName ContextHandle;
};
