#pragma once
#include "Core/CoreMinimal.h"
#include "Object/FName.h"

class UWorld;

enum class EWorldType : uint32
{
    Editor,    // Editor mode — no BeginPlay
    Game,      // Game mode — BeginPlay/Tick active
	Preview,   // Object Viewer mode - BeginPlay/Tick active (to check animation)
    PIE,       // Play In Editor (future use)
};

struct FWorldContext
{
    EWorldType WorldType = EWorldType::Editor;
    UWorld* World = nullptr;
    FString ContextName;
    FName ContextHandle;
};
