#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Core/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "GameFramework/World.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Object/Object.h"
#include "Editor/Selection/SelectionManager.h"
#include "Runtime/SceneView.h"
#include "EditorUtils.h"

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;
	NavigationController.SetCamera(&Camera);
}

void FEditorViewportClient::SetSelectionManager(FSelectionManager* InSelectionManager)
{
	SelectionManager = InSelectionManager;
	NavigationController.SetSelectionManager(InSelectionManager);
}

void FEditorViewportClient::CreateCamera()
{
	bHasCamera = true;
	Camera = FViewportCamera();
	Camera.OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
	NavigationController.SetCamera(&Camera);
}

void FEditorViewportClient::DestroyCamera()
{
	bHasCamera = false;
}

void FEditorViewportClient::ResetCamera()
{
	if (!bHasCamera || !Settings)
	{
		return;
	}

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
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	if (InWidth > 0.0f)
	{
		WindowWidth = InWidth;
	}

	if (InHeight > 0.0f)
	{
		WindowHeight = InHeight;
	}

	if (bHasCamera)
	{
		Camera.OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
	}
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	// TODO: 나중에 기능 완성되면 주석해제
	// if (!State->bFocused) return;
	TickInput(DeltaTime);
	NavigationController.Tick(DeltaTime);
	TickInteraction(DeltaTime);
}

void FEditorViewportClient::BuildSceneView(FSceneView& OutView) const
{
	if (!Camera) return;
	// Renderer 에서 사용할 SceneView 설정 
	OutView.ViewMatrix           = Camera->GetViewMatrix();
	OutView.ProjectionMatrix     = Camera->GetProjectionMatrix();
	OutView.ViewProjectionMatrix = OutView.ViewMatrix * OutView.ProjectionMatrix;

	OutView.CameraPosition = Camera->GetWorldLocation();
	OutView.CameraForward  = Camera->GetForwardVector();
	OutView.CameraRight    = Camera->GetRightVector();
	OutView.CameraUp       = Camera->GetUpVector();

	OutView.bOrthographic = Camera->IsOrthogonal();

	if (State)
	{
		OutView.ViewRect = State->Rect;
		OutView.ViewMode = State->ViewMode;
	}
}

void FEditorViewportClient::ApplyCameraMode()
{
	if (!Camera) return;

	// 직교 뷰는 기존 회전값이 LookAt에 간섭하지 않도록 초기화
	Camera->SetRelativeRotation(FVector(0.f, 0.f, 0.f));

	switch (ViewportType)
	{
	case EVT_Perspective:
		Camera->SetOrthographic(false);
		// Perspective 카메라 위치/방향은 Settings->InitViewPos 기준 유지
		if (Settings)
		{
			Camera->SetWorldLocation(Settings->InitViewPos);
			Camera->LookAt(Settings->InitLookAt);
		}
		break;

	// --- 직교 뷰 (X=Forward, Y=Right, Z=Up) ---

	case EVT_OrthoTop:			// 위에서 아래 (-Z 방향)
		Camera->SetOrthographic(true);
		Camera->SetWorldLocation(FVector(0.f, 0.f, 1000.f));
		Camera->LookAt(FVector(0.f, 0.f, 0.f));
		break;

	case EVT_OrthoBottom:		// 아래에서 위 (+Z 방향)
		Camera->SetOrthographic(true);
		Camera->SetWorldLocation(FVector(0.f, 0.f, -1000.f));
		Camera->LookAt(FVector(0.f, 0.f, 0.f));
		break;

	case EVT_OrthoFront:		// 앞(-X)에서 뒤 (+X 방향)
		Camera->SetOrthographic(true);
		Camera->SetWorldLocation(FVector(-1000.f, 0.f, 0.f));
		Camera->LookAt(FVector(0.f, 0.f, 0.f));
		break;

	case EVT_OrthoBack:			// 뒤(+X)에서 앞 (-X 방향)
		Camera->SetOrthographic(true);
		Camera->SetWorldLocation(FVector(1000.f, 0.f, 0.f));
		Camera->LookAt(FVector(0.f, 0.f, 0.f));
		break;

	case EVT_OrthoLeft:			// 왼쪽(-Y)에서 오른쪽 (+Y 방향)
		Camera->SetOrthographic(true);
		Camera->SetWorldLocation(FVector(0.f, -1000.f, 0.f));
		Camera->LookAt(FVector(0.f, 0.f, 0.f));
		break;

	case EVT_OrthoRight:		// 오른쪽(+Y)에서 왼쪽 (-Y 방향)
		Camera->SetOrthographic(true);
		Camera->SetWorldLocation(FVector(0.f, 1000.f, 0.f));
		Camera->LookAt(FVector(0.f, 0.f, 0.f));
		break;

	default:
		break;
	}
}

