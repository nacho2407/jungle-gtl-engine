#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Input/EditorViewportInputContexts.h"
#include "Editor/Input/EditorViewportInputMapping.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Slate/SlateApplication.h"
#include "EditorEngine.h"

#include "GameFramework/World.h"
#include "GameFramework/PlayerController.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Object/Object.h"
#include "Object/ActorIterator.h"
#include "Editor/Selection/SelectionManager.h"
#include "Runtime/SceneView.h"
#include "EditorUtils.h"
#include "Math/Vector4.h"
#include "Slate/SWidget.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace
{
bool IsEditorCameraKeyboardKey(int VK)
{
	switch (VK)
	{
	case 'W':
	case 'A':
	case 'S':
	case 'D':
	case 'Q':
	case 'E':
	case VK_LEFT:
	case VK_RIGHT:
	case VK_UP:
	case VK_DOWN:
		return true;
	default:
		return false;
	}
}

bool IsRuntimeGameInputCaptured()
{
	return GEngine
		&& GEngine->GetRuntimeInputMode() == ERuntimeInputMode::GameOnly
		&& GEngine->IsRuntimeCursorLocked();
}
}

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow, UEditorEngine* InEditor)
{
	FViewportClient::Initialize(InWindow);
	Editor = InEditor;
	InputRouter.SetActiveController(EActiveEditorController::EditorWorldController);
	InitializeInputTools();
}

void FEditorViewportClient::SetWorld(UWorld* InWorld)
{
	World = InWorld;
	InputRouter.GetEditorWorldController().SetWorld(InWorld);
}

void FEditorViewportClient::StartPIE(UWorld* InWorld)
{
	World = InWorld;
	InputRouter.GetEditorWorldController().SetWorld(InWorld);
	InputRouter.GetPIEController().SetCamera(&Camera); // re-sync Yaw/Pitch
    InputRouter.GetPIEController().SetTargetLocation(InputRouter.GetEditorWorldController().GetTargetLocation());
	InputRouter.SetActiveController(EActiveEditorController::PIEController);
	bControlLocked = false;
	bPIEMouseFocusReleased = false;
	TriggerPIEStartOutlineFlash();
}

void FEditorViewportClient::EndPIE(UWorld* InWorld)
{
	World = InWorld;
	if (InputRouter.GetActiveController() == EActiveEditorController::PIEController)
	{
		InputRouter.GetEditorWorldController().SetTargetLocation(InputRouter.GetPIEController().GetTargetLocation());
	}
	InputRouter.GetEditorWorldController().SetWorld(InWorld);
	InputRouter.SetActiveController(EActiveEditorController::EditorWorldController);
	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
	ClearEndPIECallback();
	ClearPIEPlayerController();
	InputSystem::Get().LockMouse(false);
	bControlLocked = false;
	bPIEMouseFocusReleased = false;
}

void FEditorViewportClient::SetSelectionManager(FSelectionManager* InSelectionManager)
{
	SelectionManager = InSelectionManager;
	InputRouter.GetEditorWorldController().SetSelectionManager(InSelectionManager);
}

void FEditorViewportClient::CreateCamera()
{
	bHasCamera = true;
	Camera = FViewportCamera();
	Camera.OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
	InputRouter.GetEditorWorldController().SetCamera(&Camera);
	InputRouter.GetPIEController().SetCamera(&Camera);
	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
	InputRouter.GetPIEController().ResetTargetLocation();
}

void FEditorViewportClient::DestroyCamera()
{
	bHasCamera = false;
	InputRouter.GetEditorWorldController().NullifyCamera();
}

void FEditorViewportClient::ResetCamera()
{
	if (!bHasCamera || !Settings)
		return;

	Camera.SetLocation(Settings->InitViewPos);

	const FVector Forward = (Settings->InitLookAt - Settings->InitViewPos).GetSafeNormal();
	if (!Forward.IsNearlyZero())
	{
		FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
		if (!Right.IsNearlyZero())
		{
			FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();
			FMatrix RotationMatrix = FMatrix::Identity;
			RotationMatrix.SetAxes(Forward, Right, Up);

			FQuat NewRotation(RotationMatrix);
			NewRotation.Normalize();
			Camera.SetRotation(NewRotation);
		}
	}
	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
}

FViewportCamera* FEditorViewportClient::GetRenderCamera()
{
	if (!bHasCamera)
	{
		return nullptr;
	}

	if (IsPIEPossessed() && World)
	{
		if (FViewportCamera* ActiveCamera = World->GetActiveCamera())
		{
			return ActiveCamera;
		}
	}

	return &Camera;
}

const FViewportCamera* FEditorViewportClient::GetRenderCamera() const
{
	return const_cast<FEditorViewportClient*>(this)->GetRenderCamera();
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	FViewportClient::SetViewportSize(InWidth, InHeight);

	if (bHasCamera)
		Camera.OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
}

void FEditorViewportClient::SetGizmo(UGizmoComponent* InGizmo)
{
	Gizmo = InGizmo;
	InputRouter.GetEditorWorldController().SetGizmo(InGizmo);
	ApplyTransformModeToGizmo();
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	if (PIEStartOutlineFlashRemaining > 0.0f)
	{
		PIEStartOutlineFlashRemaining = std::max(0.0f, PIEStartOutlineFlashRemaining - DeltaTime);
	}

	if (bRoutedInputProcessedThisFrame)
	{
		TickInteraction(DeltaTime);
		bRoutedInputProcessedThisFrame = false;
		bLegacyInputSuppressedThisFrame = false;
		return;
	}

	if (State && !State->bHovered)
	{
		bLegacyInputSuppressedThisFrame = false;
		return;
	}

	if (!bLegacyInputSuppressedThisFrame)
	{
		TickInput(DeltaTime);
	}
	TickInteraction(DeltaTime);
	bLegacyInputSuppressedThisFrame = false;
}

void FEditorViewportClient::TriggerPIEStartOutlineFlash(float DurationSeconds)
{
	PIEStartOutlineFlashDuration = std::max(0.01f, DurationSeconds);
	PIEStartOutlineFlashRemaining = PIEStartOutlineFlashDuration;
}

float FEditorViewportClient::GetPIEStartOutlineFlashAlpha() const
{
	if (PIEStartOutlineFlashDuration <= 0.0f || PIEStartOutlineFlashRemaining <= 0.0f)
	{
		return 0.0f;
	}
	return MathUtil::Clamp(PIEStartOutlineFlashRemaining / PIEStartOutlineFlashDuration, 0.0f, 1.0f);
}

void FEditorViewportClient::RequestToggleCoordinateSpace()
{
	if (Gizmo)
	{
		Gizmo->SetWorldSpace(!Gizmo->IsWorldSpace());
	}
}

void FEditorViewportClient::RequestSelectAtViewportLocalPoint(float LocalX, float LocalY, bool bToggle, bool bAdditive)
{
	if (!World || !SelectionManager || !bHasCamera || !Viewport)
	{
		return;
	}

	const FViewportRect& Rect = Viewport->GetRect();
	if (Rect.Width <= 0 || Rect.Height <= 0)
	{
		return;
	}

	const FRay Ray = Camera.DeprojectScreenToWorld(LocalX, LocalY, static_cast<float>(Rect.Width), static_cast<float>(Rect.Height));
	AActor* BestActor = nullptr;
	float ClosestDist = FLT_MAX;

	FWorldSpatialIndex::FPrimitiveRayQueryScratch RayQueryScratch;
	TArray<UPrimitiveComponent*> CandidatePrimitives;
	TArray<float> CandidateTs;
	World->GetSpatialIndex().RayQueryPrimitives(Ray, CandidatePrimitives, CandidateTs, RayQueryScratch);

	for (int32 CandidateIndex = 0; CandidateIndex < static_cast<int32>(CandidatePrimitives.size()); ++CandidateIndex)
	{
		if (CandidateTs[CandidateIndex] > ClosestDist)
		{
			break;
		}

		UPrimitiveComponent* PrimitiveComp = CandidatePrimitives[CandidateIndex];
		AActor* Actor = PrimitiveComp ? PrimitiveComp->GetOwner() : nullptr;
		if (!Actor || !Actor->GetRootComponent())
		{
			continue;
		}

		FHitResult HitResult{};
		if (PrimitiveComp->Raycast(Ray, HitResult) && HitResult.Distance < ClosestDist)
		{
			ClosestDist = HitResult.Distance;
			BestActor = Actor;
		}
	}

	if (!BestActor)
	{
		if (!bToggle && !bAdditive)
		{
			SelectionManager->ClearSelection();
		}
		return;
	}

	if (bToggle)
	{
		SelectionManager->ToggleSelect(BestActor);
	}
	else if (bAdditive)
	{
		SelectionManager->AddSelect(BestActor);
	}
	else
	{
		SelectionManager->Select(BestActor);
	}
}

