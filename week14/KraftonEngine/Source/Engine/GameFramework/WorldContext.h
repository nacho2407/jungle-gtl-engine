#pragma once
#include "Core/CoreTypes.h"
#include "Object/FName.h"

class UWorld;

enum class EWorldType : uint32
{
    Editor,    // Editor mode — no BeginPlay
    Game,      // Game mode — BeginPlay/Tick active
    PIE,       // Play In Editor
};

struct FWorldContext
{
    EWorldType WorldType = EWorldType::Editor;
    UWorld* World = nullptr;
    FString ContextName;
    FName ContextHandle;

    // Scene 파일이 명시한 GameMode 클래스 이름 — LoadSceneFromJSON 이 채운다. 비면
    // ProjectSettings 의 GameModeClassName 또는 호출자 지정 default 가 fallback.
    FString GameModeClassName;
};