bool FEditorViewportClient::OnMouseMove(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FEditorViewportClient::OnMouseButtonDown(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FEditorViewportClient::OnMouseButtonUp(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FEditorViewportClient::OnMouseWheel(float Delta)
{
	return false;
}

bool FEditorViewportClient::OnKeyDown(uint32 Key)
{
	return false;
}

bool FEditorViewportClient::OnKeyUp(uint32 Key)
{
	return false;
}

void FEditorViewportClient::TickInput(float DeltaTime)
{
	if (!bHasCamera)
	{
		return;
	}

	if (InputSystem::Get().GetGuiInputState().bUsingKeyboard)
	{
		return;
	}

	const bool bAltDown = InputSystem::Get().GetKey(VK_MENU);
	const bool bCtrlDown = InputSystem::Get().GetKey(VK_CONTROL);
	const bool bShiftDown = InputSystem::Get().GetKey(VK_SHIFT);

	// ----------------------------
	// Mouse button begin/end state bridge
	// ----------------------------
	if (InputSystem::Get().GetKeyDown(VK_RBUTTON) && !bCtrlDown && !bAltDown && !bShiftDown)
	{
		bRightMouseRotating = true;
		bFirstMouseMoveAfterRotateStart = true;
		NavigationController.SetRotating(true);

		if (bIsCursorVisible)
		{
			while (ShowCursor(FALSE) >= 0) {}
			bIsCursorVisible = false;
		}
	}

	if (InputSystem::Get().GetKeyUp(VK_RBUTTON) && bRightMouseRotating)
	{
		bRightMouseRotating = false;
		NavigationController.SetRotating(false);

		if (!bIsCursorVisible)
		{
			while (ShowCursor(TRUE) < 0) {}
			bIsCursorVisible = true;
		}
	}

	if (InputSystem::Get().GetKeyDown(VK_MBUTTON))
	{
		bMiddleMousePanning = true;
		bFirstMouseMoveAfterPanStart = true;
		NavigationController.BeginPanning();

		if (bIsCursorVisible)
		{
			while (ShowCursor(FALSE) >= 0) {}
			bIsCursorVisible = false;
		}
	}

	if (InputSystem::Get().GetKeyUp(VK_MBUTTON) && bMiddleMousePanning)
	{
		bMiddleMousePanning = false;
		NavigationController.EndPanning();

		if (!bIsCursorVisible)
		{
			while (ShowCursor(TRUE) < 0) {}
			bIsCursorVisible = true;
		}
	}

	if (InputSystem::Get().GetKeyDown(VK_LBUTTON) && bAltDown && !bCtrlDown && !bShiftDown)
	{
		bAltLeftMouseOrbiting = true;
		bFirstMouseMoveAfterOrbitStart = true;
		NavigationController.BeginOrbit(ResolveOrbitPivot());

		if (bIsCursorVisible)
		{
			while (ShowCursor(FALSE) >= 0) {}
			bIsCursorVisible = false;
		}
	}

	if ((InputSystem::Get().GetKeyUp(VK_LBUTTON) || !bAltDown) && bAltLeftMouseOrbiting)
	{
		bAltLeftMouseOrbiting = false;
		NavigationController.EndOrbit();

		if (!bIsCursorVisible)
		{
			while (ShowCursor(TRUE) < 0) {}
			bIsCursorVisible = true;
		}
	}

	// ----------------------------
	// Keyboard movement while rotating
	// (이전 엔진 감각 유지)
	// ----------------------------
	if (bRightMouseRotating)
	{
		float ForwardValue = 0.f;
		float RightValue = 0.f;
		float UpValue = 0.f;

		if (InputSystem::Get().GetKey('W'))
			ForwardValue += 1.f;
		if (InputSystem::Get().GetKey('S'))
			ForwardValue -= 1.f;
		if (InputSystem::Get().GetKey('D'))
			RightValue += 1.f;
		if (InputSystem::Get().GetKey('A'))
			RightValue -= 1.f;
		if (InputSystem::Get().GetKey('E'))
			UpValue += 1.f;
		if (InputSystem::Get().GetKey('Q'))
			UpValue -= 1.f;

		FVector NormalizedInput(ForwardValue, RightValue, UpValue);
		NormalizedInput = NormalizedInput.GetSafeNormal();

		NavigationController.MoveForward(NormalizedInput.X, DeltaTime);
		NavigationController.MoveRight(NormalizedInput.Y, DeltaTime);
		NavigationController.MoveUp(NormalizedInput.Z, DeltaTime);
	}

	// ----------------------------
	// Mouse rotate / pan / orbit
	// ----------------------------
	const float MouseDeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
	const float MouseDeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());

	if (bRightMouseRotating)
	{
		if (bFirstMouseMoveAfterRotateStart)
		{
			bFirstMouseMoveAfterRotateStart = false;
		}
		else
		{
			NavigationController.AddYawInput(MouseDeltaX);
			NavigationController.AddPitchInput(-MouseDeltaY);
		}
	}

	if (bMiddleMousePanning)
	{
		if (bFirstMouseMoveAfterPanStart)
		{
			bFirstMouseMoveAfterPanStart = false;
		}
		else
		{
			NavigationController.AddPanInput(MouseDeltaX, -MouseDeltaY);
		}
	}

	if (bAltLeftMouseOrbiting)
	{
		if (bFirstMouseMoveAfterOrbitStart)
		{
			bFirstMouseMoveAfterOrbitStart = false;
		}
		else
		{
			NavigationController.AddYawInput(MouseDeltaX);
			NavigationController.AddPitchInput(-MouseDeltaY);
		}
	}

	// ----------------------------
	// Zoom / speed
	// ----------------------------
	const float ScrollNotches = InputSystem::Get().GetScrollNotches();
	if (!MathUtil::IsNearlyZero(ScrollNotches))
	{
		if (bRightMouseRotating)
		{
			const float SpeedStep = (ScrollNotches > 0.f) ? 5.0f : -5.0f;
			NavigationController.AdjustMoveSpeed(SpeedStep);
		}
		else
		{
			NavigationController.ModifyFOVorOrthoHeight(-ScrollNotches);
		}
	}

	// Toggle projection
	if (InputSystem::Get().GetKeyDown('O'))
	{
		if (Camera.GetProjectionType() == EViewportProjectionType::Perspective)
		{
			Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		}
		else
		{
			Camera.SetProjectionType(EViewportProjectionType::Perspective);
		}
	}

	if (InputSystem::Get().GetKeyUp(VK_SPACE) && Gizmo)
	{
		Gizmo->SetNextMode();
	}
}

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;

	if (!bHasCamera || !Gizmo || !World)
	{
		return;
	}

	Gizmo->ApplyScreenSpaceScaling(Camera.GetLocation());

	if (InputSystem::Get().GetGuiInputState().bUsingMouse)
	{
		return;
	}

	POINT MousePoint = InputSystem::Get().GetMousePos();
	MousePoint = Window->ScreenToClientPoint(MousePoint);


	FHitResult HitResult;
	Gizmo->Raycast(Ray, HitResult);

	if (InputSystem::Get().GetKeyDown(VK_LBUTTON))
	{
		HandleDragStart(Ray);
	}
	else if (InputSystem::Get().GetLeftDragging())
	{
		if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding())
		{
			Gizmo->SetHolding(true);
		}

		if (Gizmo->IsHolding())
		{
			Gizmo->UpdateDrag(Ray);
		}
	}
	else if (InputSystem::Get().GetLeftDragEnd())
	{
		Gizmo->DragEnd();
	}
}

