#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Slate/SlateApplication.h"
#include "EditorEngine.h"

#include "GameFramework/World.h"
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
#include <unordered_set>

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow, UEditorEngine* InEditor)
{
	FViewportClient::Initialize(InWindow);
	Editor = InEditor;
	InputRouter.SetActiveController(EActiveEditorController::EditorWorldController);
}

void FEditorViewportClient::SetWorld(UWorld* InWorld)
{
	World = InWorld;
	InputRouter.GetEditorWorldController().SetWorld(InWorld);
}

void FEditorViewportClient::StartPIE(UWorld* InWorld)
{
	World = InWorld;
    InputRouter.GetPIEController().SetCamera(&Camera); // re-sync Yaw/Pitch
    InputRouter.GetPIEController().SetTargetLocation(InputRouter.GetEditorWorldController().GetTargetLocation());
	InputRouter.SetActiveController(EActiveEditorController::PIEController);
}

void FEditorViewportClient::EndPIE(UWorld* InWorld)
{
	World = InWorld;
    InputRouter.GetEditorWorldController().SetTargetLocation(InputRouter.GetPIEController().GetTargetLocation());
	InputRouter.GetEditorWorldController().SetWorld(InWorld);
	InputRouter.SetActiveController(EActiveEditorController::EditorWorldController);
	InputRouter.GetEditorWorldController().ResetTargetLocation();
	ClearEndPIECallback();
	InputSystem::Get().LockMouse(false);
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
	InputRouter.GetEditorWorldController().ResetTargetLocation();
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
	InputRouter.GetEditorWorldController().ResetTargetLocation();
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	FViewportClient::SetViewportSize(InWidth, InHeight);

	if (bHasCamera)
		Camera.OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	if (State && !State->bHovered)
		return;

	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
}