bool FEditorViewportClient::ProcessInput(FViewportInputContext& Context)
{
	if (!bHasCamera)
		return false;

	TickInput(Context);
	bRoutedInputProcessedThisFrame = true;
	return true;
}

bool FEditorViewportClient::WantsRelativeMouseMode(const FViewportInputContext& Context, POINT& OutRestoreScreenPos) const
{
	OutRestoreScreenPos = Context.Frame.MouseScreenPos;

	if (!bHasCamera || bControlLocked)
	{
		return false;
	}

	if (Context.bImGuiCapturedMouse || IsPointerInViewportInputDeadZone(Context))
	{
		return false;
	}

	if (InputRouter.GetActiveController() == EActiveEditorController::PIEController)
	{
		return IsPIEActive()
			&& IsRuntimeGameInputCaptured()
			&& !bPIEMouseFocusReleased
			&& (Context.bFocused || Context.bCaptured || Context.bHovered || Context.bRelativeMouseMode);
	}

	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
	{
		return false;
	}

	const bool bGizmoInteractionActive = Gizmo && (Gizmo->IsHolding() || Gizmo->IsPressedOnHandle());
	if (bGizmoInteractionActive)
	{
		return false;
	}

	bool bGizmoBlocksLeftRelativeDrag = Gizmo && Gizmo->IsHovered();
	if (!bGizmoBlocksLeftRelativeDrag && Gizmo && Viewport && Context.Frame.IsDown(VK_LBUTTON))
	{
		const FViewportRect& Rect = Viewport->GetRect();
		if (Rect.Width > 0 && Rect.Height > 0)
		{
			const float LocalMouseX = static_cast<float>(Context.MouseLocalPos.x);
			const float LocalMouseY = static_cast<float>(Context.MouseLocalPos.y);
			const FRay MouseRay = Camera.DeprojectScreenToWorld(LocalMouseX, LocalMouseY, static_cast<float>(Rect.Width), static_cast<float>(Rect.Height));
			FHitResult GizmoHit{};
			bGizmoBlocksLeftRelativeDrag = Gizmo->RaycastMesh(MouseRay, GizmoHit);
		}
	}

	const bool bPlainPerspectiveLeftLookChord =
		!Camera.IsOrthographic()
		&& TransformMode != ETransformMode::Select
		&& !bBoxSelecting
		&& !IsBoxSelectionChordActive(Context)
		&& Context.Frame.IsDown(VK_LBUTTON)
		&& !Context.Frame.IsAltDown()
		&& !Context.Frame.IsCtrlDown()
		&& !Context.Frame.IsShiftDown();
	const bool bLeftGesture =
		Context.Frame.bLeftDragging
		|| Context.WasPointerDragStarted(EPointerButton::Left)
		|| Context.bRelativeMouseMode;
	const bool bLeftLookDrag =
		bPlainPerspectiveLeftLookChord
		&& bLeftGesture
		&& !bGizmoBlocksLeftRelativeDrag;
	const bool bRightLookDrag =
		Context.Frame.IsDown(VK_RBUTTON)
		&& (Context.Frame.bRightDragging || Context.WasPointerDragStarted(EPointerButton::Right) || Context.bRelativeMouseMode);
	const bool bMouseOwnedByViewport = Context.bCaptured || Context.bHovered || Context.bRelativeMouseMode;
	if (!bMouseOwnedByViewport)
	{
		return false;
	}

	return bRightLookDrag
		|| Context.Frame.IsDown(VK_MBUTTON)
		|| (Context.Frame.IsDown(VK_LBUTTON) && Context.Frame.IsAltDown() && !Context.Frame.IsCtrlDown())
		|| Context.Frame.bMiddleDragging
		|| bLeftLookDrag;
}

bool FEditorViewportClient::WantsAbsoluteMouseClip(const FViewportInputContext& Context, RECT& OutClipScreenRect) const
{
	(void)Context;

	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
	{
		return false;
	}
	if (!Window || !Viewport || !Gizmo || !Gizmo->IsVisible() || !Gizmo->IsHolding())
	{
		return false;
	}

	const FViewportRect& ViewportRect = Viewport->GetRect();
	if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
	{
		return false;
	}

	POINT TopLeft =
	{
		static_cast<LONG>(ViewportRect.X),
		static_cast<LONG>(ViewportRect.Y + ViewportInputDeadZoneTop)
	};
	POINT BottomRight =
	{
		static_cast<LONG>(ViewportRect.X + ViewportRect.Width),
		static_cast<LONG>(ViewportRect.Y + ViewportRect.Height)
	};

	::ClientToScreen(Window->GetHWND(), &TopLeft);
	::ClientToScreen(Window->GetHWND(), &BottomRight);

	OutClipScreenRect.left = TopLeft.x;
	OutClipScreenRect.top = TopLeft.y;
	OutClipScreenRect.right = BottomRight.x;
	OutClipScreenRect.bottom = BottomRight.y;
	return OutClipScreenRect.right > OutClipScreenRect.left && OutClipScreenRect.bottom > OutClipScreenRect.top;
}

void FEditorViewportClient::BuildSceneView(FSceneView& OutView) const
{
	if (!bHasCamera) return;

	const FViewportCamera* RenderCamera = GetRenderCamera();
	if (!RenderCamera)
	{
		return;
	}

	if (Viewport)
	{
		const FViewportRect& Rect = Viewport->GetRect();
		if (Rect.Width > 0 && Rect.Height > 0)
		{
			const_cast<FViewportCamera*>(RenderCamera)->OnResize(
				static_cast<uint32>(Rect.Width),
				static_cast<uint32>(Rect.Height));
		}
	}

	OutView.ViewMatrix           = RenderCamera->GetViewMatrix();
	OutView.ProjectionMatrix     = RenderCamera->GetProjectionMatrix();
	OutView.ViewProjectionMatrix = OutView.ViewMatrix * OutView.ProjectionMatrix;

	OutView.CameraPosition = RenderCamera->GetLocation();
	OutView.CameraForward  = RenderCamera->GetForwardVector();
	OutView.CameraRight    = RenderCamera->GetRightVector();
	OutView.CameraUp       = RenderCamera->GetUpVector();

	OutView.bOrthographic = RenderCamera->IsOrthographic();

    OutView.CameraOrthoHeight = RenderCamera->GetOrthoHeight();

	OutView.CameraFrustum = RenderCamera->GetFrustum();

	if (State)
	{
		OutView.ViewRect = Viewport->GetRect();
		OutView.ViewMode = State->ViewMode;
		OutView.LightCullMode = State->LightCullMode;
	}
}

