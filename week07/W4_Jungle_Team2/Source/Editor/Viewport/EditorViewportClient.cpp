#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Core/InputSystem.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Slate/SlateApplication.h"

#include "GameFramework/World.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Object/Object.h"
#include "Editor/Selection/SelectionManager.h"
#include "Runtime/SceneView.h"
#include "EditorUtils.h"
#include "Math/Vector4.h"
#include <algorithm>

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

	// 이전 lerp 타겟이 남아 있으면 리셋 후 카메라가 이동 처리 목적
	NavigationController.ResetTargetLocation();
}

void FEditorViewportClient::DestroyCamera()
{
	bHasCamera = false;
	NavigationController.SetCamera(nullptr); 
	// EndMouseConstrain();
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
	// 우클릭 회전 / 중클릭 팬이 진행 중이면
	// 마우스가 다른 뷰포트로 나가더라도 이 뷰포트가 계속 입력을 독점합니다.
	const bool bActiveOperation = bRightMouseRotating
	                            || bRightMousePanning
	                            || bMiddleMousePanning
	                            || bAltRightMouseDollying
	                            || bBoxSelecting;

	if (State && !State->bHovered && !bActiveOperation)
	{
		return;
	}
	TickInput(DeltaTime);
	NavigationController.Tick(DeltaTime);
	TickInteraction(DeltaTime);
}

