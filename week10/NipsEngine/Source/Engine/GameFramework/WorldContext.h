#pragma once
#include "Core/CoreMinimal.h"
#include "Object/FName.h"

class UWorld;

enum class EWorldType : uint32
{
    Editor,          // Editor mode — no BeginPlay
	PIE,             // Play In Editor
	EditorPriview,   // Editor Preview mode - BeginPlay/Tick active
	ViewerPreview,   // Object Viewer mode - BeginPlay/Tick active (to check animation)
	Game,		     // Game mode — BeginPlay/Tick active
};

struct FWorldContext
{
    EWorldType WorldType = EWorldType::Editor;
    UWorld* World = nullptr;
    FString ContextName;
    FName ContextHandle;
};

namespace EEndPlayReason
{
    enum Type
    {
        Destroyed,
        LevelTransition,
        EndPlayInEditor,
        RemovedFromWorld,
        Quit
    };
}