void FEditorViewportClient::ApplyCameraMode()
{
	// Orthographic views reset rotation so the existing value doesn't interfere with LookAt.
	Camera.SetRotation(FRotator(0.f, 0.f, 0.f));

	switch (ViewportType)
	{
	case EVT_Perspective:
		Camera.SetProjectionType(EViewportProjectionType::Perspective);
		Camera.ClearCustomLookDir();
		Camera.SetLocation(FVector(5.f, 3.f, 5.f));
		Camera.SetLookAt(FVector(0.f, 0.f, 0.f));
		break;

	// Orthographic views (X=Forward, Y=Right, Z=Up)

	case EVT_OrthoTop:			// top-down (-Z), screen-up = +X
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 0.f, 1000.f));
		Camera.SetCustomLookDir(FVector(0.f, 0.f, -1.f), FVector(1.f, 0.f, 0.f));
		break;

	case EVT_OrthoBottom:		// bottom-up (+Z), screen-up = +X
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 0.f, -1000.f));
		Camera.SetCustomLookDir(FVector(0.f, 0.f, 1.f), FVector(1.f, 0.f, 0.f));
		break;

	case EVT_OrthoFront:		// front (-X->+X), screen-up = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(1000.f, 0.f, 0.f));
		Camera.SetCustomLookDir(FVector(-1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoBack:			// back (+X->-X), screen-up = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(-1000.f, 0.f, 0.f));
		Camera.SetCustomLookDir(FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoLeft:			// left (-Y->+Y), screen-up = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, -1000.f, 0.f));
		Camera.SetCustomLookDir(FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoRight:		// right (+Y->-Y), screen-up = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 1000.f, 0.f));
		Camera.SetCustomLookDir(FVector(0.f, -1.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	default:
		break;
	}

	// Reset lerp target immediately so accumulated TargetLocation doesn't
	// move the camera on the next Tick after a mode switch.
	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
}

// ── Input tick sub-steps ──────────────────────────────────────────────────────

void FEditorViewportClient::TickInput(float DeltaTime)
{
	if (!bHasCamera)
		return;

	const float VX = State ? static_cast<float>(Viewport->GetRect().X) : 0.f;
    const float VY = State ? static_cast<float>(Viewport->GetRect().Y) : 0.f;

	TickCursorCapture();
	InputRouter.SetViewportDim(VX, VY, WindowWidth, WindowHeight);
	InputRouter.Tick(DeltaTime);
	TickKeyboardInput();
	TickEditorShortcuts();
	TickPIEShortCuts();
	TickMouseInput(VX, VY);
}

void FEditorViewportClient::TickInput(const FViewportInputContext& Context)
{
	if (!bHasCamera)
		return;

	const float VX = State ? static_cast<float>(Viewport->GetRect().X) : 0.f;
	const float VY = State ? static_cast<float>(Viewport->GetRect().Y) : 0.f;

	InputRouter.SetViewportDim(VX, VY, WindowWidth, WindowHeight);
	InputRouter.Tick(Context.DeltaSeconds);
	if (TryReacquirePIEMouseFocusOnViewportClick(Context))
	{
		bRoutedInputProcessedThisFrame = true;
		return;
	}
	TickInputContexts(Context);
}

void FEditorViewportClient::TickCursorCapture()
{
	// Cursor ownership is now centralized in FInputRouter/FCursorControl.
	// Keep the legacy path from hiding the cursor a second time; only clean up
	// stale locks after buttons are released.
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
	{
		return;
	}

    const InputSystem& IS = InputSystem::Get();
    const bool bAnyMouseReleased = IS.GetKeyUp(VK_LBUTTON) || IS.GetKeyUp(VK_RBUTTON) || IS.GetKeyUp(VK_MBUTTON);
    if (bAnyMouseReleased && !IS.GetKey(VK_LBUTTON) && !IS.GetKey(VK_RBUTTON) && !IS.GetKey(VK_MBUTTON))
    {
        InputSystem::Get().LockMouse(false);
    }
}

void FEditorViewportClient::TickKeyboardInput()
{
	static constexpr int WatchKeys[] = {
		'W', 'A', 'S', 'D', 'Q', 'E',
		VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
		VK_SPACE, VK_ESCAPE,
	};

	if (bControlLocked) return;
	if (InputRouter.GetActiveController() == EActiveEditorController::PIEController && !IsRuntimeGameInputCaptured())
	{
		return;
	}
	const InputSystem& IS = InputSystem::Get();
	const bool bEditorWorld = InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController;
	const bool bKeyboardNavigationChord =
		IS.GetKey(VK_RBUTTON)
		|| (IS.GetKey(VK_LBUTTON) && !IS.GetKey(VK_CONTROL) && !IS.GetKey(VK_MENU))
		|| IS.GetKey(VK_MBUTTON);
	for (int VK : WatchKeys)
	{
		if (bEditorWorld && IsEditorCameraKeyboardKey(VK) && !bKeyboardNavigationChord)
		{
			continue;
		}
		if (IS.GetKeyDown(VK)) InputRouter.RouteKeyboardInput(EKeyInputType::KeyPressed,  VK);
		if (IS.GetKey(VK))     InputRouter.RouteKeyboardInput(EKeyInputType::KeyDown,     VK);
		if (IS.GetKeyUp(VK))   InputRouter.RouteKeyboardInput(EKeyInputType::KeyReleased, VK);
	}
}

void FEditorViewportClient::TickKeyboardInput(const FViewportInputContext& Context)
{
	static constexpr int WatchKeys[] = {
		'W', 'A', 'S', 'D', 'Q', 'E',
		VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
		VK_SPACE, VK_ESCAPE,
	};

	if (bControlLocked) return;
	if (InputRouter.GetActiveController() == EActiveEditorController::PIEController && !IsRuntimeGameInputCaptured())
	{
		return;
	}
	const bool bEditorWorld = InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController;
	const bool bKeyboardNavigationChord =
		Context.Frame.IsDown(VK_RBUTTON)
		|| (Context.Frame.IsDown(VK_LBUTTON) && !Context.Frame.IsCtrlDown() && !Context.Frame.IsAltDown())
		|| Context.Frame.IsDown(VK_MBUTTON)
		|| Context.bRelativeMouseMode;

	for (int VK : WatchKeys)
	{
		if (bEditorWorld && IsEditorCameraKeyboardKey(VK) && !bKeyboardNavigationChord)
		{
			continue;
		}
		if (Context.WasPressed(VK)) InputRouter.RouteKeyboardInput(EKeyInputType::KeyPressed, VK);
		if (Context.Frame.IsDown(VK)) InputRouter.RouteKeyboardInput(EKeyInputType::KeyDown, VK);
		if (Context.WasReleased(VK)) InputRouter.RouteKeyboardInput(EKeyInputType::KeyReleased, VK);
	}
}

void FEditorViewportClient::TickEditorShortcuts()
{
	// These shortcuts only apply in the editor world, not during PIE.
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
		return;

	const bool bInPIE = IsPIEActive();
	const InputSystem& IS        = InputSystem::Get();
	const bool         bCtrlDown = IS.GetKey(VK_CONTROL);
	const bool         bAltDown  = IS.GetKey(VK_MENU);
	const bool         bShiftDown = IS.GetKey(VK_SHIFT);
	const bool         bMouseNavigationActive =
		IS.GetRightDragging()
		|| IS.GetMiddleDragging()
		|| (IS.GetKey(VK_LBUTTON) && !bCtrlDown);

	if (!bMouseNavigationActive)
	{
		if (IS.GetKeyUp(VK_TAB))
			CycleTransformMode();
		if (IS.GetKeyUp(VK_SPACE))
			CycleGizmoTransformMode();
		if (IS.GetKeyUp('1') || IS.GetKeyUp('Q'))
			SetTransformMode(ETransformMode::Select);
		if (IS.GetKeyUp('2') || IS.GetKeyUp('W'))
			SetTransformMode(ETransformMode::Translate);
		if (IS.GetKeyUp('3') || IS.GetKeyUp('E'))
			SetTransformMode(ETransformMode::Rotate);
		if (IS.GetKeyUp('4') || IS.GetKeyUp('R'))
			SetTransformMode(ETransformMode::Scale);
	}

	if (IS.GetKeyUp('X') && Gizmo)
		Gizmo->SetWorldSpace(!Gizmo->IsWorldSpace());

	if (IS.GetKeyUp(VK_DELETE))
		DeleteSelectedActors();

	if (IS.GetKeyDown('A') && bCtrlDown && !bAltDown)
		SelectAllActors();

	if (!bInPIE && IS.GetKeyDown('Z') && bCtrlDown && !bAltDown && Editor)
	{
		if (bShiftDown)
			Editor->Redo();
		else
			Editor->Undo();
	}
}

void FEditorViewportClient::TickEditorShortcuts(const FViewportInputContext& Context)
{
	using EEditorViewportAction = EditorViewportInputMapping::EEditorViewportAction;
	const bool bInPIE = IsPIEActive();

	const bool bMouseNavigationActive =
		EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavLookRightDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavLookMiddleDown)
		|| (Context.Frame.IsDown(VK_LBUTTON) && !Context.Frame.IsCtrlDown() && !Context.Frame.IsAltDown())
		|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavOrbitAltLeftDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavDollyAltRightDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavPanAltMiddleDown);

	// These shortcuts only apply in the editor world, not during PIE.
	if (InputRouter.GetActiveController() == EActiveEditorController::PIEController)
	{
		if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::EndPIE) && Editor)
		{
			Editor->StopPlaySession();
		}
		if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::TogglePIEPossessEject))
		{
			TogglePIEPossessEject();
		}
		if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::ReleasePIEMouseFocus))
		{
			ReleasePIEMouseFocus();
		}
		return;
	}

	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::CycleMode))
	{
		CycleTransformMode();
	}

	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::CycleGizmoMode))
	{
		CycleGizmoTransformMode();
	}

	if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::ToggleCoordinateSpace) && Gizmo)
	{
		Gizmo->SetWorldSpace(!Gizmo->IsWorldSpace());
	}

	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SetModeSelect))
	{
		SetTransformMode(ETransformMode::Select);
	}
	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SetModeTranslate))
	{
		SetTransformMode(ETransformMode::Translate);
	}
	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SetModeRotate))
	{
		SetTransformMode(ETransformMode::Rotate);
	}
	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SetModeScale))
	{
		SetTransformMode(ETransformMode::Scale);
	}

	if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::FocusSelection))
	{
		FocusPrimarySelection();
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::Undo) && Editor)
	{
		Editor->Undo();
		return;
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::Redo) && Editor)
	{
		Editor->Redo();
		return;
	}

	if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::DeleteSelection))
	{
		DeleteSelectedActors();
	}

	if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SelectAll))
	{
		SelectAllActors();
	}

	if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::DuplicateSelection))
	{
		DuplicateSelection();
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NewScene) && Editor)
	{
		Editor->GetMainPanel().RequestNewScene();
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::LoadScene) && Editor)
	{
		Editor->GetMainPanel().RequestLoadSceneWithDialog();
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SaveScene) && Editor)
	{
		Editor->GetMainPanel().RequestSaveScene();
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SaveSceneAs) && Editor)
	{
		Editor->GetMainPanel().RequestSaveSceneAsWithDialog();
	}
}

