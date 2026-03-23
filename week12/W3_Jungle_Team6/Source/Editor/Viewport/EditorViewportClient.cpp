#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Core/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Object/Object.h"
#include "Editor/Selection/SelectionManager.h"

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;

	UE_LOG("Hello ZZup Engine! %d", 2026);
}

void FEditorViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
}

void FEditorViewportClient::DestroyCamera()
{
	if (Camera)
	{
		UObjectManager::Get().DestroyObject(Camera);
		Camera = nullptr;
	}
}

void FEditorViewportClient::ResetCamera()
{
	if (!Camera || !Settings) return;
	Camera->SetWorldLocation(Settings->InitViewPos);
	Camera->LookAt(Settings->InitLookAt);
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

	if (Camera)
	{
		Camera->OnResize(static_cast<int32>(WindowWidth), static_cast<int32>(WindowHeight));
	}
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
	TickCursorOverlay(DeltaTime);
}

void FEditorViewportClient::TickInput(float DeltaTime)
{
	if (!Camera)
	{
		return;
	}

	if (InputSystem::Get().GetGuiInputState().bUsingKeyboard == true)
	{
		return;
	}

	const FCameraState& CameraState = Camera->GetCameraState();

	const float MoveSensitivity = Settings ? Settings->CameraMoveSensitivity : 1.f;
	const float CameraSpeed = (Settings ? Settings->CameraSpeed : 10.f) * MoveSensitivity;

	FVector Move = FVector(0, 0, 0);

	if (InputSystem::Get().GetKey('W') && !CameraState.bIsOrthogonal)
		Move.X += CameraSpeed;
	if (InputSystem::Get().GetKey('A'))
		Move.Y -= CameraSpeed;
	if (InputSystem::Get().GetKey('S') && !CameraState.bIsOrthogonal)
		Move.X -= CameraSpeed;
	if (InputSystem::Get().GetKey('D'))
		Move.Y += CameraSpeed;
	if (InputSystem::Get().GetKey('Q'))
		Move.Z -= CameraSpeed;
	if (InputSystem::Get().GetKey('E'))
		Move.Z += CameraSpeed;

	Move *= DeltaTime;
	Camera->MoveLocal(Move);

	FVector Rotation = FVector(0, 0, 0);
	FVector MouseRotation = FVector(0, 0, 0);

	const float RotateSensitivity = Settings ? Settings->CameraRotateSensitivity : 1.f;
	const float AngleVelocity = (Settings ? Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
	if (InputSystem::Get().GetKey(VK_UP))
		Rotation.Z -= AngleVelocity;
	if (InputSystem::Get().GetKey(VK_LEFT))
		Rotation.Y -= AngleVelocity;
	if (InputSystem::Get().GetKey(VK_DOWN))
		Rotation.Z += AngleVelocity;
	if (InputSystem::Get().GetKey(VK_RIGHT))
		Rotation.Y += AngleVelocity;

	// Mouse sensitivity is degrees per pixel (do not multiply by DeltaTime)
	float MouseRotationSpeed = 0.15f * RotateSensitivity;

	if (InputSystem::Get().GetKey(VK_RBUTTON))
	{
		float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
		float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());

		MouseRotation.Y += DeltaX * MouseRotationSpeed; // yaw
		MouseRotation.Z += DeltaY * MouseRotationSpeed; // pitch

		MouseRotation.Y = Clamp(MouseRotation.Y, -89.0f, 89.0f);
		MouseRotation.Z = Clamp(MouseRotation.Z, -89.0f, 89.0f);
	}

	if (InputSystem::Get().GetKeyUp(VK_SPACE))
		Gizmo->SetNextMode();

	Rotation *= DeltaTime;
	Camera->Rotate(Rotation.Y + MouseRotation.Y, Rotation.Z + MouseRotation.Z);

	if (InputSystem::Get().GetKeyDown('O')) {
		Camera->SetOrthographic(!CameraState.bIsOrthogonal);
	}
}

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;

	if (!Camera || !Gizmo || !World)
	{
		return;
	}

	Gizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation());

	if (InputSystem::Get().GetGuiInputState().bUsingMouse)
	{
		return;
	}

	const float ZoomSpeed = Settings ? Settings->CameraZoomSpeed : 300.f;

	float ScrollNotches = InputSystem::Get().GetScrollNotches();
	if (ScrollNotches != 0.0f) {
		if (Camera->IsOrthogonal()) {
			float NewWidth = Camera->GetOrthoWidth() - ScrollNotches * ZoomSpeed * DeltaTime;
			Camera->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f));
		}
		else {
			float NewFOV = Camera->GetFOV() - ScrollNotches * ZoomSpeed * DeltaTime;
			Camera->SetFOV(Clamp(NewFOV, 1.f * DEG_TO_RAD, 90.0f * DEG_TO_RAD));
		}
	}

	POINT MousePoint = InputSystem::Get().GetMousePos();
	MousePoint = Window->ScreenToClientPoint(MousePoint);

	//	Cursor
	CursorOverlayState.ScreenX = static_cast<float>(MousePoint.x);
	CursorOverlayState.ScreenY = static_cast<float>(MousePoint.y);

	if (InputSystem::Get().GetKeyDown(VK_LBUTTON))
	{
		CursorOverlayState.bPressed = true;
		CursorOverlayState.bVisible = true;
		CursorOverlayState.TargetRadius = CursorOverlayState.MaxRadius;
		CursorOverlayState.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow for left-click

		if (bIsCursorVisible)
		{
			while (ShowCursor(FALSE) >= 0);
			bIsCursorVisible = false;
		}

	}

	if (InputSystem::Get().GetKeyUp(VK_LBUTTON))
	{
		CursorOverlayState.bPressed = false;
		CursorOverlayState.TargetRadius = 0.0f;

		if (!bIsCursorVisible)
		{
			while (ShowCursor(TRUE) < 0);
			bIsCursorVisible = true;
		}
	}

	if (InputSystem::Get().GetKeyDown(VK_RBUTTON))
	{
		CursorOverlayState.bPressed = true;
		CursorOverlayState.bVisible = true;
		CursorOverlayState.TargetRadius = CursorOverlayState.MaxRadius;
		CursorOverlayState.Color = FVector4(0.0f, 0.0f, 1.0f, 1.0f);  // Blue for right-click

		if (bIsCursorVisible)
		{
			while (ShowCursor(FALSE) >= 0);
			bIsCursorVisible = false;
		}
	}

	if (InputSystem::Get().GetKeyUp(VK_RBUTTON))
	{
		CursorOverlayState.bPressed = false;
		CursorOverlayState.TargetRadius = 0.0f;

		if (!bIsCursorVisible)
		{
			while (ShowCursor(TRUE) < 0);
			bIsCursorVisible = true;
		}
	}

	FRay Ray = Camera->DeprojectScreenToWorld(static_cast<float>(MousePoint.x), static_cast<float>(MousePoint.y), WindowWidth, WindowHeight);
	FHitResult HitResult;

	//Gizmo Hover
	Gizmo->Raycast(Ray, HitResult);

	if (InputSystem::Get().GetKeyDown(VK_LBUTTON))
	{
		HandleDragStart(Ray);
	}
	else if (InputSystem::Get().GetLeftDragging())
	{
		//	눌려있고, Holding되지 않았다면 다음 Loop부터 드래그 업데이트 시작
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
			if (!Actor || !Actor->GetRootComponent()) {
				continue;
			}
			USceneComponent* RootComp = Actor->GetRootComponent();
			if (!RootComp->IsA<UPrimitiveComponent>()) continue;
			UPrimitiveComponent* PrimitiveComp = static_cast<UPrimitiveComponent*>(RootComp);

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

		bool bCtrlHeld = InputSystem::Get().GetKey(VK_CONTROL);

		if (BestActor == nullptr)
		{
			if (!bCtrlHeld)
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

void FEditorViewportClient::TickCursorOverlay(float DeltaTime)
{
	const float Alpha = std::min(1.0f, DeltaTime * CursorOverlayState.LerpSpeed);
	CursorOverlayState.CurrentRadius += (CursorOverlayState.TargetRadius - CursorOverlayState.CurrentRadius) * Alpha;

	if (!CursorOverlayState.bPressed && CursorOverlayState.CurrentRadius < 0.01f)
	{
		CursorOverlayState.CurrentRadius = 0.0f;
		CursorOverlayState.bVisible = false;
	}
}
