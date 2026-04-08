#include "Viewport/Services/EditorViewportInputService.h"

#include "EditorEngine.h"
#include "Viewport/EditorViewportRegistry.h"
#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Core/Engine.h"
#include "Debug/EngineLog.h"
#include "Gizmo/Gizmo.h"
#include "imgui.h"
#include "Input/InputManager.h"
#include "Picking/Picker.h"
#include "Level/Level.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"

void FEditorViewportInputService::TickCameraNavigation(
	FEngine* Engine,
	FEditorEngine* EditorEngine,
	FEditorViewportRegistry& ViewportRegistry,
	const FGizmo& Gizmo)
{
	if (!Engine || !EditorEngine)
	{
		return;
	}

	FSlateApplication* Slate = EditorEngine->GetSlateApplication();
	if (!Slate || Slate->GetFocusedViewportId() == INVALID_VIEWPORT_ID)
	{
		return;
	}

	if (ImGui::GetCurrentContext())
	{
		const ImGuiIO& IO = ImGui::GetIO();
		if (IO.WantCaptureKeyboard || IO.WantCaptureMouse)
		{
			return;
		}
	}

	FInputManager* Input = Engine->GetInputManager();
	if (!Input || !Input->IsMouseButtonDown(FInputManager::MOUSE_RIGHT) || Gizmo.IsDragging())
	{
		return;
	}

	FViewportEntry* FocusedEntry = ViewportRegistry.FindEntryByViewportID(Slate->GetFocusedViewportId());
	if (!FocusedEntry || !FocusedEntry->bActive)
	{
		return;
	}

	const float DeltaX = Input->GetMouseDeltaX();
	const float DeltaY = Input->GetMouseDeltaY();

	if (FocusedEntry->LocalState.ProjectionType == EViewportType::Perspective)
	{
		float Sensitivity = 0.2f;
		if (FCamera* Cam = Engine->GetScene()->GetCamera())
		{
			Sensitivity = Cam->GetMouseSensitivity();
		}

		FocusedEntry->LocalState.Rotation.Yaw += DeltaX * Sensitivity;
		FocusedEntry->LocalState.Rotation.Pitch -= DeltaY * Sensitivity;
		if (FocusedEntry->LocalState.Rotation.Pitch > 89.0f)
		{
			FocusedEntry->LocalState.Rotation.Pitch = 89.0f;
		}
		if (FocusedEntry->LocalState.Rotation.Pitch < -89.0f)
		{
			FocusedEntry->LocalState.Rotation.Pitch = -89.0f;
		}
		return;
	}

	FVector ViewFwd;
	FVector ViewUp;
	switch (FocusedEntry->LocalState.ProjectionType)
	{
	case EViewportType::OrthoTop:
		ViewFwd = FVector(0, 0, -1);
		ViewUp = FVector(1, 0, 0);
		break;

	case EViewportType::OrthoBottom:
		ViewFwd = FVector(0, 0, 1);
		ViewUp = FVector(1, 0, 0);
		break;

	case EViewportType::OrthoLeft:
		ViewFwd = FVector(0, 1, 0);
		ViewUp = FVector(0, 0, 1);
		break;

	case EViewportType::OrthoRight:
		ViewFwd = FVector(0, -1, 0);
		ViewUp = FVector(0, 0, 1);
		break;

	case EViewportType::OrthoFront:
		ViewFwd = FVector(-1, 0, 0);
		ViewUp = FVector(0, 0, 1);
		break;

	case EViewportType::OrthoBack:
		ViewFwd = FVector(1, 0, 0);
		ViewUp = FVector(0, 0, 1);
		break;

	default:
		return;
	}

	const FVector ViewRight = FVector::CrossProduct(ViewUp, ViewFwd).GetSafeNormal();
	const int32 H = FocusedEntry->Viewport->GetRect().Height;
	if (H <= 0) return;
	float WorldPerPixel = (2.0f * FocusedEntry->LocalState.OrthoZoom) / static_cast<float>(H);
	FocusedEntry->LocalState.OrthoTarget -= ViewRight * DeltaX * WorldPerPixel;
	FocusedEntry->LocalState.OrthoTarget += ViewUp * DeltaY * WorldPerPixel;
}