void FEditorViewportClient::TickPIEShortCuts()
{
	if (!IsPIEActive()) return;

	InputSystem& IS = InputSystem::Get();

	if (IS.GetKeyDown(VK_ESCAPE) && Editor)
	{
		Editor->StopPlaySession();
	}
	if (IS.GetKeyDown(VK_F8))
	{
		TogglePIEPossessEject();
	}
	if (IS.GetKeyDown(VK_F1) && IS.GetKey(VK_SHIFT))
	{
		ReleasePIEMouseFocus();
	}
}

void FEditorViewportClient::TickPIEShortCuts(const FViewportInputContext& Context)
{
	if (!IsPIEActive()) return;

	if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::EndPIE) && Editor)
	{
		Editor->StopPlaySession();
	}
	if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::TogglePIEPossessEject))
	{
		TogglePIEPossessEject();
	}
	if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::ReleasePIEMouseFocus))
	{
		ReleasePIEMouseFocus();
	}
}

void FEditorViewportClient::InitializeInputTools()
{
	InputTools.clear();
	InputTools.push_back(std::make_unique<FEditorViewportCommandTool>());
	InputTools.push_back(std::make_unique<FEditorViewportGizmoTool>());
	InputTools.push_back(std::make_unique<FEditorViewportSelectionTool>());
	InputTools.push_back(std::make_unique<FEditorViewportNavigationTool>());

	std::sort(InputTools.begin(), InputTools.end(),
		[](const std::unique_ptr<IEditorViewportTool>& Lhs, const std::unique_ptr<IEditorViewportTool>& Rhs)
		{
			return Lhs->GetPriority() > Rhs->GetPriority();
		});
}

bool FEditorViewportClient::TickInputContexts(const FViewportInputContext& Context)
{
	if (InputTools.empty())
	{
		InitializeInputTools();
	}

	for (const std::unique_ptr<IEditorViewportTool>& Tool : InputTools)
	{
		if (Tool && Tool->HandleInput(*this, Context))
		{
			return true;
		}
	}

	return false;
}

bool FEditorViewportClient::HandleCommandInput(const FViewportInputContext& Context)
{
	using EEditorViewportAction = EditorViewportInputMapping::EEditorViewportAction;

	const bool bInPIE = IsPIEActive();
	const bool bPIEPossessed = IsPIEPossessed();
	const TArray<EEditorViewportAction> CommandActions = bPIEPossessed
		? TArray<EEditorViewportAction>
		{
			EEditorViewportAction::EndPIE,
			EEditorViewportAction::TogglePIEPossessEject,
			EEditorViewportAction::ReleasePIEMouseFocus
		}
		: (bInPIE
		? TArray<EEditorViewportAction>
		{
			EEditorViewportAction::EndPIE,
			EEditorViewportAction::TogglePIEPossessEject,
			EEditorViewportAction::ReleasePIEMouseFocus,
			EEditorViewportAction::CycleMode,
			EEditorViewportAction::SetModeSelect,
			EEditorViewportAction::CycleGizmoMode,
			EEditorViewportAction::ToggleCoordinateSpace,
			EEditorViewportAction::SetModeTranslate,
			EEditorViewportAction::SetModeRotate,
			EEditorViewportAction::SetModeScale,
			EEditorViewportAction::FocusSelection,
			EEditorViewportAction::DeleteSelection,
			EEditorViewportAction::SelectAll,
			EEditorViewportAction::DuplicateSelection
		}
		: TArray<EEditorViewportAction>
		{
			EEditorViewportAction::CycleMode,
			EEditorViewportAction::SetModeSelect,
			EEditorViewportAction::CycleGizmoMode,
			EEditorViewportAction::ToggleCoordinateSpace,
			EEditorViewportAction::SetModeTranslate,
			EEditorViewportAction::SetModeRotate,
			EEditorViewportAction::SetModeScale,
			EEditorViewportAction::FocusSelection,
			EEditorViewportAction::Undo,
			EEditorViewportAction::Redo,
			EEditorViewportAction::DeleteSelection,
			EEditorViewportAction::SelectAll,
			EEditorViewportAction::DuplicateSelection,
			EEditorViewportAction::NewScene,
			EEditorViewportAction::LoadScene,
			EEditorViewportAction::SaveScene,
			EEditorViewportAction::SaveSceneAs
		});

	bool bTriggered = false;
	for (EEditorViewportAction Action : CommandActions)
	{
		bTriggered |= EditorViewportInputMapping::IsTriggered(Context, Action);
	}

	if (!bTriggered)
	{
		return false;
	}

	if (bPIEPossessed)
	{
		TickPIEShortCuts(Context);
	}
	else if (bInPIE
		&& (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::EndPIE)
			|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::TogglePIEPossessEject)
			|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::ReleasePIEMouseFocus)))
	{
		TickPIEShortCuts(Context);
	}
	else
	{
		TickEditorShortcuts(Context);
	}
	return true;
}

