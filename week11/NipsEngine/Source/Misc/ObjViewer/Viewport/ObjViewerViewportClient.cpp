#include "Misc/ObjViewer/Viewport/ObjViewerViewportClient.h"

#include "Misc/ObjViewer/Settings/ObjViewerSettings.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Engine/Core/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Editor/Selection/SelectionManager.h"
#include "GameFramework/World.h"
#include "Object/Object.h"

#include "Viewport/ViewportCamera.h"

void FObjViewerViewportClient::Initialize(FWindowsWindow* InWindow)
{
    Window = InWindow;
    NavigationController.SetCamera(Camera);
}

void FObjViewerViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = new FViewportCamera();

	Camera->SetLocation(FVector(10, 0, 5));
	Camera->SetLookAt(FVector(0, 0, 0));

	NavigationController.SetCamera(Camera);
	NavigationController.ResetTargetLocation();
}

void FObjViewerViewportClient::DestroyCamera()
{
	if (Camera)
	{
		delete Camera;
		Camera = nullptr;
	}
	NavigationController.SetCamera(nullptr);
}

// 카메라를 초기 위치로 이동시킨다.
void FObjViewerViewportClient::ResetCamera()
{
    if (!Camera || !Settings)
        return;

	SavedCameraLocation = Camera->GetLocation();
	SavedCameraRotation = Camera->GetRotation();
	bSavedCameraPosition = true;
}

// 카메라를 초기 위치로 애니메이션과 함께 부드럽게 이동시킨다.
void FObjViewerViewportClient::ResetCameraSmoothly()
{
    if (!Camera || !Settings)
        return;

	FVector TargetPos = SavedCameraLocation;
	FQuat TargetRot = SavedCameraRotation;

    CameraGUIParams.ResetStartLocation = Camera->GetLocation();
    CameraGUIParams.ResetStartRotation = Camera->GetRotation();
    CameraGUIParams.ResetTargetLocation = TargetPos;
    CameraGUIParams.ResetTargetRotation = TargetRot;

    CameraGUIParams.bIsResettingCamera = true;
    CameraGUIParams.ResetCameraProgress = 0.0f;
}

void FObjViewerViewportClient::SaveCameraPosition()
{
	if (!Camera) return;

	bSavedCameraPosition = true;
	SavedCameraLocation = Camera->GetLocation();
	SavedCameraRotation = Camera->GetRotation();
}

void FObjViewerViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	if (InWidth > 0.0f) WindowWidth = InWidth;
	if (InHeight > 0.0f) WindowHeight = InHeight;

	if (Camera)
	{
		Camera->OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
	}
}

void FObjViewerViewportClient::Tick(float DeltaTime)
{
	if (ImGui::GetIO().WantCaptureMouse) 
	{
		TickCursorOverlay(DeltaTime);
		return;
	}

	TickInput(DeltaTime);
	NavigationController.Tick(DeltaTime); // 내비게이션 컨트롤러의 부드러운 위치 보간 적용
	TickInteraction(DeltaTime);
	TickCursorOverlay(DeltaTime);
}

