#pragma once
#include <Core/CoreTypes.h>
#include "Core/CoreMinimal.h"
#include "Engine/Input/InputTypes.h"
#include "Engine/Runtime/WindowsWindow.h"

struct FSceneView;
struct FViewportMouseEvent;

/**
 * Viewport 의 행동 정책을 정의하는 베이스 인터페이스
 * EditorViewportClient / GameViewportClient 가 이를 상속받음.
 *
 * Window, viewport dimensions, and event hooks are provided here so that
 * any subclass (editor or game) can build on a consistent foundation
 * without redeclaring the same boilerplate.
 */
class FViewportClient
{
public:
	virtual ~FViewportClient() = default;

	/** Called once after construction to bind the OS window. */
	virtual void Initialize(FWindowsWindow* InWindow) { Window = InWindow; }

	/** Resize the logical viewport. Subclasses may override to respond. */
	virtual void SetViewportSize(float InWidth, float InHeight)
	{
		if (InWidth  > 0.f) WindowWidth  = InWidth;
		if (InHeight > 0.f) WindowHeight = InHeight;
	}

	virtual void Tick(float DeltaTime) = 0;

	/**
	 * Populate OutView with camera matrices and viewport rect for the renderer.
	 */
	virtual void BuildSceneView(FSceneView& OutView) const = 0;

	/*
	 * Input event hooks — default to unhandled (return false).
	 */
	virtual bool OnMouseMove(const FViewportMouseEvent& Ev)      { return false; }
	virtual bool OnMouseButtonDown(const FViewportMouseEvent& Ev){ return false; }
	virtual bool OnMouseButtonUp(const FViewportMouseEvent& Ev)  { return false; }
	virtual bool OnMouseWheel(float Delta)                       { return false; }
	virtual bool MouseLockTickGuard()                            { return false; }
	virtual bool OnKeyDown(uint32 Key)                           { return false; }
	virtual bool OnKeyUp(uint32 Key)                             { return false; }

	/**
	 * Frame-based input path used by the editor/game input router.
	 * Legacy event hooks stay available while clients migrate incrementally.
	 */
	virtual bool ProcessInput(FViewportInputContext& Context) { return false; }
	virtual bool WantsRelativeMouseMode(const FViewportInputContext& Context, POINT& OutRestoreScreenPos) const { return false; }
	virtual bool WantsAbsoluteMouseClip(const FViewportInputContext& Context, RECT& OutClipScreenRect) const { return false; }

protected:
	FWindowsWindow* Window       = nullptr;
	float           WindowWidth  = 1920.f;
	float           WindowHeight = 1080.f;
};