void FEditorViewportClient::HandleDragStart(const FRay& Ray)
{
	FHitResult HitResult{};
	if (Gizmo->Raycast(Ray, HitResult))
	{
		Gizmo->SetPressedOnHandle(true);
		UE_LOG("Gizmo is Holding");
	}
	else
	{
		AActor* BestActor = nullptr;
		float ClosestDistance = FLT_MAX;

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !Actor->GetRootComponent())
			{
				continue;
			}

			for (auto* primitive : Actor->GetPrimitiveComponents())
			{
				UPrimitiveComponent* PrimitiveComp = static_cast<UPrimitiveComponent*>(primitive);

				HitResult = {};
				if (PrimitiveComp->Raycast(Ray, HitResult))
				{
					if (HitResult.Distance < ClosestDistance)
					{
						ClosestDistance = HitResult.Distance;
						BestActor = Actor;
					}
				}
			}
		}

		bool bCtrlHeld = InputSystem::Get().GetKey(VK_CONTROL);

		if (BestActor == nullptr)
		{
			if (!bCtrlHeld && SelectionManager)
			{
				SelectionManager->ClearSelection();
			}
		}
		else
		{
			if (bCtrlHeld)
			{
				SelectionManager->ToggleSelect(BestActor);
			}
			else
			{
				SelectionManager->Select(BestActor);
			}
		}
	}
}

FVector FEditorViewportClient::ResolveOrbitPivot() const
{
	if (SelectionManager == nullptr)
	{
		return Camera.GetLocation() + Camera.GetForwardVector() * 300.0f;
	}

	// 현재 SelectionManager API를 모르므로 임시 기본값.
	// 나중에 primary selection center를 반환하도록 연결 권장.
	return Camera.GetLocation() + Camera.GetForwardVector() * 300.0f;
}
