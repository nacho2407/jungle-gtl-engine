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
#include "Runtime/SceneView.h"
#include "EditorUtils.h"

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;

	// UE_LOG("Hello ZZup Engine! %d", 2026);
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
	// TODO: 나중에 기능 완성되면 주석해제
	// if (!State->bFocused) return;
	TickInput(DeltaTime);
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
			//USceneComponent* RootComp = Actor->GetRootComponent();
			//if (!RootComp->IsA<UPrimitiveComponent>()) continue;

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
