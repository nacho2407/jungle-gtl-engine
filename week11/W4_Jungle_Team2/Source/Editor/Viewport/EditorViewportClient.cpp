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
	TickInput(DeltaTime);
	NavigationController.Tick(DeltaTime);
	TickInteraction(DeltaTime);
	TickCursorOverlay(DeltaTime);
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
		
	const float MoveSensitivity = Settings ? Settings->CameraMoveSensitivity : 1.0f;
	const float RotateSensitivity = Settings ? Settings->CameraRotateSensitivity : 1.0f;

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
		NormalizedInput = NormalizedInput.GetSafeNormal() * MoveSensitivity;

		NavigationController.MoveForward(NormalizedInput.X, DeltaTime);
		NavigationController.MoveRight(NormalizedInput.Y, DeltaTime);
		NavigationController.MoveUp(NormalizedInput.Z, DeltaTime);
	}

	// ----------------------------
	// Mouse rotate / pan / orbit
	// ----------------------------
	const float MouseDeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
	const float MouseDeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());

	const float ScaledRotateX = MouseDeltaX * RotateSensitivity;
	const float ScaledRotateY = MouseDeltaY * RotateSensitivity;
	const float ScaledPanX = MouseDeltaX * MoveSensitivity;
	const float ScaledPanY = MouseDeltaY * MoveSensitivity;

	if (bRightMouseRotating)
	{
		if (bFirstMouseMoveAfterRotateStart)
		{
			bFirstMouseMoveAfterRotateStart = false;
		}
		else
		{
			NavigationController.AddYawInput(ScaledRotateX);
			NavigationController.AddPitchInput(-ScaledRotateY);
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
			NavigationController.AddPanInput(ScaledPanX, -ScaledPanY);
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
			NavigationController.AddYawInput(ScaledRotateX);
			NavigationController.AddPitchInput(-ScaledRotateY);
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

	CursorOverlayState.ScreenX = static_cast<float>(MousePoint.x);
	CursorOverlayState.ScreenY = static_cast<float>(MousePoint.y);

	if (InputSystem::Get().GetKeyDown(VK_LBUTTON))
	{
		CursorOverlayState.bPressed = true;
		CursorOverlayState.bVisible = true;
		CursorOverlayState.TargetRadius = CursorOverlayState.MaxRadius;
		CursorOverlayState.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
	}

	if (InputSystem::Get().GetKeyUp(VK_LBUTTON))
	{
		CursorOverlayState.bPressed = false;
		CursorOverlayState.TargetRadius = 0.0f;
	}

	if (InputSystem::Get().GetKeyDown(VK_RBUTTON))
	{
		CursorOverlayState.bPressed = true;
		CursorOverlayState.bVisible = true;
		CursorOverlayState.TargetRadius = CursorOverlayState.MaxRadius;
		CursorOverlayState.Color = FVector4(0.0f, 0.0f, 1.0f, 1.0f);
	}

	if (InputSystem::Get().GetKeyUp(VK_RBUTTON))
	{
		CursorOverlayState.bPressed = false;
		CursorOverlayState.TargetRadius = 0.0f;
	}

	FRay Ray = Camera.DeprojectScreenToWorld(
		static_cast<float>(MousePoint.x),
		static_cast<float>(MousePoint.y),
		WindowWidth,
		WindowHeight
	);

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
			if (SelectionManager)
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
}

void FEditorViewportClient::TickCursorOverlay(float DeltaTime)
{
	const float Alpha = std::min(1.0f, DeltaTime * CursorOverlayState.LerpSpeed);
	CursorOverlayState.CurrentRadius +=
		(CursorOverlayState.TargetRadius - CursorOverlayState.CurrentRadius) * Alpha;

	if (!CursorOverlayState.bPressed && CursorOverlayState.CurrentRadius < 0.01f)
	{
		CursorOverlayState.CurrentRadius = 0.0f;
		CursorOverlayState.bVisible = false;
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
