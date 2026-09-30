#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"

class FPrimitiveProxy;

// FSceneOverlayText는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FSceneOverlayText
{
    FString  Text;
    FVector2 Position;
    float    Scale = 1.0f;
};

// FSceneDebugAABB는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FSceneDebugAABB
{
    FVector Min;
    FVector Max;
    FColor  Color;
};

// FSceneDebugLine는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FSceneDebugLine
{
    FVector Start;
    FVector End;
    FColor  Color;
};

// FSceneGridParams는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
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

    bool                            HasGrid() const { return Guides.Grid.bEnabled; }
    float                           GetGridSpacing() const { return Guides.Grid.Spacing; }
    int32                           GetGridHalfLineCount() const { return Guides.Grid.HalfLineCount; }
    const TArray<FSceneDebugAABB>&  GetDebugAABBs() const { return Debug.AABBs; }
    const TArray<FSceneDebugLine>&  GetDebugLines() const { return Debug.Lines; }
    const TArray<FPrimitiveProxy*>& GetEditorHelperBillboards() const { return EditorHelpers.Billboards; }
    const TArray<FPrimitiveProxy*>& GetEditorHelperTexts() const { return EditorHelpers.Texts; }

    // FDebugPayload는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
    struct FDebugPayload
    {
        TArray<FSceneDebugAABB> AABBs;
        TArray<FSceneDebugLine> Lines;
    } Debug;

    // FGuidePayload는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
    struct FGuidePayload
    {
        FSceneGridParams Grid;
    } Guides;

    // FEditorHelperPayload는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
    struct FEditorHelperPayload
    {
        TArray<FPrimitiveProxy*> Billboards;
        TArray<FPrimitiveProxy*> Texts;
    } EditorHelpers;
};
