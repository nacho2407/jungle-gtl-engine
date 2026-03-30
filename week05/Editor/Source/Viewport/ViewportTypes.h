#pragma once
#include "CoreMinimal.h"
#include "Core/ShowFlags.h"

class FViewport;

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
	OrthoFront,
	OrthoRight,
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
	float NearPlane = 0.1f;
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
	FViewport* Viewport;
	bool bActive = false;
	FViewportLocalState LocalState;
};