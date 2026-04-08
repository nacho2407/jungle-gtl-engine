#pragma once
#include "CoreMinimal.h"
#include "Core/ShowFlags.h"

class FViewport;
struct FWorldContext;

using FViewportId = uint32;
constexpr FViewportId INVALID_VIEWPORT_ID = UINT32_MAX;
constexpr int32 MAX_VIEWPORTS = 4;

struct FPoint
{
	int32 X = 0;
	int32 Y = 0;

	FPoint(int32 InX = 0, int32 InY = 0) : X(InX), Y(InY)
	{
	}
};

struct FRect
{
	int32 X = 0;
	int32 Y = 0;
	int32 Width = 0;
	int32 Height = 0;

	FRect(int32 InX = 0, int32 InY = 0, int32 InWidth = 0, int32 InHeight = 0) :
		X(InX), Y(InY), Width(InWidth), Height(InHeight)
	{
	}

	bool IsValid() const { return Width > 0 && Height > 0; }
};


inline bool ContainsPoint(const FRect& Rect, const FPoint& Point)
{
	return Rect.IsValid() && Point.X >= Rect.X && Point.X < Rect.X + Rect.Width && Point.Y >= Rect.Y && Point.Y < Rect.Y + Rect.Height;
}

inline FRect IntersectRect(const FRect& A, const FRect& B)
{
	const int32 Left = (A.X > B.X) ? A.X : B.X;
	const int32 Top = (A.Y > B.Y) ? A.Y : B.Y;
	const int32 Right = ((A.X + A.Width) < (B.X + B.Width)) ? (A.X + A.Width) : (B.X + B.Width);
	const int32 Bottom = ((A.Y + A.Height) < (B.Y + B.Height)) ? (A.Y + A.Height) : (B.Y + B.Height);
	return { Left, Top, Right - Left, Bottom - Top };
}

inline FRect UnionRect(const FRect& A, const FRect& B)
{
	if (!A.IsValid()) return B;
	if (!B.IsValid()) return A;
	const int32 Left = (A.X < B.X) ? A.X : B.X;
	const int32 Top = (A.Y < B.Y) ? A.Y : B.Y;
	const int32 Right = ((A.X + A.Width) > (B.X + B.Width)) ? (A.X + A.Width) : (B.X + B.Width);
	const int32 Bottom = ((A.Y + A.Height) > (B.Y + B.Height)) ? (A.Y + A.Height) : (B.Y + B.Height);
	return { Left, Top, Right - Left, Bottom - Top };
}

enum class ERenderMode
{
	Lighting,
	NoLighting,
	Wireframe,
};

enum class EViewportType : uint8
{
	Perspective,
	OrthoTop,
	OrthoBottom,
	OrthoLeft,
	OrthoRight,
	OrthoFront,
	OrthoBack
};

enum class EViewportLayout : uint8 {
	Single,
	SplitH, SplitV,
	ThreeLeft, ThreeRight, ThreeTop, ThreeBottom,
	FourGrid,
};

struct FViewportLocalState
{
	EViewportType ProjectionType = EViewportType::Perspective;

	FVector Position = FVector(0, 0, -5);
	FRotator Rotation = FRotator::ZeroRotator;
	float FovY = 60.f;
	float NearPlane = 0.001f;
	float FarPlane = 10000.f;

	FVector OrthoTarget = FVector::ZeroVector;
	float OrthoZoom = 1000.f;

	ERenderMode ViewMode = ERenderMode::Lighting;

	bool bShowGrid = true;
	float GridSize = 10.0f;
	float LineThickness = 1.0f;
	FShowFlags ShowFlags;

	static FViewportLocalState CreateDefault(EViewportType Type);
	FMatrix BuildViewMatrix() const;
	FMatrix BuildProjMatrix(float AspectRatio) const;
};

struct FViewportEntry
{
	FViewportId Id = INVALID_VIEWPORT_ID;
	FViewport* Viewport = nullptr;
	FWorldContext* WorldContext = nullptr;
	bool bActive = false;
	FViewportLocalState LocalState;
};
