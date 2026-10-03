#pragma once

#include "Core/CoreTypes.h"

enum class ERuntimeUIRenderMode : uint8
{
    GameClient,
    PIE
};

struct FRuntimeUIVector2
{
    float X = 0.0f;
    float Y = 0.0f;

    FRuntimeUIVector2() = default;
    FRuntimeUIVector2(float InX, float InY)
        : X(InX)
        , Y(InY)
    {
    }

    FRuntimeUIVector2 operator+(const FRuntimeUIVector2& Other) const
    {
        return FRuntimeUIVector2(X + Other.X, Y + Other.Y);
    }

    FRuntimeUIVector2 operator-(const FRuntimeUIVector2& Other) const
    {
        return FRuntimeUIVector2(X - Other.X, Y - Other.Y);
    }

    FRuntimeUIVector2 operator*(float Scalar) const
    {
        return FRuntimeUIVector2(X * Scalar, Y * Scalar);
    }
};

struct FRuntimeUIRenderContext
{
    ERuntimeUIRenderMode RenderMode = ERuntimeUIRenderMode::GameClient;
    FRuntimeUIVector2 ViewportMin;
    FRuntimeUIVector2 ViewportSize;
    FRuntimeUIVector2 LayoutSize;
    float DeltaTime = 0.0f;
    bool bPreviewDocumentOnly = false;
};
