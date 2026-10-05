#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class FSelectionManager;
class FEditorInputRouter;
struct FViewportInputContext;
class FViewportCamera;
class UGizmoComponent;

class FEditorViewportNavigationController
{
public:
	static bool FocusPrimarySelection(FSelectionManager* SelectionManager, FViewportCamera* Camera);
	static bool ResetCamera(FViewportCamera* Camera, const FVector& InitViewPos, const FVector& InitLookAt);
	static void ApplyCameraMode(FViewportCamera& Camera, int32 ViewportType);

	static bool HandleLeftNavigationInput(
		const FViewportInputContext& Context,
		FViewportCamera& Camera,
		FEditorInputRouter& InputRouter,
		UGizmoComponent* Gizmo,
		bool bEditorWorldControllerActive,
		bool bControlLocked,
		bool bHasCamera,
		bool bPointerInDeadZone,
		bool bLeftNavigationChord,
		float MoveSpeed);

	static bool HandleMoveSpeedWheel(
		const FViewportInputContext& Context,
		bool bEditorWorldControllerActive,
		float CurrentMoveSpeed,
		float& OutMoveSpeed);

	static bool HandleAltNavigationInput(
		const FViewportInputContext& Context,
		FViewportCamera& Camera,
		FSelectionManager* SelectionManager,
		bool bEditorWorldControllerActive,
		bool bControlLocked,
		bool bHasCamera,
		float MoveSpeed,
		bool& bOutSyncCameraTarget);

	static void RouteKeyboardNavigationInput(
		const FViewportInputContext& Context,
		FEditorInputRouter& InputRouter,
		bool bControlLocked,
		bool bRuntimeGameInputCaptured);

private:
	static bool IsEditorCameraKeyboardKey(int VK);
};
