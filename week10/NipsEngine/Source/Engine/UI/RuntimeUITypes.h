#pragma once

#include "Core/Containers/String.h"
#include "Core/CoreTypes.h"

#include <algorithm>

enum class ERuntimeUIWidgetType : uint8
{
    Panel,
    Text,
    Button,
    Image,
    ProgressBar
};

enum class ERuntimeUICanvasScaleMode : uint8
{
    ConstantPixelSize,
    ScaleWithScreenSize
};

enum class ERuntimeUIRenderMode : uint8
{
    GameClient,
    PIE
};

enum class ERuntimeUIImageDrawMode : uint8
{
    Simple,
    NineSlice
};

enum class ERuntimeUIProgressFillDirection : uint8
{
    LeftToRight,
    RightToLeft,
    BottomToTop,
    TopToBottom
};

enum class ERuntimeUIAnimationEasing : uint8
{
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    SmoothStep
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

struct FRuntimeUIRect
{
    FRuntimeUIVector2 Min;
    FRuntimeUIVector2 Size;

    FRuntimeUIRect() = default;
    FRuntimeUIRect(const FRuntimeUIVector2& InMin, const FRuntimeUIVector2& InSize)
        : Min(InMin)
        , Size(InSize)
    {
    }

    FRuntimeUIVector2 Max() const
    {
        return FRuntimeUIVector2(Min.X + Size.X, Min.Y + Size.Y);
    }

    bool Contains(const FRuntimeUIVector2& Point) const
    {
        return Point.X >= Min.X && Point.Y >= Min.Y && Point.X <= Min.X + Size.X && Point.Y <= Min.Y + Size.Y;
    }
};

struct FRuntimeUIUVRect
{
    FRuntimeUIVector2 Min = FRuntimeUIVector2(0.0f, 0.0f);
    FRuntimeUIVector2 Max = FRuntimeUIVector2(1.0f, 1.0f);
};

struct FRuntimeUIMargin
{
    float Left = 0.0f;
    float Top = 0.0f;
    float Right = 0.0f;
    float Bottom = 0.0f;

    FRuntimeUIMargin() = default;
    FRuntimeUIMargin(float InLeft, float InTop, float InRight, float InBottom)
        : Left(InLeft)
        , Top(InTop)
        , Right(InRight)
        , Bottom(InBottom)
    {
    }
};

struct FRuntimeUIColor
{
    float R = 1.0f;
    float G = 1.0f;
    float B = 1.0f;
    float A = 1.0f;

    FRuntimeUIColor() = default;
    FRuntimeUIColor(float InR, float InG, float InB, float InA = 1.0f)
        : R(InR)
        , G(InG)
        , B(InB)
        , A(InA)
    {
    }
};

struct FRuntimeUIRenderContext
{
    ERuntimeUIRenderMode RenderMode = ERuntimeUIRenderMode::GameClient;
    FRuntimeUIVector2 ViewportMin;
    FRuntimeUIVector2 ViewportSize;
    float DeltaTime = 0.0f;
};

inline float RuntimeUIClamp01(float Value)
{
    return std::clamp(Value, 0.0f, 1.0f);
}
