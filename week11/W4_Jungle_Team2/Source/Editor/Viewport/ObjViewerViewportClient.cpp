#include "Editor/Viewport/ObjViewerViewportClient.h"

#include "Editor/Settings/ObjViewerSettings.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Engine/Core/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Editor/Selection/SelectionManager.h"
#include "GameFramework/World.h"
#include "Object/Object.h"

void FObjViewerViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;

	// UE_LOG("Hello ZZup Engine! %d", 2026);
}

void FObjViewerViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
}

void FObjViewerViewportClient::DestroyCamera()
{
	if (Camera)
	{
		UObjectManager::Get().DestroyObject(Camera);
		Camera = nullptr;
	}
}

// 카메라를 초기 위치로 이동시킨다.
void FObjViewerViewportClient::ResetCamera()
{
	if (!Camera || !Settings)
		return;
	Camera->SetWorldLocation(Settings->InitViewPos);
	Camera->LookAt(Settings->InitLookAt);
}

// 모델의 크기와 비례하게 카메라의 이동 범위를 제한한다.
void FObjViewerViewportClient::ClampCameraPosition()
{
	if (!Camera || !World) return;

	// 허용되는 최대 거리를 설정
	FVector SceneCenter = FVector(0, 0, 0);
	float ModelRadius = GetModelRadius();
	float MaxAllowedDistance = ModelRadius * 3.0f;

	// 카메라 위치를 확인하고 이동 범위 제한(Clamp)을 적용
	FVector CamPos = Camera->GetWorldLocation();
	float CurrentDistance = (CamPos - SceneCenter).Size();
	if (CurrentDistance > MaxAllowedDistance)
	{
		FVector Direction = CamPos - SceneCenter;
		Direction.Normalize();

		FVector ClampedPosition = SceneCenter + Direction * MaxAllowedDistance;
		Camera->SetWorldLocation(ClampedPosition);
	}
}

void FObjViewerViewportClient::SetViewportSize(float InWidth, float InHeight)
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

void FObjViewerViewportClient::Tick(float DeltaTime)
{
	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
	TickCursorOverlay(DeltaTime);
}

void FObjViewerViewportClient::TickInput(float DeltaTime)
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

	FVector Rotation = FVector(0, 0, 0);
	FVector MouseRotation = FVector(0, 0, 0);

	const float RotateSensitivity = Settings ? Settings->CameraRotateSensitivity : 1.f;

	// Mouse sensitivity is degrees per pixel (do not multiply by DeltaTime)
	float MouseRotationSpeed = 0.5f * RotateSensitivity;
	if (bIsOrbiting && InputSystem::Get().GetRightDragging())
	{
		float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
		float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());
		const float CurrentPitch = Camera->GetRelativeRotation().Y;
		const float RawPitchDelta = -DeltaY * MouseRotationSpeed;
		const float TargetPitch = MathUtil::Clamp(CurrentPitch + RawPitchDelta, -89.0f, 89.0f);
		const float ClampedPitchDelta = TargetPitch - CurrentPitch;

		// 카메라를 타겟(회전의 중심점) 위치로 이동시킨 뒤, 마우스 이동량만큼 회전
		Camera->SetWorldLocation(OrbitPivot);
		Camera->Rotate(DeltaX * MouseRotationSpeed, ClampedPitchDelta);

		// 회전된 카메라의 '로컬 X축(앞)'의 반대 방향으로 Distance만큼 이동
		Camera->MoveLocal(FVector(-OrbitDistance, 0.0f, 0.0f));
	}

	Rotation *= DeltaTime;
	Camera->Rotate(Rotation.Y + MouseRotation.Y, Rotation.Z + MouseRotation.Z);

	if (InputSystem::Get().GetKeyDown('O'))
	{
		Camera->SetOrthographic(!CameraState.bIsOrthogonal);
	}
}

void FObjViewerViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;

	if (!Camera || !World)
	{
		return;
	}

	if (InputSystem::Get().GetGuiInputState().bUsingMouse)
	{
		return;
	}

	// Viewer에서는 Zoom이 일어나는 대신 카메라를 전후로 이동한다.
	const float ForwardSpeed = Settings ? Settings->CameraForwardSpeed : 500.f;
	float ScrollNotches = InputSystem::Get().GetScrollNotches();
	if (ScrollNotches != 0.0f)
	{
		float ModelRadius = GetModelRadius();
		float DynamicForwardSpeed = ForwardSpeed * ModelRadius;

		if (Camera->IsOrthogonal())
		{
			float NewWidth = Camera->GetOrthoWidth() - ScrollNotches * DynamicForwardSpeed * DeltaTime;
			Camera->SetOrthoWidth(MathUtil::Clamp(NewWidth, 0.1f, 1000.0f));
		}
		else
		{
			float MoveAmount = ScrollNotches * DynamicForwardSpeed * DeltaTime;
			Camera->MoveLocal(FVector(MoveAmount, 0.0f, 0.0f));
		}
	}

	POINT MousePoint = InputSystem::Get().GetMousePos();
	MousePoint = Window->ScreenToClientPoint(MousePoint);
	FRay Ray = Camera->DeprojectScreenToWorld(static_cast<float>(MousePoint.x), static_cast<float>(MousePoint.y),
	                                          WindowWidth, WindowHeight);

	//	Cursor
	CursorOverlayState.ScreenX = static_cast<float>(MousePoint.x);
	CursorOverlayState.ScreenY = static_cast<float>(MousePoint.y);

	if (InputSystem::Get().GetKeyDown(VK_LBUTTON))
	{
		CursorOverlayState.bPressed = true;
		CursorOverlayState.bVisible = true;
		CursorOverlayState.TargetRadius = CursorOverlayState.MaxRadius;
		CursorOverlayState.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow for left-click

		if (bIsCursorVisible)
		{
			while (ShowCursor(FALSE) >= 0);
			bIsCursorVisible = false;
		}

		HandleDragStart(Ray);
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
		CursorOverlayState.Color = FVector4(0.0f, 0.0f, 1.0f, 1.0f); // Blue for right-click

		if (bIsCursorVisible)
		{
			while (ShowCursor(FALSE) >= 0);
			bIsCursorVisible = false;
		}

		POINT MousePoint = InputSystem::Get().GetMousePos();
		MousePoint = Window->ScreenToClientPoint(MousePoint);

		FVector CameraLocation = Camera->GetWorldLocation();
		FVector CameraDirection = Camera->GetForwardVector();
		OrbitDistance = FVector::DotProduct(CameraLocation, CameraDirection);

		OrbitPivot = CameraLocation + CameraDirection * OrbitDistance;

		bIsOrbiting = true;
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

		bIsOrbiting = false;
	}

	FHitResult HitResult;

	if (InputSystem::Get().GetKeyDown(VK_LBUTTON))
	{
		HandleDragStart(Ray);
	}
	else if (InputSystem::Get().GetLeftDragging())
	{
		// 평행 이동(Pan) 로직을 추가한다.
		float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
		float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());

		const float MoveSensitivity = Settings ? Settings->CameraMoveSensitivity : 1.f;
		float PanSpeed = MoveSensitivity * 0.01f;

		FVector Center = FVector(0.0f, 0.0f, 0.0f);
		FVector OldPos = Camera->GetWorldLocation();
		float OldDist = (OldPos - Center).Size();

		// MoveLocal은 로컬 좌표계를 사용한다. (X: 전진, Y: 우측, Z: 상단)
		// 마우스를 우측(+)으로 끌면 화면은 좌측(-)으로, 마우스를 아래(+)로 끌면 화면은 위(+)로 이동합니다.
		FVector Move(0.0f, -DeltaX * PanSpeed, DeltaY * PanSpeed);
		Camera->MoveLocal(Move);

		// 파고들기 방지: 이동 후 거리가 줄어들었다면 원래 거리로 밀어냄
		FVector NewPos = Camera->GetWorldLocation();
		float NewDist = (NewPos - Center).Size();

		if (NewDist < OldDist)
		{
			FVector Dir = NewPos - Center;
			Dir.Normalize();

			// 거리를 OldDist로 강제 복구
			Camera->SetWorldLocation(Center + Dir * OldDist);
		}
	}
	else if (InputSystem::Get().GetLeftDragEnd())
	{
	}

	// Camera Position 및 Lookat 좌표를 제한한다.
	ClampCameraPanToObject();
	ClampCameraPosition();
}

