#pragma once

#include "Render/Common/RenderTypes.h"
#include "Engine/Geometry/Ray.h"
#include "Core/CollisionTypes.h"
#include "Runtime/ViewportClient.h"
#include "Engine/Input/Controller/EditorController/EditorInputRouter.h"
#include "Spatial/WorldSpatialIndex.h"
#include "Editor/EditorUtils.h"
#include "Editor/Input/EditorViewportTools.h"
#include "Camera/ViewportCamera.h"

#include <memory>

enum EEditorViewportType
{
	EVT_Perspective = 0,		// Perspective
	EVT_OrthoXY = 1,			// Top
	EVT_OrthoXZ = 2,			// Right
	EVT_OrthoYZ = 3,			// Back
	EVT_OrthoNegativeXY = 4,	// Bottom
	EVT_OrthoNegativeXZ = 5,	// Left
	EVT_OrthoNegativeYZ = 6,	// Front

	EVT_OrthoTop    = EVT_OrthoXY,			// TOP
	EVT_OrthoLeft   = EVT_OrthoXZ,			// Left
	EVT_OrthoFront  = EVT_OrthoNegativeYZ,	// Front
	EVT_OrthoBack   = EVT_OrthoYZ,			// Back
	EVT_OrthoBottom = EVT_OrthoNegativeXY,	// Bottom
	EVT_OrthoRight  = EVT_OrthoNegativeXZ,	// Right
	LVT_MAX = 7,
};


class UEditorEngine;
class UWorld;
class UGizmoComponent;
class FEditorSettings;
class FWindowsWindow;
class FSelectionManager;
class FSceneViewport;
class FViewportCamera;
class APlayerController;
struct FEditorViewportState;

/*
* Per-viewport camera / view mode / input / picking / gizmo.
* BuildSceneView, orthographic/perspective branching, and gizmo axis visibility
* branching all live here.
*/

class FEditorViewportClient : public FViewportClient
{
	friend class FEditorViewportCommandTool;
	friend class FEditorViewportGizmoTool;
	friend class FEditorViewportSelectionTool;
	friend class FEditorViewportNavigationTool;

public:
	enum class ETransformMode
	{
		Select,
		Translate,
		Rotate,
		Scale,
	};

	void Initialize(FWindowsWindow* InWindow, UEditorEngine* InEditor);
	UWorld* GetFocusedWorld() const { return World; }
	void SetWorld(UWorld* InWorld);
	void StartPIE(UWorld* InWorld);
	void EndPIE(UWorld* InWorld);

	// PIE 상태 (뷰포트별 독립)
	EViewportPlayState GetPlayState() const { return PlayState; }
	void SetPlayState(EViewportPlayState InState) { PlayState = InState; }
	bool IsPIEActive() const { return PlayState != EViewportPlayState::Editing; }
	bool IsPIEPossessed() const { return IsPIEActive() && InputRouter.GetActiveController() == EActiveEditorController::PIEController; }
	bool IsPIEEditorControlMode() const { return IsPIEActive() && InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController; }
	bool AllowsEditorWorldControl() const { return InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController; }

	// PIE 시작 전 카메라 상태 저장 / 정지 시 복원
	void SaveCameraSnapshot();
	void RestoreCameraSnapshot();

	void SetGizmo(UGizmoComponent* InGizmo);
	void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	void SetSelectionManager(FSelectionManager* InSelectionManager);

	UGizmoComponent* GetGizmo() { return Gizmo; }

	/** Override to also resize the camera. */
	void SetViewportSize(float InWidth, float InHeight) override;

