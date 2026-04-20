#include "Render/Submission/Batching/LineBatch.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
float Comp(const FVector& V, int I)
{
    return (&V.X)[I];
}

FVector MakeGridPoint(int A0, int A1, int N, float V0, float V1, float VN)
{
    FVector P;
    (&P.X)[A0] = V0;
    (&P.X)[A1] = V1;
    (&P.X)[N] = VN;
    return P;
}

FVector4 AxisColor(int Axis)
{
    switch (Axis)
    {
    case 0:
        return FColor::Red().ToVector4();
    case 1:
        return FColor::Green().ToVector4();
    default:
        return FColor::Blue().ToVector4();
    }
}

float SnapToGrid(float Value, float Spacing)
{
    return std::round(Value / Spacing) * Spacing;
}

bool IsAxisLine(float Coordinate, float Spacing)
{
    return std::fabs(Coordinate) <= (Spacing * 0.25f);
}

int32 ComputeDynamicHalfCount(float Spacing, int32 BaseHalfCount, float CameraNormalDist)
{
    const float BaseExtent = Spacing * static_cast<float>(std::max(BaseHalfCount, 1));
    const float HeightDrivenExtent = (std::fabs(CameraNormalDist) * 2.0f) + (Spacing * 4.0f);
    const float RequiredExtent = std::max(BaseExtent, HeightDrivenExtent);
    return std::max(BaseHalfCount, static_cast<int32>(std::ceil(RequiredExtent / Spacing)));
}

int DominantAxis(const FVector& V)
{
    float AX = std::fabs(V.X), AY = std::fabs(V.Y), AZ = std::fabs(V.Z);
    if (AX >= AY && AX >= AZ)
        return 0;
    if (AY >= AX && AY >= AZ)
        return 1;
    return 2;
}
} // namespace

void FLineBatch::Create(ID3D11Device* InDevice)
{
    Release();
    LineBuffer.Create(InDevice, 512, 1536);
}

void FLineBatch::Release()
{
    LineBuffer.Release();
}

void FLineBatch::AddLine(const FVector& Start, const FVector& End, const FVector4& InColor)
{
    AddLine(Start, End, InColor, InColor);
}

void FLineBatch::AddLine(const FVector& Start, const FVector& End, const FVector4& StartColor, const FVector4& EndColor)
{
    const uint32 BaseVertex = static_cast<uint32>(LineBuffer.Vertices.size());
    LineBuffer.Vertices.emplace_back(Start, StartColor);
    LineBuffer.Vertices.emplace_back(End, EndColor);
    LineBuffer.Indices.push_back(BaseVertex);
    LineBuffer.Indices.push_back(BaseVertex + 1);
}

void FLineBatch::AddAABB(const FBoundingBox& Box, const FColor& InColor)
{
    const FVector4 BoxColor = InColor.ToVector4();
    const uint32 BaseVertex = static_cast<uint32>(LineBuffer.Vertices.size());

    LineBuffer.Vertices.emplace_back(FVector(Box.Min.X, Box.Min.Y, Box.Min.Z), BoxColor);
    LineBuffer.Vertices.emplace_back(FVector(Box.Max.X, Box.Min.Y, Box.Min.Z), BoxColor);
    LineBuffer.Vertices.emplace_back(FVector(Box.Max.X, Box.Max.Y, Box.Min.Z), BoxColor);
    LineBuffer.Vertices.emplace_back(FVector(Box.Min.X, Box.Max.Y, Box.Min.Z), BoxColor);
    LineBuffer.Vertices.emplace_back(FVector(Box.Min.X, Box.Min.Y, Box.Max.Z), BoxColor);
    LineBuffer.Vertices.emplace_back(FVector(Box.Max.X, Box.Min.Y, Box.Max.Z), BoxColor);
    LineBuffer.Vertices.emplace_back(FVector(Box.Max.X, Box.Max.Y, Box.Max.Z), BoxColor);
    LineBuffer.Vertices.emplace_back(FVector(Box.Min.X, Box.Max.Y, Box.Max.Z), BoxColor);

    static constexpr uint32 AABBEdgeIndices[] = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };

    for (uint32 EdgeIndex : AABBEdgeIndices)
    {
        LineBuffer.Indices.push_back(BaseVertex + EdgeIndex);
    }
}

