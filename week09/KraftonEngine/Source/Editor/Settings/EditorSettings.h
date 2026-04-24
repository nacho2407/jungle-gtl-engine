// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Platform/Paths.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"

// FEditorSettings는 에디터 영역의 핵심 동작을 담당합니다.
class FEditorSettings : public TSingleton<FEditorSettings>
{
    friend class TSingleton<FEditorSettings>;

public:
    // Viewport
    float CameraSpeed = 10.f;
    float CameraRotationSpeed = 60.f;
    float CameraZoomSpeed = 300.f;
    FVector InitViewPos = FVector(10, 0, 5);
    FVector InitLookAt = FVector(0, 0, 0);

    // Viewport Layout
    int32 LayoutType = 0; // EViewportLayout
    FViewportRenderOptions SlotOptions[4];
    float SplitterRatios[3] = { 0.5f, 0.5f, 0.5f };
    int32 SplitterCount = 0;

    FVector PerspCamLocation = FVector(10, 0, 5);
    FRotator PerspCamRotation;
    float PerspCamFOV = 60.0f;
    float PerspCamNearClip = 0.1f;
    float PerspCamFarClip = 1000.0f;

    // File paths
    FString DefaultSavePath = FPaths::ToUtf8(FPaths::SceneDir());

    // FUIVisibility는 에디터 처리에 필요한 데이터를 묶는 구조체입니다.
    struct FUIVisibility
    {
        bool bConsole = true;
        bool bControl = true;
        bool bProperty = true;
        bool bScene = true;
        bool bStat = false;
    } UI;

    void SaveToFile(const FString& Path) const;
    void LoadFromFile(const FString& Path);

    static FString GetDefaultSettingsPath() { return FPaths::ToUtf8(FPaths::SettingsFilePath()); }
};
