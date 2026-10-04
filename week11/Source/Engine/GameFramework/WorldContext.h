#pragma once
#include "Core/CoreMinimal.h"
#include "Object/FName.h"

class UWorld;
class FSelectionManager;

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
    UWorld*    World     = nullptr;
    FSelectionManager* SelectionManager = nullptr;
    FString    ContextName;
    FName      ContextHandle;
    bool       bPaused  = false;  // PIE 일시정지 중이면 WorldTick에서 제외됩니다.

	~FWorldContext() = default;
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
