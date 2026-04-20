#pragma once

#include "Core/CoreTypes.h"
#include "Render/Scene/DebugDraw/DebugDrawQueue.h"

struct FSceneOverlayText
{
    FString Text;
    FVector2 Position;
    float Scale = 1.0f;
};

struct FSceneDebugAABB
{
    FVector Min;
    FVector Max;
    FColor Color;
};

struct FSceneDebugLine
{
    FVector Start;
    FVector End;
    FColor Color;
};

struct FSceneGridParams
{
    float Spacing = 0.0f;
    int32 HalfLineCount = 0;
    bool bEnabled = false;
};

class FSceneDebugData
{
public:
    void ClearTransientData();

    void AddOverlayText(FString Text, const FVector2& Position, float Scale);
    void AddDebugAABB(const FVector& Min, const FVector& Max, const FColor& Color);
    void AddDebugLine(const FVector& Start, const FVector& End, const FColor& Color);
    void SetGrid(float Spacing, int32 HalfLineCount);

    const TArray<FSceneOverlayText>& GetOverlayTexts() const { return OverlayTexts; }
    const TArray<FSceneDebugAABB>& GetDebugAABBs() const { return DebugAABBs; }
    const TArray<FSceneDebugLine>& GetDebugLines() const { return DebugLines; }
    bool HasGrid() const { return Grid.bEnabled; }
    float GetGridSpacing() const { return Grid.Spacing; }
    int32 GetGridHalfLineCount() const { return Grid.HalfLineCount; }

    FDebugDrawQueue& GetDebugDrawQueue() { return DebugDrawQueue; }
    const FDebugDrawQueue& GetDebugDrawQueue() const { return DebugDrawQueue; }

private:
    TArray<FSceneOverlayText> OverlayTexts;
    TArray<FSceneDebugAABB> DebugAABBs;
    TArray<FSceneDebugLine> DebugLines;
    FSceneGridParams Grid;
    FDebugDrawQueue DebugDrawQueue;
};
