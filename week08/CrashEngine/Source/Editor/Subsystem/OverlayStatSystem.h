// 에디터 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class UEditorEngine;

// 스크린 공간 텍스트 — Overlay Stats 등에서 사용
struct FOverlayStatLine
{
    FString Label;
    FString Value;
    FVector2 ScreenPosition = FVector2(0.0f, 0.0f);
    bool bIsSectionHeader = false;
};

// FOverlayStatLayout는 에디터 처리에 필요한 데이터를 묶는 구조체입니다.
struct FOverlayStatLayout
{
    float StartX = 16.0f;
    float StartY = 25.0f;
    float LineHeight = 22.0f;
    float GroupSpacing = 12.0f;
    float TextScale = 1.05f;
};

// FOverlayStatSystem는 에디터 영역의 핵심 동작을 담당합니다.
class FOverlayStatSystem
{
public:
    FOverlayStatSystem() = default;

    void ShowFPS(bool bEnable = true);
    void ShowPickingTime(bool bEnable = true);
    void ShowMemory(bool bEnable = true);
    void ShowShadow(bool bEnable = true);
    void ShowLightCull(bool bEnable = true);
    void RecordPickingAttempt(double ElapsedMs);
    void HideAll();

    const FOverlayStatLayout& GetLayout() const { return Layout; }
    FOverlayStatLayout& GetLayout() { return Layout; }
    FString GetDisplayTitle() const;

    void BuildLines(const UEditorEngine& Editor, TArray<FOverlayStatLine>& OutLines) const;
    TArray<FOverlayStatLine> BuildLines(const UEditorEngine& Editor) const;

private:
    void AppendLine(TArray<FOverlayStatLine>& OutLines, float Y, const FString& Label, const FString& Value) const;
    void AppendSection(TArray<FOverlayStatLine>& OutLines, float Y, const FString& Label) const;
    void ClearDisplayFlags();

    bool bShowFPS = false;
    bool bShowPickingTime = false; // WM_LBUTTONDOWN , VK_LBUTTON 입력 시점이 아닌 오브젝트 충돌 판정에 걸린 시간을 측정합니다.
    bool bShowMemory = false;
    bool bShowShadow = false;
    bool bShowLightCull = false;
    double LastPickingTimeMs = 0.0;
    double AccumulatedPickingTimeMs = 0.0;
    uint32 PickingAttemptCount = 0;
    mutable FString CachedFPSLine;
    mutable FString CachedPickingLine;
    mutable bool bHasAveragedFPS = false;
    mutable float AveragedFPS = 0.0f;
    mutable float AveragedFrameTimeMs = 0.0f;

    FOverlayStatLayout Layout;
};