void FEditorViewportClient::BuildSceneView(FSceneView& OutView) const
{
	if (!bHasCamera) return;

	OutView.ViewMatrix           = Camera.GetViewMatrix();
	OutView.ProjectionMatrix     = Camera.GetProjectionMatrix();
	OutView.ViewProjectionMatrix = OutView.ViewMatrix * OutView.ProjectionMatrix;

	OutView.CameraPosition = Camera.GetLocation();
	OutView.CameraForward  = Camera.GetForwardVector();
	OutView.CameraRight    = Camera.GetRightVector();
	OutView.CameraUp       = Camera.GetUpVector();

	OutView.bOrthographic = Camera.IsOrthographic();

	if (State)
	{
		OutView.ViewRect = State->Rect;
		OutView.ViewMode = State->ViewMode;
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
	InputRouter.GetEditorWorldController().ResetTargetLocation();
}

bool FEditorViewportClient::IsActiveOperation() const
{
	const InputSystem& IS = InputSystem::Get();
	return IS.GetRightDragging() || IS.GetMiddleDragging();
}

// ── Input tick sub-steps ──────────────────────────────────────────────────────

void FEditorViewportClient::TickInput(float DeltaTime)
{
	if (!bHasCamera)
		return;

	const float VX = State ? static_cast<float>(State->Rect.X) : 0.f;
	const float VY = State ? static_cast<float>(State->Rect.Y) : 0.f;

	TickCursorCapture();
	InputRouter.SetViewportDim(VX, VY, WindowWidth, WindowHeight);
	InputRouter.Tick(DeltaTime);
	TickKeyboardInput();
	TickEditorShortcuts();
	TickMouseInput(VX, VY);
}

void FEditorViewportClient::TickCursorCapture()
{
	// Only the EditorWorldController drives drag-based cursor capture.
	// PIE cursor behaviour is handled by the PIEController / EndPIE.
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
		return;

	const InputSystem& IS = InputSystem::Get();
	const bool bDragBegin = IS.GetKeyDown(VK_RBUTTON) || IS.GetKeyDown(VK_MBUTTON);
	const bool bDragEnd   = IS.GetKeyUp(VK_RBUTTON)   || IS.GetKeyUp(VK_MBUTTON);

	if (bDragBegin)
	{
		InputSystem::Get().SetCursorVisibility(false);
		LockCursorToViewport();
	}
	else if (bDragEnd)
	{
		InputSystem::Get().SetCursorVisibility(true);
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

	const InputSystem& IS = InputSystem::Get();
	for (int VK : WatchKeys)
	{
		if (IS.GetKeyDown(VK)) InputRouter.RouteKeyboardInput(EKeyInputType::KeyPressed,  VK);
		if (IS.GetKey(VK))     InputRouter.RouteKeyboardInput(EKeyInputType::KeyDown,     VK);
		if (IS.GetKeyUp(VK))   InputRouter.RouteKeyboardInput(EKeyInputType::KeyReleased, VK);
	}
}

void FEditorViewportClient::TickEditorShortcuts()
{
	// These shortcuts only apply in the editor world, not during PIE.
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
		return;

	const InputSystem& IS        = InputSystem::Get();
	const bool         bCtrlDown = IS.GetKey(VK_CONTROL);
	const bool         bAltDown  = IS.GetKey(VK_MENU);

	if (IS.GetKeyUp('X') && Gizmo)
		Gizmo->SetWorldSpace(!Gizmo->IsWorldSpace());

	if (IS.GetKeyUp(VK_DELETE))
		DeleteSelectedActors();

	if (IS.GetKeyDown('A') && bCtrlDown && !bAltDown)
		SelectAllActors();
}

void FEditorViewportClient::TickMouseInput(float VX, float VY)
{
	const InputSystem& IS = InputSystem::Get();

	POINT MP = IS.GetMousePos();
	if (Window) MP = Window->ScreenToClientPoint(MP);
	const float LocalX = static_cast<float>(MP.x) - VX;
	const float LocalY = static_cast<float>(MP.y) - VY;
	const float DX     = static_cast<float>(IS.MouseDeltaX());
	const float DY     = static_cast<float>(IS.MouseDeltaY());

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

// ── Interaction (gizmo scaling + box selection) ───────────────────────────────

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;

	if (!bHasCamera || !Gizmo)
		return;

	if (World && World->GetWorldType() == EWorldType::PIE)
		return;

	// Gizmo screen-space scaling must happen every frame.
	if (Camera.IsOrthographic())
		Gizmo->ApplyScreenSpaceScalingOrtho(Camera.GetOrthoHeight());
	else
		Gizmo->ApplyScreenSpaceScaling(Camera.GetLocation());

	if (!World || !SelectionManager)
		return;

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
	const float VX     = State ? static_cast<float>(State->Rect.X) : 0.f;
	const float VY     = State ? static_cast<float>(State->Rect.Y) : 0.f;
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

	if (IS.GetKeyDown(VK_LBUTTON) && bCtrlDown && bAltDown)
	{
		bBoxSelecting  = true;
		BoxSelectStart = POINT{ static_cast<LONG>(LocalX), static_cast<LONG>(LocalY) };
		BoxSelectEnd   = BoxSelectStart;
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
	POINT Origin = { State->Rect.X, State->Rect.Y };
	if (Window)
		::ClientToScreen(Window->GetHWND(), &Origin);
	InputSystem::Get().LockMouse(true, (float)Origin.x, (float)Origin.y,
	                             (float)State->Rect.Width, (float)State->Rect.Height);
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

	InputRouter.GetEditorWorldController().ResetTargetLocation();
}

void FEditorViewportClient::DeleteSelectedActors()
{
	if (!SelectionManager)
		return;

	const TArray<AActor*> SelectedActors = SelectionManager->GetSelectedActors();
	for (AActor* Actor : SelectedActors)
	{
		if (!Actor)
			continue;
		if (UWorld* ActorWorld = Actor->GetWorld())
			ActorWorld->DestroyActor(Actor);
	}
	SelectionManager->ClearSelection();
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
