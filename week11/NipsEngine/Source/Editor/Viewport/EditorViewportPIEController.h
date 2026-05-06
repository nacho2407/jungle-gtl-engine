#pragma once

#include "Editor/PIE/PIESession.h"

class APlayerController;
class FEditorInputRouter;
class FViewportCamera;
class UEditorEngine;
class UWorld;
struct FViewportInputContext;

class FEditorViewportPIEController
{
public:
	static void StartPIE(
		UEditorEngine* Editor,
		UWorld* World,
		FEditorInputRouter& InputRouter,
		FViewportCamera& Camera,
		bool& bControlLocked);

	static void EndPIE(
		UEditorEngine* Editor,
		UWorld* World,
		FEditorInputRouter& InputRouter,
		FViewportCamera& Camera,
		bool& bControlLocked);

	static bool IsPossessed(bool bPIEActive, UEditorEngine* Editor, const FEditorInputRouter& InputRouter);
	static bool IsEditorControlMode(bool bPIEActive, UEditorEngine* Editor, const FEditorInputRouter& InputRouter);
	static bool AllowsEditorWorldControl(bool bPIEActive, UEditorEngine* Editor, const FEditorInputRouter& InputRouter);

	static APlayerController* GetPlayerController(UEditorEngine* Editor, const FEditorInputRouter& InputRouter);
	static void SetPlayerController(UEditorEngine* Editor, FEditorInputRouter& InputRouter, APlayerController* Controller);
	static void ClearPlayerController(UEditorEngine* Editor, FEditorInputRouter& InputRouter);

	static void RequestEndPIE(UEditorEngine* Editor);
	static bool ExecuteShellCommand(
		UEditorEngine* Editor,
		EPIESessionShellCommand Command,
		IPIESessionShellCommandHandler& Handler);

	static bool IsMouseFocusReleased(UEditorEngine* Editor);
	static void MarkMouseFocusReleased(UEditorEngine* Editor);
	static void ClearMouseFocusReleased(UEditorEngine* Editor);
	static void ReleaseMouseFocus(UEditorEngine* Editor, bool& bControlLocked);

	static void EnterEditorControlMode(
		bool bPIEActive,
		UEditorEngine* Editor,
		UWorld* World,
		FEditorInputRouter& InputRouter,
		FViewportCamera& Camera,
		bool& bControlLocked);

	static void EnterPossessedMode(
		bool bPIEActive,
		UEditorEngine* Editor,
		UWorld* World,
		FEditorInputRouter& InputRouter,
		FViewportCamera& Camera,
		bool& bControlLocked);
};