bool FEditorViewportClient::HandleGizmoInput(const FViewportInputContext& Context)
{
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
	{
		return false;
	}

	const bool bGizmoDragArmed = Gizmo && (Gizmo->IsPressedOnHandle() || Gizmo->IsHolding());
	if (IsPointerInViewportInputDeadZone(Context)
		|| !Gizmo
		|| !Gizmo->IsVisible()
		|| TransformMode == ETransformMode::Select
		|| (IsBoxSelectionChordActive(Context) && !bGizmoDragArmed))
	{
		return false;
	}

	if (Context.Frame.bLeftDragging && bGizmoDragArmed)
	{
		if (!bGizmoDragUndoCaptured && Editor)
		{
			Editor->CaptureUndoSnapshot("Transform Actors");
			bGizmoDragUndoCaptured = true;
		}
		const float LocalX = static_cast<float>(Context.MouseLocalPos.x);
		const float LocalY = static_cast<float>(Context.MouseLocalPos.y);
		InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseDragged, LocalX, LocalY);
		return true;
	}

	if (Context.WasPointerDragEnded(EPointerButton::Left) && Gizmo->IsHolding())
	{
		const float LocalX = static_cast<float>(Context.MouseLocalPos.x);
		const float LocalY = static_cast<float>(Context.MouseLocalPos.y);
		InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseDragEnded, LocalX, LocalY);
		bGizmoDragUndoCaptured = false;
		return true;
	}

	if (Context.WasReleased(VK_LBUTTON))
	{
		bGizmoDragUndoCaptured = false;
	}

	return false;
}

bool FEditorViewportClient::HandleSelectionInput(const FViewportInputContext& Context)
{
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
	{
		return false;
	}

	if (IsPointerInViewportInputDeadZone(Context))
	{
		return false;
	}

	const bool bLeftReleased =
		Context.WasPointerDragEnded(EPointerButton::Left) ||
		Context.WasReleased(VK_LBUTTON);
	const bool bLeftActive =
		Context.Frame.IsDown(VK_LBUTTON) ||
		Context.Frame.bLeftDragging ||
		Context.WasPointerDragStarted(EPointerButton::Left);
	const bool bLeftDragBeyondThreshold =
		Context.Frame.bLeftDragging ||
		Context.WasPointerDragStarted(EPointerButton::Left);
	const bool bSelectModePlainDrag =
		TransformMode == ETransformMode::Select
		&& Context.Frame.IsDown(VK_LBUTTON)
		&& !Context.Frame.IsAltDown()
		&& !Context.Frame.IsCtrlDown()
		&& !Context.Frame.IsShiftDown()
		&& bLeftDragBeyondThreshold;
	const bool bBoxSelectionChord = IsBoxSelectionChordActive(Context) || bSelectModePlainDrag;
	const auto GetClampedLocalPoint = [&](float InLocalX, float InLocalY) -> POINT
	{
		const float ClampedX = MathUtil::Clamp(
			InLocalX,
			0.0f,
			std::max(0.0f, WindowWidth - 1.0f));
		const float ClampedY = MathUtil::Clamp(
			InLocalY,
			0.0f,
			std::max(0.0f, WindowHeight - 1.0f));
		return POINT{ static_cast<LONG>(ClampedX), static_cast<LONG>(ClampedY) };
	};
	const auto GetCurrentLocalPoint = [&]() -> POINT
	{
		return GetClampedLocalPoint(
			static_cast<float>(Context.MouseLocalPos.x),
			static_cast<float>(Context.MouseLocalPos.y));
	};
	const auto GetDragStartLocalPoint = [&]() -> POINT
	{
		return GetClampedLocalPoint(
			static_cast<float>(Context.MouseLocalPos.x - Context.Frame.LeftDragVector.x),
			static_cast<float>(Context.MouseLocalPos.y - Context.Frame.LeftDragVector.y));
	};

	if (bBoxSelecting)
	{
		BoxSelectEnd = GetCurrentLocalPoint();
		if (bLeftReleased)
		{
			HandleBoxSelection();
			bBoxSelecting = false;
		}
		else if (!bLeftActive)
		{
			bBoxSelecting = false;
		}
		return true;
	}

	if (bBoxSelectionChord)
	{
		if (bLeftDragBeyondThreshold)
		{
			bBoxSelecting = true;
			BoxSelectStart = GetDragStartLocalPoint();
			BoxSelectEnd = GetCurrentLocalPoint();
		}
		return true;
	}

	return false;
}

bool FEditorViewportClient::HandleNavigationInput(const FViewportInputContext& Context)
{
	if (InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController
		&& Context.Frame.IsDown(VK_RBUTTON)
		&& !MathUtil::IsNearlyZero(Context.Frame.WheelNotches))
	{
		const float WheelScale = std::pow(1.15f, static_cast<float>(Context.Frame.WheelNotches));
		SetMoveSpeed(MathUtil::Clamp(GetMoveSpeed() * WheelScale, 0.1f, 5000.0f));
		return true;
	}

	if (HandleAltNavigationInput(Context))
	{
		return true;
	}
	if (HandleLeftNavigationInput(Context))
	{
		return true;
	}

	TickKeyboardInput(Context);
	TickPIEShortCuts(Context);
	TickMouseInput(Context);
	return true;
}

bool FEditorViewportClient::HandleLeftNavigationInput(const FViewportInputContext& Context)
{
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController || bControlLocked || !bHasCamera)
	{
		return false;
	}
	if (IsPointerInViewportInputDeadZone(Context) || Context.bImGuiCapturedMouse || Camera.IsOrthographic())
	{
		return false;
	}

	const bool bLeftNavigationChord =
		TransformMode != ETransformMode::Select
		&& !bBoxSelecting
		&& !IsBoxSelectionChordActive(Context)
		&& Context.Frame.IsDown(VK_LBUTTON)
		&& !Context.Frame.IsCtrlDown()
		&& !Context.Frame.IsAltDown()
		&& !Context.Frame.IsShiftDown();
	if (!bLeftNavigationChord)
	{
		return false;
	}

	if (Gizmo && (Gizmo->IsHolding() || Gizmo->IsPressedOnHandle() || Gizmo->IsHovered()))
	{
		return false;
	}

	const bool bLeftGesture =
		Context.Frame.bLeftDragging
		|| Context.WasPointerDragStarted(EPointerButton::Left)
		|| Context.bRelativeMouseMode;
	if (!bLeftGesture)
	{
		return false;
	}

	float DeltaX = static_cast<float>(Context.Frame.MouseDelta.x);
	float DeltaY = static_cast<float>(Context.Frame.MouseDelta.y);
	if (MathUtil::IsNearlyZero(DeltaX) && MathUtil::IsNearlyZero(DeltaY))
	{
		DeltaX = static_cast<float>(Context.MouseLocalDelta.x);
		DeltaY = static_cast<float>(Context.MouseLocalDelta.y);
	}
	if (MathUtil::IsNearlyZero(DeltaX) && MathUtil::IsNearlyZero(DeltaY))
	{
		return true;
	}

	const FVector Forward = Camera.GetForwardVector().GetSafeNormal();
	float Pitch = MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.0f, 1.0f)));
	float Yaw = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));

	const FEditorSettings& Settings = FEditorSettings::Get();
	const float RotationSpeed = Settings.CameraRotationSpeed / 400.0f;
	Yaw += DeltaX * RotationSpeed;
	const float PitchRad = MathUtil::DegreesToRadians(Pitch);
	const float YawRad = MathUtil::DegreesToRadians(Yaw);
	FVector NextForward(
		std::cos(PitchRad) * std::cos(YawRad),
		std::cos(PitchRad) * std::sin(YawRad),
		std::sin(PitchRad));
	NextForward = NextForward.GetSafeNormal();

	FVector Right = FVector::CrossProduct(FVector::UpVector, NextForward).GetSafeNormal();
	FQuat AppliedRotation = Camera.GetRotation();
	if (!Right.IsNearlyZero())
	{
		FVector Up = FVector::CrossProduct(NextForward, Right).GetSafeNormal();
		FMatrix RotMat = FMatrix::Identity;
		RotMat.SetAxes(NextForward, Right, Up);
		FQuat NextRotation(RotMat);
		NextRotation.Normalize();
		Camera.SetRotation(NextRotation);
		AppliedRotation = NextRotation;
	}

	const float ForwardScale = GetMoveSpeed() * Settings.CameraDollySpeedScale * 0.01f;
	const FVector NextLocation = Camera.GetLocation() + NextForward * (-DeltaY * ForwardScale);
	Camera.SetLocation(NextLocation);
	InputRouter.GetEditorWorldController().SetTargetLocation(NextLocation);
	InputRouter.GetEditorWorldController().SetTargetRotation(AppliedRotation);
	return true;
}

