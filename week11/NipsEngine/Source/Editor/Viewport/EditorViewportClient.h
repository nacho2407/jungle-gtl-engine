#pragma once

#include "Render/Common/RenderTypes.h"
#include "Engine/Geometry/Ray.h"
#include "Core/CollisionTypes.h"
#include "Runtime/ViewportClient.h"
#include "Engine/Input/Controller/EditorController/EditorInputRouter.h"
#include "Spatial/WorldSpatialIndex.h"

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
struct FEditorViewportState;

/*
* Per-viewport camera / view mode / input / picking / gizmo.
* BuildSceneView, orthographic/perspective branching, and gizmo axis visibility
* branching all live here.
*/

class FEditorViewportClient : public FViewportClient
{
public:
	void Initialize(FWindowsWindow* InWindow, UEditorEngine* InEditor);
	void SetWorld(UWorld* InWorld);
	void StartPIE(UWorld* InWorld);
	void EndPIE(UWorld* InWorld);

	void SetGizmo(UGizmoComponent* InGizmo)
	{
		Gizmo = InGizmo;
		InputRouter.GetEditorWorldController().SetGizmo(InGizmo);
	}
	void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	void SetSelectionManager(FSelectionManager* InSelectionManager);

	UGizmoComponent* GetGizmo() { return Gizmo; }

	/** Override to also resize the camera. */
	void SetViewportSize(float InWidth, float InHeight) override;

	float GetMoveSpeed() { return InputRouter.GetEditorWorldController().GetMoveSpeed(); }
	void  SetMoveSpeed(float InSpeed) { InputRouter.GetEditorWorldController().SetMoveSpeed(InSpeed); }
	void  FocusSelection() { FocusPrimarySelection(); }

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	FViewportCamera*       GetCamera()       { return bHasCamera ? &Camera : nullptr; }
	const FViewportCamera* GetCamera() const { return bHasCamera ? &Camera : nullptr; }
	// 외부에서 카메라 위치를 변경한 후 컨트롤러의 TargetLocation을 동기화할 때 호출
	void SyncCameraTarget() { InputRouter.GetEditorWorldController().ResetTargetLocation(); }

	void Tick(float DeltaTime) override;
	void BuildSceneView(FSceneView& OutView) const override;

	// Get / Set
	EEditorViewportType  GetViewportType() const          { return ViewportType; }
	void                 SetViewportType(EEditorViewportType InType) { ViewportType = InType; }

	FSceneViewport*       GetViewport()       { return Viewport; }
	const FSceneViewport* GetViewport() const { return Viewport; }
	void                  SetViewport(FSceneViewport* InViewport) { Viewport = InViewport; }

	FEditorViewportState*       GetViewportState()       { return State; }
	const FEditorViewportState* GetViewportState() const { return State; }
	void                        SetState(FEditorViewportState* InState) { State = InState; }

	/** Initialise camera position/orientation for the current ViewportType. */
	void ApplyCameraMode();

	/**
	 * Returns true while a drag operation (RMB or MMB) is in progress for this viewport.
	 * Used by the renderer to determine gizmo axis highlight behaviour.
	 */
	bool  IsActiveOperation() const;
	bool  IsBoxSelecting()    const { return bBoxSelecting; }
	POINT GetBoxSelectStart() const { return BoxSelectStart; }
	POINT GetBoxSelectEnd()   const { return BoxSelectEnd; }

	void LockCursorToViewport();
	void SetEndPIECallback(std::function<void()> Callback) { InputRouter.GetPIEController().SetEndPIECallback(std::move(Callback)); }
	void ClearEndPIECallback()                             { InputRouter.GetPIEController().ClearEndPIECallback(); }

private:
	// ── Tick sub-steps ───────────────────────────────────────────────────────
	void TickInput(float DeltaTime);          // top-level input tick (renamed from TickRouter)
	void TickCursorCapture();                 // hide/lock cursor on editor-world drag begin/end
	void TickKeyboardInput();                 // poll watched keys -> route to active controller
	void TickEditorShortcuts();               // editor-only hotkeys (gizmo toggle, delete, select all)
	void TickPIEShortCuts();				  // PIE-only hotkeys
	void TickMouseInput(float VX, float VY);  // poll mouse state -> route to active controller

	void TickInteraction(float DeltaTime);    // box selection + gizmo screen-scaling

	// ── Selection helpers ────────────────────────────────────────────────────
	void HandleBoxSelection();
	bool TryProjectWorldToViewport(const FVector& WorldPos, float& OutViewportX, float& OutViewportY, float& OutDepth) const;
	void FocusPrimarySelection();
	void DeleteSelectedActors();
	void SelectAllActors();

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
	bool				   bHasCamera		= false;

	bool  bBoxSelecting  = false;
	POINT BoxSelectStart = { 0, 0 };
	POINT BoxSelectEnd   = { 0, 0 };

	bool  bControlLocked = false;

	// Caller-owned scratch buffer for frustum queries in HandleBoxSelection
	FWorldSpatialIndex::FPrimitiveFrustumQueryScratch FrustumQueryScratch;
};
