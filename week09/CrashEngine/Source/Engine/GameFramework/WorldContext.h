// 게임 프레임워크 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Core/CoreTypes.h"
#include "Object/FName.h"

class UWorld;

// EWorldType는 게임 프레임워크 처리에서 사용할 선택지를 정의합니다.
enum class EWorldType : uint32
{
    Editor, // Editor mode — no BeginPlay
    Game,   // Game mode — BeginPlay/Tick active
    PIE,    // Play In Editor (future use)
};

// FWorldContext는 실행 중 공유되는 상태와 참조를 묶어 전달합니다.
struct FWorldContext
{
    EWorldType WorldType = EWorldType::Editor;
    UWorld* World = nullptr;
    FString ContextName;
    FName ContextHandle;
};