bool FEditorViewportClient::HandleAltNavigationInput(const FViewportInputContext& Context)
{
	using EEditorViewportAction = EditorViewportInputMapping::EEditorViewportAction;

	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController || bControlLocked || !bHasCamera)
	{
		return false;
	}

	const bool bAltOrbit = EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavOrbitAltLeftDown);
	const bool bAltDolly = EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavDollyAltRightDown);
	const bool bAltPan = EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavPanAltMiddleDown);
	if (!(bAltOrbit || bAltDolly || bAltPan))
	{
		return false;
	}

	const float DeltaX = static_cast<float>(Context.MouseLocalDelta.x);
	const float DeltaY = static_cast<float>(Context.MouseLocalDelta.y);
	if (MathUtil::IsNearlyZero(DeltaX) && MathUtil::IsNearlyZero(DeltaY))
	{
		return true;
	}

	if (bAltPan)
	{
		const FEditorSettings& Settings = FEditorSettings::Get();
		const float PanScale = Camera.IsOrthographic()
			? Camera.GetOrthoHeight() * 0.002f * Settings.CameraPanSpeedScale
			: GetMoveSpeed() * 0.002f * Settings.CameraPanSpeedScale;
		const FVector Right = Camera.GetEffectiveRight();
		const FVector Up = Camera.GetEffectiveUp();
		Camera.SetLocation(Camera.GetLocation() + Right * (DeltaX * PanScale) + Up * (-DeltaY * PanScale));
		SyncCameraTarget();
		return true;
	}

	if (bAltDolly)
	{
		if (Camera.IsOrthographic())
		{
			const float NextHeight = Camera.GetOrthoHeight() + DeltaY * Camera.GetOrthoHeight() * 0.003f;
			Camera.SetOrthoHeight(MathUtil::Clamp(NextHeight, 0.1f, 5000.0f));
		}
		else
		{
			const float DollyScale = GetMoveSpeed() * FEditorSettings::Get().CameraDollySpeedScale * 0.01f;
			Camera.SetLocation(Camera.GetLocation() + Camera.GetForwardVector().GetSafeNormal() * (-DeltaY * DollyScale));
			SyncCameraTarget();
		}
		return true;
	}

	if (bAltOrbit && !Camera.IsOrthographic())
	{
		FVector Pivot = Camera.GetLocation() + Camera.GetForwardVector().GetSafeNormal() * 10.0f;
		if (SelectionManager)
		{
			if (AActor* Primary = SelectionManager->GetPrimarySelection())
			{
				Pivot = Primary->GetActorLocation();
			}
		}

		const FVector Offset = Camera.GetLocation() - Pivot;
		float Radius = Offset.Size();
		if (Radius < 1.0f)
		{
			Radius = 10.0f;
		}

		float Azimuth = std::atan2(Offset.Y, Offset.X);
		float Elevation = std::asin(MathUtil::Clamp(Offset.Z / Radius, -1.0f, 1.0f));
		constexpr float OrbitSpeed = 0.0035f;
		Azimuth += DeltaX * OrbitSpeed;
		Elevation = MathUtil::Clamp(Elevation + DeltaY * OrbitSpeed, -1.4f, 1.4f);

		const float CosElevation = std::cos(Elevation);
		const FVector NextOffset(
			Radius * CosElevation * std::cos(Azimuth),
			Radius * CosElevation * std::sin(Azimuth),
			Radius * std::sin(Elevation));

		Camera.SetLocation(Pivot + NextOffset);
		Camera.SetLookAt(Pivot);
		SyncCameraTarget();
		return true;
	}

	return true;
}

void FEditorViewportClient::TickMouseInput(float VX, float VY)
{
	if (bControlLocked) return;
	if (InputRouter.GetActiveController() == EActiveEditorController::PIEController && bPIEMouseFocusReleased)
	{
		return;
	}
	if (InputRouter.GetActiveController() == EActiveEditorController::PIEController && !IsRuntimeGameInputCaptured())
	{
		return;
	}
	const InputSystem& IS = InputSystem::Get();

	POINT MP = IS.GetMousePos();
	if (Window) MP = Window->ScreenToClientPoint(MP);
	const float LocalX = static_cast<float>(MP.x) - VX;
	const float LocalY = static_cast<float>(MP.y) - VY;
	const float DX     = static_cast<float>(IS.MouseDeltaX());
	const float DY     = static_cast<float>(IS.MouseDeltaY());

	if (IsPointerInViewportInputDeadZone(LocalY))
	{
		return;
	}

	if (IS.MouseMoved())
	{
		InputRouter.RouteMouseInput(EMouseInputType::E_MouseMoved, DX, DY);
		InputRouter.RouteMouseInput(EMouseInputType::E_MouseMovedAbsolute, LocalX, LocalY);
	}

	if (IS.GetKeyDown(VK_RBUTTON))
		InputRouter.RouteMouseInput(EMouseInputType::E_RightMouseClicked, DX, DY);
	if (IS.GetRightDragging())
		InputRouter.RouteMouseInput(EMouseInputType::E_RightMouseDragged, DX, DY);
	if (IS.GetMiddleDragging())
		InputRouter.RouteMouseInput(EMouseInputType::E_MiddleMouseDragged, DX, DY);
	if (!MathUtil::IsNearlyZero(IS.GetScrollNotches()))
		InputRouter.RouteMouseInput(EMouseInputType::E_MouseWheelScrolled, IS.GetScrollNotches(), 0.f);

	if (IS.GetKeyDown(VK_LBUTTON))
		InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseClicked, LocalX, LocalY);
	if (IS.GetLeftDragging())
		InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseDragged, LocalX, LocalY);
	if (IS.GetLeftDragEnd())
		InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseDragEnded, LocalX, LocalY);
	if (IS.GetKeyUp(VK_LBUTTON) && !IS.GetLeftDragEnd())
		InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseButtonUp, LocalX, LocalY);
}

void FEditorViewportClient::TickMouseInput(const FViewportInputContext& Context)
{
	if (bControlLocked) return;
	if (InputRouter.GetActiveController() == EActiveEditorController::PIEController && bPIEMouseFocusReleased)
	{
		return;
	}
	if (InputRouter.GetActiveController() == EActiveEditorController::PIEController && !IsRuntimeGameInputCaptured())
	{
		return;
	}

	const float LocalX = static_cast<float>(Context.MouseLocalPos.x);
	const float LocalY = static_cast<float>(Context.MouseLocalPos.y);
	const float DX = static_cast<float>(Context.MouseLocalDelta.x);
	const float DY = static_cast<float>(Context.MouseLocalDelta.y);

	if (IsPointerInViewportInputDeadZone(Context))
	{
		return;
	}

	if (DX != 0.0f || DY != 0.0f)
	{
		InputRouter.RouteMouseInput(EMouseInputType::E_MouseMoved, DX, DY);
		InputRouter.RouteMouseInput(EMouseInputType::E_MouseMovedAbsolute, LocalX, LocalY);
	}

	if (Context.WasPressed(VK_RBUTTON))
		InputRouter.RouteMouseInput(EMouseInputType::E_RightMouseClicked, DX, DY);
	if (Context.Frame.bRightDragging || (Context.bRelativeMouseMode && Context.Frame.IsDown(VK_RBUTTON)))
		InputRouter.RouteMouseInput(EMouseInputType::E_RightMouseDragged, DX, DY);
	if (Context.Frame.bMiddleDragging || (Context.bRelativeMouseMode && Context.Frame.IsDown(VK_MBUTTON)))
		InputRouter.RouteMouseInput(EMouseInputType::E_MiddleMouseDragged, DX, DY);
	if (!MathUtil::IsNearlyZero(Context.Frame.WheelNotches))
		InputRouter.RouteMouseInput(EMouseInputType::E_MouseWheelScrolled, Context.Frame.WheelNotches, 0.f);

	if (!IsBoxSelectionChordActive(Context))
	{
		if (Context.WasPressed(VK_LBUTTON))
			InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseClicked, LocalX, LocalY);
		if (Context.Frame.bLeftDragging)
			InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseDragged, LocalX, LocalY);
		if (Context.WasPointerDragEnded(EPointerButton::Left))
			InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseDragEnded, LocalX, LocalY);
		if (Context.WasReleased(VK_LBUTTON) && !Context.WasPointerDragEnded(EPointerButton::Left))
			InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseButtonUp, LocalX, LocalY);
	}
}