// 물체를 카메라가 보고 있는 화면 중심에서 너무 멀리 이동시킬 수 없도록 한다.
void FObjViewerViewportClient::ClampCameraPanToObject()
{
	if (!Camera || !World) return;

	// 최신 카메라 행렬 확보
	Camera->UpdateWorldMatrix();
	FVector SceneCenter = FVector(0, 0, 0);
	FVector CamPos = Camera->GetWorldLocation();
	FVector CamFwd = Camera->GetForwardVector();
	FVector CamRight = Camera->GetRightVector();
	FVector CamUp = Camera->GetUpVector();

	FVector ToObject = SceneCenter - CamPos;
	float DistZ = FVector::DotProduct(ToObject,CamFwd);
	float DistX = FVector::DotProduct(ToObject,CamRight);
	float DistY = FVector::DotProduct(ToObject,CamUp);

	// 피타고라스의 정리를 이용해 '화면 중앙에서의 2D 직선 거리' 계산
	float CurrentPanDist = std::sqrt(DistX * DistX + DistY * DistY);

	// 직교 투영, 원근 투영에 따라 화면을 벗어나지 않도록 허용할 최대 이탈 거리 계산
	float MaxAllowedPan = 0.0f;
	if (Camera->IsOrthogonal())
	{
		MaxAllowedPan = Camera->GetOrthoWidth() * 0.5f * 0.8f;
	}
	else
	{
		MaxAllowedPan = DistZ * std::tan(Camera->GetFOV() * 0.5f) * 0.8f;
	}

	// 물체가 화면 허용 범위를 넘어갈 때 제한한다.
	if (CurrentPanDist > MaxAllowedPan)
	{
		float CorrectionScale = 1.0f - (MaxAllowedPan / CurrentPanDist);
		FVector Correction = CamRight * (DistX * CorrectionScale) + CamUp * (DistY * CorrectionScale);
		Camera->SetWorldLocation(CamPos + Correction);
	}
}

// 모델의 크기에 따라 카메라 조작 속도를 변경하도록 ModelRadius를 계산한다.
float FObjViewerViewportClient::GetModelRadius()
{
	FVector MinAABB(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxAABB(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool bHasValidMesh = false;

	// 스폰된 액터를 순회하며 AABB 박스를 계산한다. (Viewer에서는 보통 하나)
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || !Actor->GetRootComponent()) continue;

		for (auto* primitive : Actor->GetPrimitiveComponents())
		{
			UPrimitiveComponent* PrimComp = static_cast<UPrimitiveComponent*>(primitive);
			if (!PrimComp || !PrimComp->IsVisible()) continue;

			PrimComp->UpdateWorldAABB();
			FBoundingBox Box = PrimComp->GetWorldAABB();

			MinAABB.X = std::min(MinAABB.X, Box.Min.X);
			MinAABB.Y = std::min(MinAABB.Y, Box.Min.Y);
			MinAABB.Z = std::min(MinAABB.Z, Box.Min.Z);

			MaxAABB.X = std::max(MaxAABB.X, Box.Max.X);
			MaxAABB.Y = std::max(MaxAABB.Y, Box.Max.Y);
			MaxAABB.Z = std::max(MaxAABB.Z, Box.Max.Z);

			bHasValidMesh = true;
		}
	}

	// 씬 중심점과 모델의 최대 크기(반지름)를 계산
	float ModelRadius = 1000.0f;
	if (bHasValidMesh)
	{
		ModelRadius = (MaxAABB - MinAABB).Size() * 0.5f;
	}

	return ModelRadius;
}

void FObjViewerViewportClient::HandleDragStart(const FRay& Ray)
{
	FHitResult HitResult{};

	AActor* BestActor = nullptr;
	float ClosestDistance = FLT_MAX;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || !Actor->GetRootComponent())
		{
			continue;
		}
		// USceneComponent* RootComp = Actor->GetRootComponent();
		// if (!RootComp->IsA<UPrimitiveComponent>()) continue;

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
