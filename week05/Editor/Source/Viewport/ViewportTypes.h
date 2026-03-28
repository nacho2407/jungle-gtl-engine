#pragma once
#include "CoreMinimal.h"

class FViewport;

using FViewportId = uint32;
constexpr FViewportId INVALID_VIEWPORT_ID = UINT32_MAX;
constexpr int32 MAX_VIEWPORTS = 4;

struct FRect
{
	int32 X = 0;
	int32 Y = 0;
	int32 Width = 0;
	int32 Height = 0;

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
	float GridSize = 100.f;

	static FViewportLocalState CreateDefault(EViewportType Type);
	FMatrix BuildViewMatrix() const;
	FMatrix BuildProjMatrix(float AspectRatio) const;
	bool GetViewportRectInClient(int32& OutX, int32& OutY, int32& OutWidth, int32& OutHeight) const;
};

struct FViewportEntry
{
	FViewportId Id = INVALID_VIEWPORT_ID;
	EViewportType Type = EViewportType::Perspective;
	FViewport* Viewport;
	bool bActive = false;
	FViewportLocalState LocalState;
};