bool FEditorViewportClient::IsBoxSelectionChordActive(const FViewportInputContext& Context) const
{
	if (IsPointerInViewportInputDeadZone(Context))
	{
		return false;
	}

	if (Camera.IsOrthographic()
		&& Context.Frame.IsDown(VK_LBUTTON)
		&& Context.Frame.bLeftDragging
		&& !Context.Frame.IsAltDown()
		&& !Context.Frame.IsCtrlDown())
	{
		return true;
	}

	return Context.Frame.IsCtrlDown()
		&& Context.Frame.IsAltDown()
		&& Context.Frame.IsDown(VK_LBUTTON);
}

bool FEditorViewportClient::IsPointerInViewportInputDeadZone(float LocalY) const
{
	return ViewportInputDeadZoneTop > 0.0f
		&& LocalY >= 0.0f
		&& LocalY < ViewportInputDeadZoneTop;
}

bool FEditorViewportClient::IsPointerInViewportInputDeadZone(const FViewportInputContext& Context) const
{
	return IsPointerInViewportInputDeadZone(static_cast<float>(Context.MouseLocalPos.y));
}

void FEditorViewportClient::SetTransformMode(ETransformMode InMode)
{
	TransformMode = InMode;
	ApplyTransformModeToGizmo();
}

void FEditorViewportClient::CycleTransformMode()
{
	switch (TransformMode)
	{
	case ETransformMode::Select:
		SetTransformMode(ETransformMode::Translate);
		break;
	case ETransformMode::Translate:
		SetTransformMode(ETransformMode::Rotate);
		break;
	case ETransformMode::Rotate:
		SetTransformMode(ETransformMode::Scale);
		break;
	case ETransformMode::Scale:
	default:
		SetTransformMode(ETransformMode::Select);
		break;
	}
}

void FEditorViewportClient::CycleGizmoTransformMode()
{
	switch (TransformMode)
	{
	case ETransformMode::Translate:
		SetTransformMode(ETransformMode::Rotate);
		break;
	case ETransformMode::Rotate:
		SetTransformMode(ETransformMode::Scale);
		break;
	case ETransformMode::Select:
	case ETransformMode::Scale:
	default:
		SetTransformMode(ETransformMode::Translate);
		break;
	}
}

void FEditorViewportClient::ApplyTransformModeToGizmo()
{
	if (!Gizmo)
	{
		return;
	}

	if (TransformMode == ETransformMode::Select)
	{
		Gizmo->DragEnd();
		Gizmo->SetVisibility(false);
		return;
	}

	Gizmo->SetVisibility(Gizmo->HasTarget());

	switch (TransformMode)
	{
	case ETransformMode::Translate:
		Gizmo->SetTranslateMode();
		break;
	case ETransformMode::Rotate:
		Gizmo->SetRotateMode();
		break;
	case ETransformMode::Scale:
		Gizmo->SetScaleMode();
		break;
	case ETransformMode::Select:
	default:
		break;
	}
}

// ── Interaction (gizmo scaling + box selection) ───────────────────────────────

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;

	if (!bHasCamera)
		return;

	if (World && World->GetWorldType() == EWorldType::PIE)
		return;

	if (Gizmo)
	{
		ApplyTransformModeToGizmo();

		// Gizmo screen-space scaling must happen every frame while a transform mode is active.
		if (TransformMode != ETransformMode::Select && Gizmo->IsVisible())
		{
			if (Camera.IsOrthographic())
				Gizmo->ApplyScreenSpaceScalingOrtho(Camera.GetOrthoHeight());
			else
				Gizmo->ApplyScreenSpaceScaling(Camera.GetLocation());
		}
	}

	if (!World || !SelectionManager)
		return;

	if (bRoutedInputProcessedThisFrame)
	{
		return;
	}

	// ── Box selection (Ctrl+Alt+LMB drag) ────────────────────────────────────
	POINT MousePoint = InputSystem::Get().GetMousePos();

	if (bBoxSelecting)
	{
		const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
		if (!GuiState.IsInViewportHost(MousePoint.x, MousePoint.y))
		{
			bBoxSelecting = false;
			return;
		}
	}

	if (Window) MousePoint = Window->ScreenToClientPoint(MousePoint);
    const float VX = State ? static_cast<float>(Viewport->GetRect().X) : 0.f;
    const float VY = State ? static_cast<float>(Viewport->GetRect().Y) : 0.f;
	const float LocalX = static_cast<float>(MousePoint.x) - VX;
	const float LocalY = static_cast<float>(MousePoint.y) - VY;

	if (bBoxSelecting && (LocalX < 0.f || LocalY < 0.f || LocalX > WindowWidth || LocalY > WindowHeight))
	{
		bBoxSelecting = false;
		return;
	}

	const InputSystem& IS        = InputSystem::Get();
	const bool         bCtrlDown = IS.GetKey(VK_CONTROL);
	const bool         bAltDown  = IS.GetKey(VK_MENU);

	const bool bLeftDragBeyondThreshold = IS.GetLeftDragging();
	const bool bBoxSelectionChord =
		bLeftDragBeyondThreshold
		&& ((bCtrlDown && bAltDown)
			|| (Camera.IsOrthographic() && !bCtrlDown && !bAltDown));

	if (!bBoxSelecting && bBoxSelectionChord)
	{
		const POINT DragVector = IS.GetLeftDragVector();
		bBoxSelecting  = true;
		BoxSelectStart = POINT{
			static_cast<LONG>(MathUtil::Clamp(LocalX - static_cast<float>(DragVector.x), 0.0f, std::max(0.0f, WindowWidth - 1.0f))),
			static_cast<LONG>(MathUtil::Clamp(LocalY - static_cast<float>(DragVector.y), 0.0f, std::max(0.0f, WindowHeight - 1.0f)))
		};
		BoxSelectEnd = POINT{
			static_cast<LONG>(MathUtil::Clamp(LocalX, 0.0f, std::max(0.0f, WindowWidth - 1.0f))),
			static_cast<LONG>(MathUtil::Clamp(LocalY, 0.0f, std::max(0.0f, WindowHeight - 1.0f)))
		};
		return;
	}

	if (bBoxSelecting)
	{
		if (IS.GetLeftDragging())
			BoxSelectEnd = POINT{ static_cast<LONG>(LocalX), static_cast<LONG>(LocalY) };
		else if (IS.GetLeftDragEnd())
		{
			HandleBoxSelection();
			bBoxSelecting = false;
		}
		else if (IS.GetKeyUp(VK_LBUTTON))
			bBoxSelecting = false;
	}
}

void FEditorViewportClient::LockCursorToViewport()
{
	// State->Rect is in client space; LockMouse needs screen space.
    POINT Origin = { Viewport->GetRect().X, Viewport->GetRect().Y };
	if (Window)
		::ClientToScreen(Window->GetHWND(), &Origin);
	InputSystem::Get().LockMouse(true, (float)Origin.x, (float)Origin.y,
                                 (float)Viewport->GetRect().Width, (float)Viewport->GetRect().Height);
}

bool FEditorViewportClient::TryProjectWorldToViewport(const FVector& WorldPos, float& OutViewportX, float& OutViewportY, float& OutDepth) const
{
	const FVector4 Clip = FMatrix::Identity.TransformVector4(FVector4(WorldPos, 1.0f), Camera.GetViewProjectionMatrix());
	if (MathUtil::IsNearlyZero(Clip.W))
		return false;

	const float InvW = 1.0f / Clip.W;
	const float NdcX = Clip.X * InvW;
	const float NdcY = Clip.Y * InvW;
	const float NdcZ = Clip.Z * InvW;
	if (NdcX < -1.0f || NdcX > 1.0f || NdcY < -1.0f || NdcY > 1.0f)
		return false;

	OutViewportX = (NdcX * 0.5f + 0.5f) * WindowWidth;
	OutViewportY = (1.0f - (NdcY * 0.5f + 0.5f)) * WindowHeight;
	OutDepth     = NdcZ;
	return true;
}