void FEditorViewportInputService::HandleMessage(
	FEngine* Engine,
	FEditorEngine* EditorEngine,
	HWND Hwnd,
	UINT Msg,
	WPARAM WParam,
	LPARAM LParam,
	FEditorViewportRegistry& ViewportRegistry,
	FPicker& Picker,
	FGizmo& Gizmo,
	const std::function<void()>& OnSelectionChanged)
{
	(void)Hwnd;

	if (!Engine || !EditorEngine)
	{
		return;
	}

	FSlateApplication* Slate = EditorEngine->GetSlateApplication();
	if (!Slate)
	{
		return;
	}

	const int32 MouseX = static_cast<int32>(static_cast<short>(LOWORD(LParam)));
	const int32 MouseY = static_cast<int32>(static_cast<short>(HIWORD(LParam)));

	switch (Msg)
	{
	case WM_LBUTTONDOWN:
		Slate->ProcessMouseDown(MouseX, MouseY);
		break;
	case WM_LBUTTONDBLCLK:
		Slate->ProcessMouseDoubleClick(MouseX, MouseY);
		return;
	case WM_RBUTTONDOWN:
		Slate->ProcessMouseDown(MouseX, MouseY);
		break;
	case WM_MOUSEMOVE:
		Slate->ProcessMouseMove(MouseX, MouseY);
		break;
	case WM_LBUTTONUP:
		Slate->ProcessMouseUp(MouseX, MouseY);
		break;
	default:
		break;
	}

	if (Msg == WM_MOUSEWHEEL)
	{
		if (!ImGui::GetCurrentContext() || !ImGui::GetIO().WantCaptureMouse)
		{
			FViewportEntry* FocusedEntry = ViewportRegistry.FindEntryByViewportID(Slate->GetFocusedViewportId());
			if (FocusedEntry && FocusedEntry->bActive && FocusedEntry->LocalState.ProjectionType != EViewportType::Perspective)
			{
				const float WheelDelta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(WParam)) / WHEEL_DELTA;
				FocusedEntry->LocalState.OrthoZoom *= (1.0f - WheelDelta * 0.1f);
				if (FocusedEntry->LocalState.OrthoZoom < 1.0f)
				{
					FocusedEntry->LocalState.OrthoZoom = 1.0f;
				}
				if (FocusedEntry->LocalState.OrthoZoom > 10000.0f)
				{
					FocusedEntry->LocalState.OrthoZoom = 10000.0f;
				}
			}
		}
		return;
	}

	if (Slate->IsDraggingSplitter())
	{
		return;
	}

	if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)
	{
		return;
	}

	ULevel* Scene = Engine->GetScene();
	AActor* SelectedActor = EditorEngine->GetSelectedActor();
	if (!Scene)
	{
		return;
	}

	const bool bRightMouseDown = Engine->GetInputManager() &&
		Engine->GetInputManager()->IsMouseButtonDown(FInputManager::MOUSE_RIGHT);
	FViewportEntry* Entry = ViewportRegistry.FindEntryByViewportID(Slate->GetFocusedViewportId());

	switch (Msg)
	{
	case WM_KEYDOWN:
	{
		if (Slate->GetFocusedViewportId() == INVALID_VIEWPORT_ID || bRightMouseDown)
		{
			return;
		}

		switch (WParam)
		{
		case 'W':
			Gizmo.SetMode(EGizmoMode::Location);
			return;
		case 'E':
			Gizmo.SetMode(EGizmoMode::Rotation);
			return;
		case 'R':
			Gizmo.SetMode(EGizmoMode::Scale);
			return;
		case 'L':
			Gizmo.ToggleCoordinateSpace();
			UE_LOG("Gizmo Space: %s", Gizmo.GetCoordinateSpace() == EGizmoCoordinateSpace::Local ? "Local" : "World");
			return;
		case VK_SPACE:
			Gizmo.CycleMode();
			return;
		default:
			return;
		}
	}

	case WM_LBUTTONDOWN:
	{
		FViewport* Viewport = ViewportRegistry.GetViewportById(Slate->GetFocusedViewportId());
		if (!Viewport)
		{
			return;
		}

		const FRect& Rect = Viewport->GetRect();
		ScreenWidth = Rect.Width;
		ScreenHeight = Rect.Height;
		ScreenMouseX = MouseX - Rect.X;
		ScreenMouseY = MouseY - Rect.Y;

		if (SelectedActor && Gizmo.BeginDrag(SelectedActor, Entry, Picker, ScreenMouseX, ScreenMouseY))
		{
			return;
		}

		AActor* PickedActor = Picker.PickActor(Scene, Entry, ScreenMouseX, ScreenMouseY);
		EditorEngine->SetSelectedActor(PickedActor);
		if (OnSelectionChanged)
		{
			OnSelectionChanged();
		}
		return;
	}

	case WM_MOUSEMOVE:
	{
		FViewport* Viewport = ViewportRegistry.GetViewportById(Slate->GetHoveredViewportId());
		if (!Viewport)
		{
			Gizmo.ClearHover();
			return;
		}

		FViewportEntry* HoveredEntry = ViewportRegistry.FindEntryByViewportID(Slate->GetHoveredViewportId());
		const FRect& Rect = Viewport->GetRect();
		ScreenWidth = Rect.Width;
		ScreenHeight = Rect.Height;
		ScreenMouseX = MouseX - Rect.X;
		ScreenMouseY = MouseY - Rect.Y;

		if (!Gizmo.IsDragging())
		{
			Gizmo.UpdateHover(SelectedActor, HoveredEntry, Picker, ScreenMouseX, ScreenMouseY);
			return;
		}

		if (Gizmo.UpdateDrag(SelectedActor, HoveredEntry, Picker, ScreenMouseX, ScreenMouseY) && OnSelectionChanged)
		{
			OnSelectionChanged();
		}
		return;
	}

	case WM_LBUTTONUP:
	{
		if (!Gizmo.IsDragging())
		{
			return;
		}

		Gizmo.EndDrag();
		FViewport* Viewport = ViewportRegistry.GetViewportById(Slate->GetHoveredViewportId());
		if (Viewport)
		{
			FViewportEntry* HoveredEntry = ViewportRegistry.FindEntryByViewportID(Slate->GetHoveredViewportId());
			const FRect& Rect = Viewport->GetRect();
			ScreenWidth = Rect.Width;
			ScreenHeight = Rect.Height;
			ScreenMouseX = MouseX - Rect.X;
			ScreenMouseY = MouseY - Rect.Y;
			Gizmo.UpdateHover(SelectedActor, HoveredEntry, Picker, ScreenMouseX, ScreenMouseY);
		}
		else
		{
			Gizmo.ClearHover();
		}

		if (OnSelectionChanged)
		{
			OnSelectionChanged();
		}
		return;
	}

	default:
		return;
	}
}