	float GetMoveSpeed() { return InputRouter.GetEditorWorldController().GetMoveSpeed(); }
	void  SetMoveSpeed(float InSpeed) { InputRouter.GetEditorWorldController().SetMoveSpeed(InSpeed); }
	void  FocusSelection() { FocusPrimarySelection(); }
	void  RequestDeleteSelection() { DeleteSelectedActors(); }
	void  RequestSelectAllActors() { SelectAllActors(); }
	void  RequestDuplicateSelection() { DuplicateSelection(); }
	void  RequestSetSelectMode() { SetTransformMode(ETransformMode::Select); }
	void  RequestSetTranslateMode() { SetTransformMode(ETransformMode::Translate); }
	void  RequestSetRotateMode() { SetTransformMode(ETransformMode::Rotate); }
	void  RequestSetScaleMode() { SetTransformMode(ETransformMode::Scale); }
	void  RequestToggleCoordinateSpace();
	void  RequestSelectAtViewportLocalPoint(float LocalX, float LocalY, bool bToggle, bool bAdditive);
	ETransformMode GetTransformMode() const { return TransformMode; }

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	FViewportCamera*       GetCamera()       { return bHasCamera ? &Camera : nullptr; }
	const FViewportCamera* GetCamera() const { return bHasCamera ? &Camera : nullptr; }
	FViewportCamera*       GetRenderCamera();
	const FViewportCamera* GetRenderCamera() const;
	// 외부에서 카메라 위치를 변경한 후 컨트롤러의 TargetLocation을 동기화할 때 호출
	void SyncCameraTarget()
	{
		InputRouter.GetEditorWorldController().ResetTargetLocation();
		if (bHasCamera)
		{
			InputRouter.GetEditorWorldController().SetTargetRotation(Camera.GetRotation());
		}
	}

	void Tick(float DeltaTime) override;
	void BuildSceneView(FSceneView& OutView) const override;
	bool ProcessInput(FViewportInputContext& Context) override;
	bool WantsRelativeMouseMode(const FViewportInputContext& Context, POINT& OutRestoreScreenPos) const override;
	bool WantsAbsoluteMouseClip(const FViewportInputContext& Context, RECT& OutClipScreenRect) const override;
	void SetLegacyInputSuppressedThisFrame(bool bSuppress) { bLegacyInputSuppressedThisFrame = bSuppress; }

	// Get / Set
	EEditorViewportType  GetViewportType() const          { return ViewportType; }
	void                 SetViewportType(EEditorViewportType InType) { ViewportType = InType; }

	FSceneViewport*       GetViewport()       { return Viewport; }
	const FSceneViewport* GetViewport() const { return Viewport; }
	void                  SetViewport(FSceneViewport* InViewport) { Viewport = InViewport; }
	void                  SetViewportInputDeadZoneTop(float InPixels) { ViewportInputDeadZoneTop = InPixels; }

	FEditorViewportState*       GetViewportState()       { return State; }
	const FEditorViewportState* GetViewportState() const { return State; }
	void                        SetState(FEditorViewportState* InState) { State = InState; }

	/** Initialise camera position/orientation for the current ViewportType. */
	void ApplyCameraMode();

	bool  IsBoxSelecting()    const { return bBoxSelecting; }
	POINT GetBoxSelectStart() const { return BoxSelectStart; }
	POINT GetBoxSelectEnd()   const { return BoxSelectEnd; }
	void TriggerPIEStartOutlineFlash(float DurationSeconds = 0.35f);
	float GetPIEStartOutlineFlashAlpha() const;
	bool IsPointerInViewportInputDeadZone(float LocalY) const;

