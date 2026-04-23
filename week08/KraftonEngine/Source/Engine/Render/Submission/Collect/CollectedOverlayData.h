#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"

class FPrimitiveSceneProxy;

struct FSceneOverlayText
{
    FString  Text;
    FVector2 Position;
    float    Scale = 1.0f;
};

struct FSceneDebugAABB
{
    FVector Min;
    FVector Max;
    FColor  Color;
};

struct FSceneDebugLine
{
    FVector Start;
    FVector End;
    FColor  Color;
};

struct FSceneGridParams
{
    float Spacing       = 0.0f;
    int32 HalfLineCount = 0;
    bool  bEnabled      = false;
};

/*
    Overlay 계층에서 수집하는 screen/world 디버그 시각화 데이터를 정의합니다.
    OverlayText는 Primitive 수집 결과로 옮기고, Overlay는 Debug/Guide 데이터만 유지합니다.
*/
struct FCollectedOverlayData
{
    void ClearTransientData()
    {
        Debug.AABBs.clear();
        Debug.Lines.clear();
        Guides.Grid = {};
        EditorHelpers.Billboards.clear();
        EditorHelpers.Texts.clear();
    }

    bool                                 HasGrid() const { return Guides.Grid.bEnabled; }
    float                                GetGridSpacing() const { return Guides.Grid.Spacing; }
    int32                                GetGridHalfLineCount() const { return Guides.Grid.HalfLineCount; }
    const TArray<FSceneDebugAABB>&       GetDebugAABBs() const { return Debug.AABBs; }
    const TArray<FSceneDebugLine>&       GetDebugLines() const { return Debug.Lines; }
    const TArray<FPrimitiveSceneProxy*>& GetEditorHelperBillboards() const { return EditorHelpers.Billboards; }
    const TArray<FPrimitiveSceneProxy*>& GetEditorHelperTexts() const { return EditorHelpers.Texts; }

    struct FDebugPayload
    {
        TArray<FSceneDebugAABB> AABBs;
        TArray<FSceneDebugLine> Lines;
    } Debug;

    struct FGuidePayload
    {
        FSceneGridParams Grid;
    } Guides;

    struct FEditorHelperPayload
    {
        TArray<FPrimitiveSceneProxy*> Billboards;
        TArray<FPrimitiveSceneProxy*> Texts;
    } EditorHelpers;
};
