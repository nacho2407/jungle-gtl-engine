#include "Render/Scene/DebugDraw/SceneDebugData.h"

void FSceneDebugData::ClearTransientData()
{
    OverlayTexts.clear();
    DebugAABBs.clear();
    DebugLines.clear();
    Grid = {};
}

void FSceneDebugData::AddOverlayText(FString Text, const FVector2& Position, float Scale)
{
    OverlayTexts.push_back({ std::move(Text), Position, Scale });
}

void FSceneDebugData::AddDebugAABB(const FVector& Min, const FVector& Max, const FColor& Color)
{
    DebugAABBs.push_back({ Min, Max, Color });
}

void FSceneDebugData::AddDebugLine(const FVector& Start, const FVector& End, const FColor& Color)
{
    DebugLines.push_back({ Start, End, Color });
}

void FSceneDebugData::SetGrid(float Spacing, int32 HalfLineCount)
{
    Grid.Spacing = Spacing;
    Grid.HalfLineCount = HalfLineCount;
    Grid.bEnabled = (Spacing > 0.0f && HalfLineCount > 0);
}
