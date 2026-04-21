#include "Render/Scene/DebugDraw/SceneDebugData.h"

void FSceneDebugData::ClearTransientData()
{
    OverlayTexts.clear();
    DebugLines.clear();
    Grid = {};
}

void FSceneDebugData::AddOverlayText(FString Text, const FVector2& Position, float Scale)
{
    OverlayTexts.push_back({ std::move(Text), Position, Scale });
}

void FSceneDebugData::AddDebugAABB(const FVector& Min, const FVector& Max, const FColor& Color)
{
    // 8개 꼭짓점 생성
    FVector V[8] = {
        FVector(Min.X, Min.Y, Min.Z), // 0: Min
        FVector(Max.X, Min.Y, Min.Z), // 1
        FVector(Max.X, Max.Y, Min.Z), // 2
        FVector(Min.X, Max.Y, Min.Z), // 3
        FVector(Min.X, Min.Y, Max.Z), // 4
        FVector(Max.X, Min.Y, Max.Z), // 5
        FVector(Max.X, Max.Y, Max.Z), // 6: Max
        FVector(Min.X, Max.Y, Max.Z), // 7
    };

    // 12개 에지 추가 (DebugLines 리스트에 직접 삽입)
    AddDebugLine(V[0], V[1], Color);
    AddDebugLine(V[1], V[2], Color);
    AddDebugLine(V[2], V[3], Color);
    AddDebugLine(V[3], V[0], Color);

    AddDebugLine(V[4], V[5], Color);
    AddDebugLine(V[5], V[6], Color);
    AddDebugLine(V[6], V[7], Color);
    AddDebugLine(V[7], V[4], Color);

    AddDebugLine(V[0], V[4], Color);
    AddDebugLine(V[1], V[5], Color);
    AddDebugLine(V[2], V[6], Color);
    AddDebugLine(V[3], V[7], Color);
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
