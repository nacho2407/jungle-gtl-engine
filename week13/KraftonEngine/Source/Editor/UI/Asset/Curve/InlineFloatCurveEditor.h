#pragma once

#include "Math/FloatCurve.h"

#include <imgui.h>

struct FInlineCurveEditResult
{
    bool bChanged = false;
    bool bEditStarted = false;
    bool bEditEnded = false;
};

class FInlineFloatCurveEditor
{
public:
    enum class EInteractionMode : uint8
    {
        Pan,
        Zoom,
    };

    FInlineCurveEditResult Render(
        const char* Id,
        FFloatCurve& Curve,
        const ImVec2& CanvasSize,
        EInteractionMode Mode = EInteractionMode::Pan,
        bool bShowInspector = true);

    void FitViewToCurve(const FFloatCurve& Curve);
    void FitHorizontalToCurve(const FFloatCurve& Curve);
    void FitVerticalToCurve(const FFloatCurve& Curve);
    void ResetSelection();

    bool HasSelectedKey(const FFloatCurve& Curve) const;
    void SetSelectedKeyInterpMode(FFloatCurve& Curve, ECurveInterpMode Mode);
    void SetSelectedKeyTangentMode(FFloatCurve& Curve, ECurveTangentMode Mode);
    void FlattenSelectedKeyTangents(FFloatCurve& Curve);
    void StraightenSelectedKeyTangents(FFloatCurve& Curve);

private:
    enum class ETangentHandle
    {
        None,
        Arrive,
        Leave,
    };

    int32 SelectedKeyIndex = -1;
    bool bDraggingSelectedKey = false;
    ETangentHandle DraggingTangentHandle = ETangentHandle::None;
    bool bPanningView = false;
    bool bZoomSelecting = false;
    bool bSuppressNextCanvasContextMenu = false;
    bool bWasEditingLastFrame = false;

    ImVec2 ZoomStart = ImVec2(0.0f, 0.0f);
    ImVec2 ZoomEnd = ImVec2(0.0f, 0.0f);

    float PendingContextTime = 0.0f;
    float PendingContextValue = 0.0f;

    float ViewMinTime = 0.0f;
    float ViewMaxTime = 1.0f;
    float ViewMinValue = -1.0f;
    float ViewMaxValue = 1.0f;
};