void FObjViewerViewportClient::TickInput(float DeltaTime)
{
	if (!Camera) return;

	if (InputSystem::Get().GetGuiInputState().bUsingKeyboard == true) return;

	const bool bAltDown = InputSystem::Get().GetKey(VK_MENU);
	const bool bCtrlDown = InputSystem::Get().GetKey(VK_CONTROL);
	const bool bShiftDown = InputSystem::Get().GetKey(VK_SHIFT);
	
	if (!bAltDown)
	{
		if (bAltRightMouseDollying)
		{
			bAltRightMouseDollying = false;
			NavigationController.ResetTargetLocation();
		}
	}

	// Mouse button begin/end state bridge
	if (InputSystem::Get().GetKeyDown(VK_RBUTTON) && !bCtrlDown && !bAltDown && !bShiftDown)
	{
		if (Camera->GetProjectionType() == EViewportProjectionType::Orthographic)
		{
			bRightMousePanning = true;
			bFirstMouseMoveAfterRightPanStart = true;
			NavigationController.BeginPanning();
		}
		else
		{
			bRightMouseRotating = true;
			bFirstMouseMoveAfterRotateStart = true;
			NavigationController.SetRotating(true);
		}

		if (bIsCursorVisible)
		{	
			for (int32 Cnt = 0; ShowCursor(FALSE) >= 0 && Cnt < 10; ++Cnt) {}
			bIsCursorVisible = false;
		}
	}

	if (InputSystem::Get().GetKeyDown(VK_RBUTTON) && bAltDown && !bCtrlDown && !bShiftDown)
	{
		bAltRightMouseDollying = true;
		bFirstMouseMoveAfterDollyStart = true;

		if (bIsCursorVisible)
		{
			for (int32 Cnt = 0; ShowCursor(FALSE) >= 0 && Cnt < 10; ++Cnt) {}
			bIsCursorVisible = false;
		}
	}

	if (InputSystem::Get().GetKeyUp(VK_RBUTTON))
	{
		if (bRightMouseRotating)
		{
			bRightMouseRotating = false;
			NavigationController.SetRotating(false);
		}
		if (bRightMousePanning)
		{
			bRightMousePanning = false;
			NavigationController.EndPanning();
			NavigationController.ResetTargetLocation();
		}
		if (bAltRightMouseDollying)
		{
			bAltRightMouseDollying = false;
			NavigationController.ResetTargetLocation();
		}

		if (!bIsCursorVisible)
		{
			for (int32 Cnt = 0; ShowCursor(TRUE) < 0 && Cnt < 10; ++Cnt) {}
			bIsCursorVisible = true;
		}
	}

	// 중앙 휠 버튼 처리 (Panning)
	if (InputSystem::Get().GetKeyDown(VK_MBUTTON))
	{
		if(Camera->GetProjectionType() == EViewportProjectionType::Perspective)
		{
			bMiddleMousePanning = true;
			bFirstMouseMoveAfterPanStart = true;
			NavigationController.BeginPanning();

			if (bIsCursorVisible)
			{
				for (int32 Cnt = 0; ShowCursor(FALSE) >= 0 && Cnt < 10; ++Cnt) {}
				bIsCursorVisible = false;
			}
		}
	}

	if (InputSystem::Get().GetKeyUp(VK_MBUTTON) && bMiddleMousePanning)
	{
		if (Camera->GetProjectionType() == EViewportProjectionType::Perspective)
		{
			bMiddleMousePanning = false;
			NavigationController.EndPanning();
			NavigationController.ResetTargetLocation();

			if (!bIsCursorVisible)
			{
				for (int32 Cnt = 0; ShowCursor(TRUE) < 0 && Cnt < 10; ++Cnt) {}
				bIsCursorVisible = true;
			}
		}
	}
		
	const float MoveSensitivity = Settings ? Settings->CameraMoveSensitivity : 1.0f;
	const float RotateSensitivity = Settings ? Settings->CameraRotateSensitivity : 1.0f;

	// Keyboard movement while rotating (WASD, EQ)
	if (bRightMouseRotating)
	{
		float ForwardValue = 0.f;
		float RightValue = 0.f;
		float UpValue = 0.f;

		if (InputSystem::Get().GetKey('W')) ForwardValue += 1.f;
		if (InputSystem::Get().GetKey('S')) ForwardValue -= 1.f;
		if (InputSystem::Get().GetKey('D')) RightValue += 1.f;
		if (InputSystem::Get().GetKey('A')) RightValue -= 1.f;
		if (InputSystem::Get().GetKey('E')) UpValue += 1.f;
		if (InputSystem::Get().GetKey('Q')) UpValue -= 1.f;

		FVector NormalizedInput(ForwardValue, RightValue, UpValue);
		NormalizedInput = NormalizedInput.GetSafeNormal() * MoveSensitivity;

		NavigationController.MoveForward(NormalizedInput.X, DeltaTime);
		NavigationController.MoveRight(NormalizedInput.Y, DeltaTime);
		NavigationController.MoveUp(NormalizedInput.Z, DeltaTime);
	}

	// Mouse rotate / pan / orbit	
	const float MouseDeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
	const float MouseDeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());

	const float ScaledRotateX = MouseDeltaX * RotateSensitivity;
	const float ScaledRotateY = MouseDeltaY * RotateSensitivity;
	const float ScaledPanX = MouseDeltaX * MoveSensitivity;
	const float ScaledPanY = MouseDeltaY * MoveSensitivity;

	if (bRightMouseRotating)
	{
		if (bFirstMouseMoveAfterRotateStart) bFirstMouseMoveAfterRotateStart = false;
		else
		{
			NavigationController.AddYawInput(ScaledRotateX);
			NavigationController.AddPitchInput(-ScaledRotateY);
		}
	}

	if (bMiddleMousePanning)
	{
		if (bFirstMouseMoveAfterPanStart) bFirstMouseMoveAfterPanStart = false;
		else NavigationController.AddPanInput(ScaledPanX, -ScaledPanY);
	}

	if (bRightMousePanning)
	{
		if (bFirstMouseMoveAfterRightPanStart) bFirstMouseMoveAfterRightPanStart = false;
		else NavigationController.AddPanInput(MouseDeltaX, -MouseDeltaY);
	}

	if (bAltRightMouseDollying)
	{
		if (bFirstMouseMoveAfterDollyStart) bFirstMouseMoveAfterDollyStart = false;
		else
		{
			const float ClampedDollyInput = MathUtil::Clamp(MouseDeltaY, -12.0f, 12.0f);
			NavigationController.Dolly(ClampedDollyInput * 0.12f);
		}
	}

	// Zoom / speed
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

	if (InputSystem::Get().GetKeyDown('O'))
	{
		const EViewportProjectionType Current = Camera->GetProjectionType();
		const EViewportProjectionType Next = (Current == EViewportProjectionType::Perspective)
			? EViewportProjectionType::Orthographic
			: EViewportProjectionType::Perspective;
		Camera->SetProjectionType(Next);
	}
}

void FObjViewerViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;

	if (!Camera || !World) return;

	// UI(ImGui)가 마우스를 차지했을 때의 예외 처리 복구
	if (InputSystem::Get().GetGuiInputState().bUsingMouse)
	{
		if (!bIsCursorVisible)
		{
			while (ShowCursor(TRUE) < 0);
			bIsCursorVisible = true;
			
			CursorOverlayState.bPressed = false;
			CursorOverlayState.TargetRadius = 0.0f;
		}
		return;
	}

    POINT MousePoint = InputSystem::Get().GetMousePos();
    MousePoint = Window->ScreenToClientPoint(MousePoint);

    float LocalMouseX = static_cast<float>(MousePoint.x) - ViewportX;
    float LocalMouseY = static_cast<float>(MousePoint.y) - ViewportY;

    FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, WindowWidth, WindowHeight);
    CursorOverlayState.ScreenX = LocalMouseX;
    CursorOverlayState.ScreenY = LocalMouseY;

	// 좌클릭 시에만 레이캐스트 수행 및 커서 오버레이 애니메이션 실행 (우클릭 오버레이 제거)
	if (InputSystem::Get().GetKeyDown(VK_LBUTTON))
	{
		CursorOverlayState.bPressed = true;
		CursorOverlayState.bVisible = true;
		CursorOverlayState.TargetRadius = CursorOverlayState.MaxRadius;
		CursorOverlayState.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f); // 노란색

		HandleDragStart(Ray);
	}

	if (InputSystem::Get().GetKeyUp(VK_LBUTTON))
	{
		CursorOverlayState.bPressed = false;
		CursorOverlayState.TargetRadius = 0.0f;
	}
}

void FObjViewerViewportClient::SetViewportRect(float InX, float InY, float InWidth, float InHeight)
{
    ViewportX = InX;
    ViewportY = InY;
    
    WindowWidth = InWidth > 0.0f ? InWidth : 1.0f;
    WindowHeight = InHeight > 0.0f ? InHeight : 1.0f;

    if (Camera)
    {
        Camera->OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
    }
}

void FObjViewerViewportClient::HandleDragStart(const FRay& Ray)
{
	FHitResult HitResult{};
	AActor* BestActor = nullptr;
	float ClosestDistance = FLT_MAX;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || !Actor->GetRootComponent()) continue;

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

	// 픽킹 처리 기능이 필요하다면 여기에 추가 (현재 본문에서는 Ctrl을 검사하고 끝남)
	bool bCtrlHeld = InputSystem::Get().GetKey(VK_CONTROL);
}

void FObjViewerViewportClient::TickCursorOverlay(float DeltaTime)
{
	const float Alpha = std::min(1.0f, DeltaTime * CursorOverlayState.LerpSpeed);
	CursorOverlayState.CurrentRadius += (CursorOverlayState.TargetRadius - CursorOverlayState.CurrentRadius) * Alpha;

	if (!CursorOverlayState.bPressed && CursorOverlayState.CurrentRadius < 0.01f)
	{
		CursorOverlayState.CurrentRadius = 0.0f;
		CursorOverlayState.bVisible = false;
	}
}