void FEditorViewportClient::HandleBoxSelection()
{
	if (!SelectionManager || !World)
		return;

	const int32 MinX   = std::min(BoxSelectStart.x, BoxSelectEnd.x);
	const int32 MinY   = std::min(BoxSelectStart.y, BoxSelectEnd.y);
	const int32 MaxX   = std::max(BoxSelectStart.x, BoxSelectEnd.x);
	const int32 MaxY   = std::max(BoxSelectStart.y, BoxSelectEnd.y);
	const int32 Width  = MaxX - MinX;
	const int32 Height = MaxY - MinY;

	if (Width < 2 || Height < 2)
		return;

	if (!InputSystem::Get().GetKey(VK_SHIFT))
		SelectionManager->ClearSelection();

	TArray<UPrimitiveComponent*> CandidatePrimitives;
	World->GetSpatialIndex().FrustumQueryPrimitives(Camera.GetFrustum(), CandidatePrimitives, FrustumQueryScratch);

	std::unordered_set<AActor*> SeenActors;
	SeenActors.reserve(CandidatePrimitives.size());

	for (UPrimitiveComponent* Primitive : CandidatePrimitives)
	{
		AActor* Actor = (Primitive != nullptr) ? Primitive->GetOwner() : nullptr;
		if (!Actor || !Actor->GetRootComponent())
			continue;

		if (!SeenActors.insert(Actor).second)
			continue;

		float ViewportX = 0.f, ViewportY = 0.f, Depth = 0.f;
		if (!TryProjectWorldToViewport(Actor->GetActorLocation(), ViewportX, ViewportY, Depth))
			continue;

		if (Depth < 0.f || Depth > 1.f)
			continue;

		const int32 Px = static_cast<int32>(ViewportX);
		const int32 Py = static_cast<int32>(ViewportY);
		if (Px >= MinX && Px <= MaxX && Py >= MinY && Py <= MaxY)
			SelectionManager->AddSelect(Actor);
	}
}

void FEditorViewportClient::FocusPrimarySelection()
{
	if (!SelectionManager || !bHasCamera)
		return;

	AActor* Primary = SelectionManager->GetPrimarySelection();
	if (!Primary)
		return;

	const FVector Target = Primary->GetActorLocation();

	if (Camera.IsOrthographic())
	{
		const FVector Forward  = Camera.GetEffectiveForward().GetSafeNormal();
		float         Distance = FVector::DotProduct(Camera.GetLocation() - Target, Forward);
		if (MathUtil::IsNearlyZero(Distance))
			Distance = 1000.f;
		Camera.SetLocation(Target + Forward * Distance);
	}
	else
	{
		const FVector Forward = Camera.GetForwardVector().GetSafeNormal();
		Camera.SetLocation(Target - Forward * 5.f);
		Camera.SetLookAt(Target);
	}

	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
}

void FEditorViewportClient::DeleteSelectedActors()
{
	if (!SelectionManager)
		return;

	const TArray<AActor*> SelectedActors = SelectionManager->GetSelectedActors();
	if (SelectedActors.empty())
		return;

	if (Editor)
	{
		Editor->DeleteActors(SelectedActors);
	}
}

void FEditorViewportClient::SelectAllActors()
{
	if (!SelectionManager || !World)
		return;

	SelectionManager->ClearSelection();
	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		if (AActor* Actor = *Iter)
			SelectionManager->AddSelect(Actor);
	}
}

void FEditorViewportClient::DuplicateSelection()
{
	if (!SelectionManager || !World)
		return;

	const TArray<AActor*> SelectedActors = SelectionManager->GetSelectedActors();
	if (SelectedActors.empty())
		return;

	if (Editor)
	{
		Editor->CaptureUndoSnapshot("Duplicate Actors");
	}

	TArray<AActor*> NewSelection;
	for (AActor* SourceActor : SelectedActors)
	{
		if (!SourceActor)
			continue;

		AActor* DuplicatedActor = Cast<AActor>(SourceActor->Duplicate());
		if (!DuplicatedActor)
			continue;

		DuplicatedActor->SetWorld(World);
		if (ULevel* Level = World->GetPersistentLevel())
		{
			Level->AddActor(DuplicatedActor);
		}
		DuplicatedActor->AddActorWorldOffset(FVector(0.25f, 0.25f, 0.0f));
		NewSelection.push_back(DuplicatedActor);
	}

	if (NewSelection.empty())
		return;

	SelectionManager->ClearSelection();
	for (AActor* Actor : NewSelection)
	{
		SelectionManager->AddSelect(Actor);
	}

	World->RebuildSpatialIndex();
}

void FEditorViewportClient::TogglePIEPossessEject()
{
	if (!IsPIEActive())
		return;

	if (InputRouter.GetActiveController() == EActiveEditorController::PIEController)
	{
		EnterPIEEditorControlMode();
	}
	else
	{
		EnterPIEPossessedMode();
	}
}

void FEditorViewportClient::ReleasePIEMouseFocus()
{
	if (!IsPIEPossessed())
		return;

	bPIEMouseFocusReleased = true;
	bControlLocked = false;
	InputSystem& IS = InputSystem::Get();
	IS.SetCursorVisibility(true);
	IS.LockMouse(false);
}

void FEditorViewportClient::ReacquirePIEMouseFocus()
{
	if (!IsPIEActive())
		return;

	EnterPIEPossessedMode();
}

bool FEditorViewportClient::TryReacquirePIEMouseFocusOnViewportClick(const FViewportInputContext& Context)
{
	if (!IsPIEPossessed()
		|| !bPIEMouseFocusReleased
		|| !Context.WasPressed(VK_LBUTTON)
		|| Context.bImGuiCapturedMouse
		|| IsPointerInViewportInputDeadZone(Context))
	{
		return false;
	}

	ReacquirePIEMouseFocus();
	return true;
}

void FEditorViewportClient::EnterPIEEditorControlMode()
{
	if (!IsPIEActive())
	{
		return;
	}

	InputRouter.GetEditorWorldController().SetWorld(World);
	InputRouter.GetEditorWorldController().SetCamera(&Camera);
	InputRouter.GetEditorWorldController().SetTargetLocation(InputRouter.GetPIEController().GetTargetLocation());
	InputRouter.GetEditorWorldController().SetTargetRotation(Camera.GetRotation());
	InputRouter.SetActiveController(EActiveEditorController::EditorWorldController);
	if (World)
	{
		World->SetActiveCamera(&Camera);
	}

	bControlLocked = false;
	bPIEMouseFocusReleased = false;
	InputSystem& IS = InputSystem::Get();
	IS.SetCursorVisibility(true);
	IS.LockMouse(false);
}

void FEditorViewportClient::EnterPIEPossessedMode()
{
	if (!IsPIEActive())
	{
		return;
	}

	InputRouter.GetPIEController().SetCamera(&Camera);
	InputRouter.GetPIEController().SetTargetLocation(InputRouter.GetEditorWorldController().GetTargetLocation());
	InputRouter.SetActiveController(EActiveEditorController::PIEController);
	if (GEngine)
	{
		GEngine->SetRuntimeInputMode(ERuntimeInputMode::GameOnly);
	}
	if (World)
	{
		APlayerController* PlayerController = InputRouter.GetPIEController().GetPlayerController();
		World->SetActiveCamera(PlayerController ? PlayerController->GetRuntimeCamera() : &Camera);
	}

	bControlLocked = false;
	bPIEMouseFocusReleased = false;
	InputSystem& IS = InputSystem::Get();
	IS.SetCursorVisibility(false);
	LockCursorToViewport();
}

void FEditorViewportClient::SaveCameraSnapshot()
{
	FViewportCamera CameraStae;
}

void FEditorViewportClient::RestoreCameraSnapshot()
{

}