void FLineBatch::AddWorldHelpers(const FShowFlags& ShowFlags, float GridSpacing, int32 GridHalfLineCount,
                                 const FVector& CameraPosition, const FVector& CameraForward, bool bIsOrtho)
{
    const float Spacing = GridSpacing;
    const int32 BaseHalfCount = std::max(GridHalfLineCount, 1);

    if (Spacing <= 0.0f)
        return;

    int N = 2;
    if (bIsOrtho)
    {
        N = DominantAxis(CameraForward);
    }

    const int A0 = (N == 0) ? 1 : 0;
    const int A1 = (N == 2) ? 1 : 2;
    constexpr float PlaneOffset = 0.0f;

    const float Center0 = SnapToGrid(Comp(CameraPosition, A0), Spacing);
    const float Center1 = SnapToGrid(Comp(CameraPosition, A1), Spacing);
    const float CameraNormalDist = Comp(CameraPosition, N) - PlaneOffset;
    const int32 DynamicHalfCount = ComputeDynamicHalfCount(Spacing, BaseHalfCount, CameraNormalDist);
    const float BaseGridExtent = Spacing * static_cast<float>(DynamicHalfCount);

    const float Min0 = Center0 - BaseGridExtent;
    const float Max0 = Center0 + BaseGridExtent;
    const float Min1 = Center1 - BaseGridExtent;
    const float Max1 = Center1 + BaseGridExtent;

    const int32 Min0Idx = static_cast<int32>(std::floor((Min0 - Center0) / Spacing));
    const int32 Max0Idx = static_cast<int32>(std::ceil((Max0 - Center0) / Spacing));
    const int32 Min1Idx = static_cast<int32>(std::floor((Min1 - Center1) / Spacing));
    const int32 Max1Idx = static_cast<int32>(std::ceil((Max1 - Center1) / Spacing));

    const float AxisBias = std::max(Spacing * 0.001f, 0.001f);

    const bool bShowAxis0 = (Min1 <= 0.0f) && (Max1 >= 0.0f);
    const bool bShowAxis1 = (Min0 <= 0.0f) && (Max0 >= 0.0f);

    if (ShowFlags.bGrid)
    {
        const FVector4 GridColor = FColor::Gray().ToVector4();

        for (int32 I = Min1Idx; I <= Max1Idx; ++I)
        {
            const float W1 = Center1 + (static_cast<float>(I) * Spacing);
            if (bShowAxis0 && IsAxisLine(W1, Spacing))
                continue;
            AddLine(
                MakeGridPoint(A0, A1, N, Min0, W1, PlaneOffset),
                MakeGridPoint(A0, A1, N, Max0, W1, PlaneOffset),
                GridColor);
        }

        for (int32 I = Min0Idx; I <= Max0Idx; ++I)
        {
            const float W0 = Center0 + (static_cast<float>(I) * Spacing);
            if (bShowAxis1 && IsAxisLine(W0, Spacing))
                continue;
            AddLine(
                MakeGridPoint(A0, A1, N, W0, Min1, PlaneOffset),
                MakeGridPoint(A0, A1, N, W0, Max1, PlaneOffset),
                GridColor);
        }
    }

    if (ShowFlags.bWorldAxis && bShowAxis0)
    {
        AddLine(
            MakeGridPoint(A0, A1, N, Min0, 0.0f, AxisBias),
            MakeGridPoint(A0, A1, N, Max0, 0.0f, AxisBias),
            AxisColor(A0));
    }

    if (ShowFlags.bWorldAxis && bShowAxis1)
    {
        AddLine(
            MakeGridPoint(A0, A1, N, 0.0f, Min1, AxisBias),
            MakeGridPoint(A0, A1, N, 0.0f, Max1, AxisBias),
            AxisColor(A1));
    }

    if (ShowFlags.bWorldAxis && bShowAxis0 && bShowAxis1)
    {
        const float AxisLen = std::max(Spacing * static_cast<float>(BaseHalfCount), Spacing * 10.0f);
        AddLine(
            MakeGridPoint(A0, A1, N, 0.0f, 0.0f, -AxisLen),
            MakeGridPoint(A0, A1, N, 0.0f, 0.0f, AxisLen),
            AxisColor(N));
    }
}

void FLineBatch::Clear()
{
    LineBuffer.Clear();
}

bool FLineBatch::UploadBuffers(ID3D11DeviceContext* Context)
{
    return LineBuffer.Upload(Context);
}
