#pragma once

#include "Editor/UI/EditorWidget.h"

class UCurveFloatAsset;
class UActorSequenceComponent;
struct FCurveKey;

class FEditorCurveEditorWidget : public FEditorWidget
{
public:
    void Render(float DeltaTime) override;

    void OpenCurveAsset(const FString& CurvePath);
    void OpenCurveFromActorSequence(
        UCurveFloatAsset* Curve,
        UActorSequenceComponent* SequenceComp,
        const FString& SourceLabel,
        const FString& SourcePath = "",
        int32 InitialSelectedKeyIndex = -1);
    bool IsVisible() const { return bVisible; }

private:
    void DrawToolbar();
    void DrawCurveCanvas();
    void DrawKeyList();

    void AddKey();
    void AddKeyAt(float Time, float Value);
    void RemoveSelectedKey();
    void RemoveKeyAtIndex(int32 KeyIndex);
    void StartReferencePreview();
    void StopReferencePreview();
    void TickReferencePreview(float DeltaTime);
    bool DoesSequenceReferenceCurrentCurve(UActorSequenceComponent* SequenceComp) const;
    void MarkDirty();
    bool SaveCurve();
    bool ReloadCurve();

    FString CurrentPath;
    FString SourceLabel;
    UCurveFloatAsset* CurrentCurve = nullptr;
    UActorSequenceComponent* SourceSequenceComponent = nullptr;
    int32 SelectedKeyIndex = -1;
    int32 ActiveKeyDragIndex = -1;
    int32 ActiveTangentKeyIndex = -1;
    int32 ActiveTangentHandle = -1; // 0: arrive, 1: leave
    int32 ContextKeyIndex = -1;
    float ContextTime = 0.0f;
    float ContextValue = 0.0f;
    TArray<UActorSequenceComponent*> ReferencePreviewTargets;
    float CanvasHeight = 320.0f;
    float CanvasPixelsPerUnit = 120.0f;
    float ViewMinTime = 0.0f;
    float ViewMaxTime = 1.0f;
    float ViewMinValue = -0.5f;
    float ViewMaxValue = 0.5f;
    bool bCurveViewInitialized = false;
    bool bVisible = false;
    bool bDirty = false;
    bool bReferencePreviewActive = false;
    bool bOpenedFromActorSequence = false;
};