// Renderer 에서 사용할 SceneView 설정
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
	// 직교 뷰는 기존 회전값이 LookAt에 간섭하지 않도록 초기화
	Camera.SetRotation(FRotator(0.f, 0.f, 0.f));

	switch (ViewportType)
	{
	case EVT_Perspective:
		Camera.SetProjectionType(EViewportProjectionType::Perspective);
		Camera.ClearCustomLookDir();
        Camera.SetLocation(FVector(5.f, 3.f, 5.f));
		Camera.SetLookAt(FVector(0.f, 0.f, 0.f));
		break;

	// 직교 뷰 (X=Forward, Y=Right, Z=Up)

	case EVT_OrthoTop:			// 위에서 아래 (-Z 방향), 스크린 위 = +X
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 0.f, 1000.f));
		Camera.SetCustomLookDir(FVector(0.f, 0.f, -1.f), FVector(1.f, 0.f, 0.f));
		break;

	case EVT_OrthoBottom:		// 아래에서 위 (+Z 방향), 스크린 위 = +X
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 0.f, -1000.f));
		Camera.SetCustomLookDir(FVector(0.f, 0.f, 1.f), FVector(1.f, 0.f, 0.f));
		break;

	case EVT_OrthoFront:		// 앞(-X)에서 뒤 (+X 방향), 스크린 위 = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(1000.f, 0.f, 0.f));
		Camera.SetCustomLookDir(FVector(-1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoBack:			// 뒤(+X)에서 앞 (-X 방향), 스크린 위 = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(-1000.f, 0.f, 0.f));
		Camera.SetCustomLookDir(FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoLeft:			// 왼쪽(-Y)에서 오른쪽 (+Y 방향), 스크린 위 = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, -1000.f, 0.f));
		Camera.SetCustomLookDir(FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoRight:		// 오른쪽(+Y)에서 왼쪽 (-Y 방향), 스크린 위 = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 1000.f, 0.f));
		Camera.SetCustomLookDir(FVector(0.f, -1.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	default:
		break;
	}

	// 카메라 위치가 바뀐 직후 lerp 타겟을 초기화합니다
	// 미리 쌓인 TargetLocation 이 있으면 다음 Tick 에서 카메라가 이동하는 현상이 생깁니다
	NavigationController.ResetTargetLocation();
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
		// EndMouseConstrain();
		return;
	}

	/*InputSystem::Get().GetGuiInputState().bUsingMouse*/
	if (InputSystem::Get().GetGuiInputState().bUsingKeyboard)
	{
		// EndMouseConstrain();
		return;
	}

	const bool bAltDown = InputSystem::Get().GetKey(VK_MENU);
	const bool bCtrlDown = InputSystem::Get().GetKey(VK_CONTROL);
	const bool bShiftDown = InputSystem::Get().GetKey(VK_SHIFT);

	// Mouse button begin/end state bridge
	if (InputSystem::Get().GetKeyDown(VK_RBUTTON) && !bCtrlDown && !bAltDown && !bShiftDown)
	{
		if (Camera.IsOrthographic())
		{
			// 직교 뷰: 우클릭 드래그 = 팬
			bRightMousePanning = true;
			bFirstMouseMoveAfterRightPanStart = true;
			NavigationController.BeginPanning();
		}
		else
		{
			// 원근 뷰: 우클릭 드래그 = 회전
			bRightMouseRotating = true;
			bFirstMouseMoveAfterRotateStart = true;
			NavigationController.SetRotating(true);
		}

		if (bIsCursorVisible)
		{	
			// 무한루프 방지용
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

	if (InputSystem::Get().GetKeyDown(VK_MBUTTON))
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

	if (InputSystem::Get().GetKeyUp(VK_MBUTTON) && bMiddleMousePanning)
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

	const bool bAnyMouseOperation = bRightMouseRotating
		|| bRightMousePanning
		|| bMiddleMousePanning
		|| bAltRightMouseDollying;

	// if (bAnyMouseOperation)
	// {
	// 	if (!bMouseConstrained)
	// 	{
	// 		BeginMouseConstrain();
	// 	}
	// 	UpdateMouseConstrain();
	// }
	// else if (bMouseConstrained)
	// {
	// 	EndMouseConstrain();
	// }
		
	const float MoveSensitivity = Settings ? Settings->CameraMoveSensitivity : 1.0f;
	const float RotateSensitivity = Settings ? Settings->CameraRotateSensitivity : 1.0f;

	// Keyboard movement while rotating
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

	// Mouse rotate / pan / orbit	
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

	if (bRightMousePanning)
	{
		if (bFirstMouseMoveAfterRightPanStart)
		{
			bFirstMouseMoveAfterRightPanStart = false;
		}
		else
		{
			NavigationController.AddPanInput(MouseDeltaX, -MouseDeltaY);
		}
	}

	if (bAltRightMouseDollying)
	{
		if (bFirstMouseMoveAfterDollyStart)
		{
			bFirstMouseMoveAfterDollyStart = false;
		}
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

	// Toggle projection
	// TODO: 현재는 오류 발생해서 제거
	/*if (InputSystem::Get().GetKeyDown('O'))
	{
		if (Camera.GetProjectionType() == EViewportProjectionType::Perspective)
		{
			Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		}
		else
		{
			Camera.SetProjectionType(EViewportProjectionType::Perspective);
		}
	}*/

	if (InputSystem::Get().GetKeyUp(VK_SPACE) && Gizmo)
	{
		Gizmo->SetNextMode();
	}

	if (InputSystem::Get().GetKeyUp('X') && Gizmo)
	{
		Gizmo->SetWorldSpace(!Gizmo->IsWorldSpace());
	}

	if (InputSystem::Get().GetKeyUp(VK_DELETE))
	{
		DeleteSelectedActors();
	}

	if (InputSystem::Get().GetKeyDown('A') && bCtrlDown && !bAltDown)
	{
		SelectAllActors();
	}

}

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;

	if (!bHasCamera || !Gizmo || !World)
	{
		return;
	}

	// 기즈모 드래그 중이면 마우스가 뷰포트 밖으로 나가도 계속 처리
	const bool bGizmoHolding = Gizmo->IsHolding();

	// Slate 가 입력을 소유 중(스플리터 드래그 등)이면 선택/기즈모 처리를 건너뜁니다.
	// 이 검사가 없으면 스플리터 클릭 시 HandleDragStart → selection clear → 기즈모 소멸이 발생합니다.
	if (FSlateApplication::Get().GetCapturedWidget() != nullptr && !bGizmoHolding)
	{
		return;
	}

	// 마우스가 이 뷰포트 안에 없을 때는 선택/클릭 처리를 하지 않습니다.
	// 기즈모 드래그 중에는 마우스가 뷰포트 밖으로 나가도 계속 업데이트합니다
	if (State && !State->bHovered && !bGizmoHolding)
	{
		return;
	}

	POINT MousePoint = InputSystem::Get().GetMousePos();

	// Marquee(박스 선택) 드래그가 Viewport 영역을 벗어나 ImGui 패널로 넘어가면 즉시 취소합니다.
	// (선택 적용/시각화 모두 중단)
	if (bBoxSelecting)
	{
		const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
		if (!GuiState.IsInViewportHost(MousePoint.x, MousePoint.y))
		{
			bBoxSelecting = false;
			return;
		}
	}

	MousePoint = Window->ScreenToClientPoint(MousePoint);

	// 윈도우 기준 좌표 → 뷰포트 로컬 좌표로 변환
	const float ViewportOffsetX = State ? static_cast<float>(State->Rect.X) : 0.f;
	const float ViewportOffsetY = State ? static_cast<float>(State->Rect.Y) : 0.f;
	const float LocalX = static_cast<float>(MousePoint.x) - ViewportOffsetX;
	const float LocalY = static_cast<float>(MousePoint.y) - ViewportOffsetY;

	if (bBoxSelecting)
	{
		if (LocalX < 0.0f || LocalY < 0.0f || LocalX > WindowWidth || LocalY > WindowHeight)
		{
			bBoxSelecting = false;
			return;
		}
	}

	FRay Ray = Camera.DeprojectScreenToWorld(LocalX, LocalY, WindowWidth, WindowHeight);
	
	// 카메라 위치에 따른 기즈모 스케일 처리
	if (Camera.IsOrthographic())
		Gizmo->ApplyScreenSpaceScalingOrtho(Camera.GetOrthoHeight());
	else
		Gizmo->ApplyScreenSpaceScaling(Camera.GetLocation());

	FHitResult HitResult;
	Gizmo->Raycast(Ray, HitResult);

	const bool bCtrlDown = InputSystem::Get().GetKey(VK_CONTROL);
	const bool bAltDown = InputSystem::Get().GetKey(VK_MENU);
	const bool bShiftDown = InputSystem::Get().GetKey(VK_SHIFT);
	const bool bBoxSelectModifier = bCtrlDown && bAltDown;

	if (InputSystem::Get().GetKeyDown(VK_LBUTTON))
	{
		if (bBoxSelectModifier)
		{
			bBoxSelecting = true;
			BoxSelectStart = POINT{ static_cast<LONG>(LocalX), static_cast<LONG>(LocalY) };
			BoxSelectEnd = BoxSelectStart;
			return;
		}

		HandleDragStart(Ray);
	}
	else if (InputSystem::Get().GetLeftDragging())
	{
		if (bBoxSelecting)
		{
			BoxSelectEnd = POINT{ static_cast<LONG>(LocalX), static_cast<LONG>(LocalY) };
			return;
		}

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
		if (bBoxSelecting)
		{
			HandleBoxSelection();
			bBoxSelecting = false;
			return;
		}

		Gizmo->DragEnd();
	}

	if (bBoxSelecting && InputSystem::Get().GetKeyUp(VK_LBUTTON))
	{
		bBoxSelecting = false;
		return;
	}

	if (Gizmo->IsPressedOnHandle() && InputSystem::Get().GetKeyUp(VK_LBUTTON))
	{
		Gizmo->DragEnd();
	}
}

void FEditorViewportClient::HandleDragStart(const FRay& Ray)
{
	if (!World || !Gizmo) return;

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

		const bool bCtrlHeld = InputSystem::Get().GetKey(VK_CONTROL);
		const bool bShiftHeld = InputSystem::Get().GetKey(VK_SHIFT);

		if (BestActor == nullptr)
		{
			if (!bCtrlHeld && !bShiftHeld && SelectionManager)
			{
				SelectionManager->ClearSelection();
			}
		}
		else
		{
			if (bShiftHeld)
			{
				SelectionManager->AddSelect(BestActor);
			}
			else if (bCtrlHeld)
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

	if (AActor* Primary = SelectionManager->GetPrimarySelection())
	{
		return Primary->GetActorLocation();
	}

	return Camera.GetLocation() + Camera.GetForwardVector() * 300.0f;
}

bool FEditorViewportClient::TryProjectWorldToViewport(const FVector& WorldPos, float& OutViewportX, float& OutViewportY, float& OutDepth) const
{
	const FVector4 Clip = FMatrix::Identity.TransformVector4(FVector4(WorldPos, 1.0f), Camera.GetViewProjectionMatrix());
	if (MathUtil::IsNearlyZero(Clip.W))
	{
		return false;
	}

	const float InvW = 1.0f / Clip.W;
	const float NdcX = Clip.X * InvW;
	const float NdcY = Clip.Y * InvW;
	const float NdcZ = Clip.Z * InvW;
	if (NdcX < -1.0f || NdcX > 1.0f || NdcY < -1.0f || NdcY > 1.0f)
	{
		return false;
	}

	OutViewportX = (NdcX * 0.5f + 0.5f) * WindowWidth;
	OutViewportY = (1.0f - (NdcY * 0.5f + 0.5f)) * WindowHeight;
	OutDepth = NdcZ;
	return true;
}

void FEditorViewportClient::HandleBoxSelection()
{
	if (!SelectionManager || !World)
	{
		return;
	}

	const int32 MinX = std::min(BoxSelectStart.x, BoxSelectEnd.x);
	const int32 MinY = std::min(BoxSelectStart.y, BoxSelectEnd.y);
	const int32 MaxX = std::max(BoxSelectStart.x, BoxSelectEnd.x);
	const int32 MaxY = std::max(BoxSelectStart.y, BoxSelectEnd.y);
	const int32 Width = MaxX - MinX;
	const int32 Height = MaxY - MinY;

	if (Width < 2 || Height < 2)
	{
		return;
	}

	const bool bAddToExisting = InputSystem::Get().GetKey(VK_SHIFT);
	if (!bAddToExisting)
	{
		SelectionManager->ClearSelection();
	}

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || !Actor->GetRootComponent())
		{
			continue;
		}

		float ViewportX = 0.0f;
		float ViewportY = 0.0f;
		float Depth = 0.0f;
		if (!TryProjectWorldToViewport(Actor->GetActorLocation(), ViewportX, ViewportY, Depth))
		{
			continue;
		}

		if (Depth < 0.0f || Depth > 1.0f)
		{
			continue;
		}

		const int32 Px = static_cast<int32>(ViewportX);
		const int32 Py = static_cast<int32>(ViewportY);
		if (Px >= MinX && Px <= MaxX && Py >= MinY && Py <= MaxY)
		{
			SelectionManager->AddSelect(Actor);
		}
	}
}

void FEditorViewportClient::FocusPrimarySelection()
{
	if (!SelectionManager || !bHasCamera)
	{
		return;
	}

	AActor* Primary = SelectionManager->GetPrimarySelection();
	if (!Primary)
	{
		return;
	}

	const FVector Target = Primary->GetActorLocation();

	if (Camera.IsOrthographic())
	{
		const FVector Forward = Camera.GetEffectiveForward().GetSafeNormal();
		float Distance = FVector::DotProduct(Camera.GetLocation() - Target, Forward);
		if (MathUtil::IsNearlyZero(Distance))
		{
			Distance = 1000.0f;
		}
		Camera.SetLocation(Target + Forward * Distance);
	}
	else
	{
		const FVector Forward = Camera.GetForwardVector().GetSafeNormal();
		Camera.SetLocation(Target - Forward * 5.0f);
		Camera.SetLookAt(Target);
	}

	NavigationController.ResetTargetLocation();
}

void FEditorViewportClient::DeleteSelectedActors()
{
	if (!SelectionManager)
	{
		return;
	}

	const TArray<AActor*> SelectedActors = SelectionManager->GetSelectedActors();
	for (AActor* Actor : SelectedActors)
	{
		if (!Actor)
		{
			continue;
		}

		if (UWorld* ActorWorld = Actor->GetWorld())
		{
			ActorWorld->DestroyActor(Actor);
		}
	}

	SelectionManager->ClearSelection();
}

void FEditorViewportClient::SelectAllActors()
{
	if (!SelectionManager || !World)
	{
		return;
	}

	SelectionManager->ClearSelection();
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor)
		{
			continue;
		}
		SelectionManager->AddSelect(Actor);
	}
}