	void LockCursorToViewport();
	void SetEndPIECallback(std::function<void()> Callback) { InputRouter.GetPIEController().SetEndPIECallback(std::move(Callback)); }
	void ClearEndPIECallback()                             { InputRouter.GetPIEController().ClearEndPIECallback(); }
	void SetPIEPlayerController(APlayerController* InController) { InputRouter.GetPIEController().SetPlayerController(InController); }
	void ClearPIEPlayerController() { InputRouter.GetPIEController().ClearPlayerController(); }

private:
	// ── Tick sub-steps ───────────────────────────────────────────────────────
	void TickInput(float DeltaTime);          // top-level input tick (renamed from TickRouter)
	void TickInput(const FViewportInputContext& Context);
	void TickCursorCapture();                 // hide/lock cursor on editor-world drag begin/end
	void TickKeyboardInput();                 // poll watched keys -> route to active controller
	void TickKeyboardInput(const FViewportInputContext& Context);
	void TickEditorShortcuts();               // editor-only hotkeys (gizmo toggle, delete, select all)
	void TickEditorShortcuts(const FViewportInputContext& Context);
	void TickPIEShortCuts();				  // PIE-only hotkeys
	void TickPIEShortCuts(const FViewportInputContext& Context);
	void TickMouseInput(float VX, float VY);  // poll mouse state -> route to active controller
	void TickMouseInput(const FViewportInputContext& Context);
	void InitializeInputTools();
	bool TickInputContexts(const FViewportInputContext& Context);
	bool HandleCommandInput(const FViewportInputContext& Context);
	bool HandleGizmoInput(const FViewportInputContext& Context);
	bool HandleSelectionInput(const FViewportInputContext& Context);
	bool HandleNavigationInput(const FViewportInputContext& Context);
	bool HandleAltNavigationInput(const FViewportInputContext& Context);
	bool HandleLeftNavigationInput(const FViewportInputContext& Context);
	bool IsBoxSelectionChordActive(const FViewportInputContext& Context) const;
	bool IsPointerInViewportInputDeadZone(const FViewportInputContext& Context) const;
	void SetTransformMode(ETransformMode InMode);
	void CycleTransformMode();
	void CycleGizmoTransformMode();
	void ApplyTransformModeToGizmo();

	void TickInteraction(float DeltaTime);    // box selection + gizmo screen-scaling

	// ── Selection helpers ────────────────────────────────────────────────────
	void HandleBoxSelection();
	bool TryProjectWorldToViewport(const FVector& WorldPos, float& OutViewportX, float& OutViewportY, float& OutDepth) const;
	void FocusPrimarySelection();
	void DeleteSelectedActors();
	void SelectAllActors();
	void DuplicateSelection();
	void TogglePIEPossessEject();
	void ReleasePIEMouseFocus();
	void ReacquirePIEMouseFocus();
	bool TryReacquirePIEMouseFocusOnViewportClick(const FViewportInputContext& Context);
	void EnterPIEEditorControlMode();
	void EnterPIEPossessedMode();

private:
	// Window / Viewport — Window is inherited from FViewportClient
	UEditorEngine*		   Editor			= nullptr;
	FSceneViewport*		   Viewport			= nullptr;

	EEditorViewportType    ViewportType		= EVT_Perspective;
	FEditorViewportState*  State			= nullptr;

	UWorld*				   World            = nullptr;
	UGizmoComponent*	   Gizmo            = nullptr;
	const FEditorSettings* Settings			= nullptr;
	FSelectionManager*     SelectionManager = nullptr;

	FViewportCamera		   Camera;
	FEditorInputRouter	   InputRouter;
	TArray<std::unique_ptr<IEditorViewportTool>> InputTools;
	bool				   bHasCamera		= false;

	EViewportPlayState     PlayState       = EViewportPlayState::Editing;
	FCameraSnapshot        SavedCamera;
	bool                   bHasCameraSnapshot = false;

	bool  bBoxSelecting  = false;
	POINT BoxSelectStart = { 0, 0 };
	POINT BoxSelectEnd   = { 0, 0 };

	bool  bControlLocked = false;
	bool  bPIEMouseFocusReleased = false;
	bool  bRoutedInputProcessedThisFrame = false;
	bool  bLegacyInputSuppressedThisFrame = false;
	bool  bGizmoDragUndoCaptured = false;
	ETransformMode TransformMode = ETransformMode::Translate;
	float ViewportInputDeadZoneTop = 0.0f;
	float PIEStartOutlineFlashRemaining = 0.0f;
	float PIEStartOutlineFlashDuration = 0.0f;

	// Caller-owned scratch buffer for frustum queries in HandleBoxSelection
	FWorldSpatialIndex::FPrimitiveFrustumQueryScratch FrustumQueryScratch;
};
