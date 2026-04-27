#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Profiling/PlatformTime.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"
#include "GameFramework/World.h"
#include "Engine/Runtime/Engine.h"
#include "Math/Vector.h"

UWorld* FEditorViewportClient::GetWorld() const
{
	return GEngine ? GEngine->GetWorld() : nullptr;
}
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Collision/RayUtils.h"
#include "Object/Object.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/AActor.h"
#include "ImGui/imgui.h"
#include "Component/Light/LightComponentBase.h"

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;
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

void FEditorViewportClient::SetViewportType(ELevelViewportType NewType)
{
	if (!Camera) return;

	RenderOptions.ViewportType = NewType;

	if (NewType == ELevelViewportType::Perspective)
	{
		Camera->SetOrthographic(false);
		return;
	}

	// FreeOrthographic: 현재 카메라 위치/회전 유지, 투영만 Ortho로 전환
	if (NewType == ELevelViewportType::FreeOrthographic)
	{
		Camera->SetOrthographic(true);
		return;
	}

	// 고정 방향 Orthographic: 카메라를 프리셋 방향으로 설정
	Camera->SetOrthographic(true);

	constexpr float OrthoDistance = 50.0f;
	FVector Position = FVector(0, 0, 0);
	FVector Rotation = FVector(0, 0, 0); // (Roll, Pitch, Yaw)

	switch (NewType)
	{
	case ELevelViewportType::Top:
		Position = FVector(0, 0, OrthoDistance);
		Rotation = FVector(0, 90.0f, 0);	// Pitch down (positive pitch = look -Z)
		break;
	case ELevelViewportType::Bottom:
		Position = FVector(0, 0, -OrthoDistance);
		Rotation = FVector(0, -90.0f, 0);	// Pitch up (negative pitch = look +Z)
		break;
	case ELevelViewportType::Front:
		Position = FVector(OrthoDistance, 0, 0);
		Rotation = FVector(0, 0, 180.0f);	// Yaw to look -X
		break;
	case ELevelViewportType::Back:
		Position = FVector(-OrthoDistance, 0, 0);
		Rotation = FVector(0, 0, 0.0f);		// Yaw to look +X
		break;
	case ELevelViewportType::Left:
		Position = FVector(0, -OrthoDistance, 0);
		Rotation = FVector(0, 0, 90.0f);	// Yaw to look +Y
		break;
	case ELevelViewportType::Right:
		Position = FVector(0, OrthoDistance, 0);
		Rotation = FVector(0, 0, -90.0f);	// Yaw to look -Y
		break;
	default:
		break;
	}

	Camera->SetRelativeLocation(Position);
	Camera->SetRelativeRotation(Rotation);
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
	if (!bIsActive) return;

	// Camera Focus Animation Update
	if (bIsFocusAnimating && Camera)
	{
		FocusAnimTimer += DeltaTime;
		float Alpha = FocusAnimTimer / FocusAnimDuration;
		if (Alpha >= 1.0f)
		{
			Alpha = 1.0f;
			bIsFocusAnimating = false;
		}

		// SmoothStep curve for better feel
		float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

		FVector NewLoc = FocusStartLoc * (1.0f - SmoothAlpha) + FocusEndLoc * SmoothAlpha;
		
		// Rotation Interpolation (Slerp-like for Rotators)
		FQuat StartQuat = FocusStartRot.ToQuaternion();
		FQuat EndQuat = FocusEndRot.ToQuaternion();
		FQuat BlendedQuat = FQuat::Slerp(StartQuat, EndQuat, SmoothAlpha);
		
		Camera->SetWorldLocation(NewLoc);
		Camera->SetRelativeRotation(FRotator::FromQuaternion(BlendedQuat));
	}

	TickEditorShortcuts();
	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
}

void FEditorViewportClient::TickEditorShortcuts()
{
	UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
	if (!EditorEngine)
	{
		return;
	}

	// PIE 중 ESC로 종료 (UE 동작과 동일)
	if (EditorEngine->IsPlayingInEditor() && InputSystem::Get().GetKeyDown(VK_ESCAPE))
	{
		EditorEngine->RequestEndPlayMap();
	}

	const FGuiInputState& GuiInput = InputSystem::Get().GetGuiInputState();
	const bool bAllowKeyboardInput = !GuiInput.bUsingKeyboard && !ImGui::GetIO().WantTextInput;
	if (!bAllowKeyboardInput)
	{
		return;
	}

	if (SelectionManager && InputSystem::Get().GetKeyDown(VK_DELETE))
	{
		SelectionManager->DeleteSelectedActors();
		return;
	}

	if (!InputSystem::Get().GetKey(VK_CONTROL) && InputSystem::Get().GetKeyDown('X'))
	{
		EditorEngine->ToggleCoordSystem();
		return;
	}

	if (SelectionManager && InputSystem::Get().GetKeyDown('F'))
	{
		AActor* Selected = SelectionManager->GetPrimarySelection();
		if (Selected && Camera)
		{
			FVector TargetLoc = Selected->GetActorLocation();
			FVector CameraForward = Camera->GetForwardVector();
			
			// 1. 현재 상태 백업
			FVector OriginalLoc = Camera->GetWorldLocation();
			FRotator OriginalRot = Camera->GetRelativeRotation();

			// 2. 목표 좌표 계산 (5m 거리)
			float FocusDistance = 5.0f;
			FVector NewCameraLoc = TargetLoc - CameraForward * FocusDistance;
			
			// 3. 임시로 이동하여 정확한 목표 회전값 추출
			Camera->SetWorldLocation(NewCameraLoc);
			Camera->LookAt(TargetLoc);
			FRotator TargetRot = Camera->GetRelativeRotation();

			// 4. 카메라 복구 및 애니메이션 설정
			Camera->SetWorldLocation(OriginalLoc);
			Camera->SetRelativeRotation(OriginalRot);

			bIsFocusAnimating = true;
			FocusAnimTimer = 0.0f;
			FocusStartLoc = OriginalLoc;
			FocusStartRot = OriginalRot;
			FocusEndLoc = NewCameraLoc;
			FocusEndRot = TargetRot;
		}
	}

	if (SelectionManager && InputSystem::Get().GetKey(VK_CONTROL) && InputSystem::Get().GetKeyDown('D'))
	{
		const TArray<AActor*> ToDuplicate = SelectionManager->GetSelectedActors();
		if (!ToDuplicate.empty())
		{
			const FVector DuplicateOffsetStep(0.1f, 0.1f, 0.1f);
			TArray<AActor*> NewSelection;
			int32 DuplicateIndex = 0;
			for (AActor* Src : ToDuplicate)
			{
				if (!Src) continue;
				AActor* Dup = Cast<AActor>(Src->Duplicate(nullptr));
				if (Dup)
				{
					Dup->AddActorWorldOffset(DuplicateOffsetStep * static_cast<float>(DuplicateIndex + 1));
					NewSelection.push_back(Dup);
					++DuplicateIndex;
				}
			}
			SelectionManager->ClearSelection();
			for (AActor* Actor : NewSelection)
			{
				SelectionManager->ToggleSelect(Actor);
			}
			if (EditorEngine->GetGizmo())
			{
				EditorEngine->GetGizmo()->UpdateGizmoTransform();
			}
		}
	}
}

void FEditorViewportClient::SetLightViewOverride(ULightComponentBase* Light)
{
	LightViewOverride = Light;
	PointLightFaceIndex = 0;

	if (Light && SelectionManager)
	{
		SelectionManager->ClearSelection();
	}
}

void FEditorViewportClient::ClearLightViewOverride()
{
	LightViewOverride = nullptr;
}

void FEditorViewportClient::TickInput(float DeltaTime)
{
	if (!Camera)
	{
		return;
	}

	if (IsViewingFromLight()) return;

	if (InputSystem::Get().GetGuiInputState().bUsingKeyboard == true)
	{
		return;
	}

	InputSystem& Input = InputSystem::Get();
	const bool bCtrlHeld = Input.GetKey(VK_CONTROL);

	const FCameraState& CameraState = Camera->GetCameraState();
	const bool bIsOrtho = CameraState.bIsOrthogonal;

	const float MoveSensitivity = RenderOptions.CameraMoveSensitivity;
	const float CameraSpeed = (Settings ? Settings->CameraSpeed : 10.f) * MoveSensitivity;
	const float PanMouseScale = CameraSpeed * 0.01f;

	if (!bIsOrtho)
	{
		// ── Perspective: 키보드 이동 + 중클릭 로컬 팬 ──
		FVector LocalMove = FVector(0, 0, 0);
		float WorldVerticalMove = 0.0f;

		if (!bCtrlHeld && Input.GetKey('W'))
			LocalMove.X += CameraSpeed;
		if (!bCtrlHeld && Input.GetKey('A'))
			LocalMove.Y -= CameraSpeed;
		if (!bCtrlHeld && Input.GetKey('S'))
			LocalMove.X -= CameraSpeed;
		if (!bCtrlHeld && Input.GetKey('D'))
			LocalMove.Y += CameraSpeed;
		if (!bCtrlHeld && Input.GetKey('Q'))
			WorldVerticalMove -= CameraSpeed;
		if (!bCtrlHeld && Input.GetKey('E'))
			WorldVerticalMove += CameraSpeed;

		LocalMove *= DeltaTime;
		Camera->MoveLocal(LocalMove);
		if (WorldVerticalMove != 0.0f)
		{
			Camera->AddWorldOffset(FVector(0.0f, 0.0f, WorldVerticalMove * DeltaTime));
		}

		//pan 패닝
		if (Input.GetKey(VK_MBUTTON))
		{
			float DeltaX = static_cast<float>(Input.MouseDeltaX());
			float DeltaY = static_cast<float>(Input.MouseDeltaY());
			Camera->MoveLocal(FVector(0.0f, -DeltaX * PanMouseScale * 0.05f , DeltaY * PanMouseScale * 0.05f));
		}

		// ── Perspective: 키보드 회전 ──
		FVector Rotation = FVector(0, 0, 0);

		const float RotateSensitivity = RenderOptions.CameraRotateSensitivity;
		const float AngleVelocity = (Settings ? Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
		if (Input.GetKey(VK_UP))
			Rotation.Z -= AngleVelocity;
		if (Input.GetKey(VK_LEFT))
			Rotation.Y -= AngleVelocity;
		if (Input.GetKey(VK_DOWN))
			Rotation.Z += AngleVelocity;
		if (Input.GetKey(VK_RIGHT))
			Rotation.Y += AngleVelocity;

		// ── Perspective: 마우스 우클릭 → 회전 ──
		FVector MouseRotation = FVector(0, 0, 0);
		float MouseRotationSpeed = 0.15f * RotateSensitivity;

		if (Input.GetKey(VK_RBUTTON))
		{
			float DeltaX = static_cast<float>(Input.MouseDeltaX());
			float DeltaY = static_cast<float>(Input.MouseDeltaY());

			MouseRotation.Y += DeltaX * MouseRotationSpeed;
			MouseRotation.Z += DeltaY * MouseRotationSpeed;
		}

		Rotation *= DeltaTime;
		Camera->Rotate(Rotation.Y + MouseRotation.Y, Rotation.Z + MouseRotation.Z);
	}
	else
	{
		// ── Orthographic: 마우스 우클릭 드래그 → 평행이동 (Pan) ──
		if (Input.GetKey(VK_RBUTTON))
		{
			float DeltaX = static_cast<float>(Input.MouseDeltaX());
			float DeltaY = static_cast<float>(Input.MouseDeltaY());

			// OrthoWidth 기준으로 감도 스케일 (줌 레벨에 비례)
			float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;

			// 카메라 로컬 Right/Up 방향으로 이동
			Camera->MoveLocal(FVector(0, -DeltaX * PanScale, DeltaY * PanScale));
		}
	}

	if (Input.GetKeyUp(VK_SPACE))
	{
		Gizmo->SetNextMode();
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			EditorEngine->ApplyTransformSettingsToGizmo();
		}
	}
}

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;

	if (!Camera || !Gizmo || !GetWorld())
	{
		return;
	}

	//기즈모 비활성화하는 설정. 일단은 PIE 중에도 기즈모가 생김.
	//UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
	//if (EditorEngine && EditorEngine->IsPlayingInEditor())
	//{
	//	Gizmo->Deactivate();
	//	return;
	//}

	Gizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(),
		Camera->IsOrthogonal(), Camera->GetOrthoWidth());

	// LineTrace 히트 판정용 AxisMask 갱신 (렌더링은 Proxy가 per-viewport로 직접 계산)
	Gizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(RenderOptions.ViewportType, Gizmo->GetMode()));

	// 기즈모 드래그 중에는 마우스가 뷰포트 밖으로 나가도 드래그 종료를 처리해야 함
	if (InputSystem::Get().GetGuiInputState().bUsingMouse && !Gizmo->IsHolding() && !bIsMarqueeSelecting)
	{
		return;
	}

	const float ZoomSpeed = Settings ? Settings->CameraZoomSpeed : 300.f;

	float ScrollNotches = InputSystem::Get().GetScrollNotches();
	if (ScrollNotches != 0.0f)
	{
		if (Camera->IsOrthogonal())
		{
			float NewWidth = Camera->GetOrthoWidth() - ScrollNotches * ZoomSpeed * DeltaTime;
			Camera->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f));
		}
		else
		{
			//foot zoom 발줌은 절대 delta time를 곱하지 않음. 노치당 이동 거리가 일정해야 하기 때문.
			Camera->MoveLocal(FVector(ScrollNotches * ZoomSpeed*0.015f, 0.0f, 0.0f));
		}
	}

	// 마우스 좌표를 뷰포트 슬롯 로컬 좌표로 변환
	// (ImGui screen space = 윈도우 클라이언트 좌표)
	ImVec2 MousePos = ImGui::GetIO().MousePos;
	float LocalMouseX = MousePos.x - ViewportScreenRect.X;
	float LocalMouseY = MousePos.y - ViewportScreenRect.Y;

	// FViewport 크기 기준으로 디프로젝션 (슬롯 크기와 동기화됨)
	float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
	float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
	
	FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
	FHitResult HitResult;

	// 기즈모 hovering 효과를 주석처리해 일단 fps를 개선합니다
	FRayUtils::RaycastComponent(Gizmo, Ray, HitResult);

	InputSystem& Input = InputSystem::Get();

	if (Input.GetKeyDown(VK_LBUTTON))
	{
		if (Input.GetKey(VK_CONTROL) && Input.GetKey(VK_MENU)) // Ctrl + Alt
		{
			bIsMarqueeSelecting = true;
			MarqueeStartPos = FVector(MousePos.x, MousePos.y,0);
			MarqueeCurrentPos = FVector(MousePos.x, MousePos.y, 0);
		}
		else
		{
			HandleDragStart(Ray);
		}
	}
	else if (Input.GetLeftDragging())
	{
		if (bIsMarqueeSelecting)
		{
			MarqueeCurrentPos = FVector(MousePos.x, MousePos.y, 0);
		}
		else
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
	}
	else if (Input.GetLeftDragEnd())
	{
		if (bIsMarqueeSelecting)
		{
			// Marquee Selection 종료 및 선택 로직 수행
			bIsMarqueeSelecting = false;

			float MinX = (std::min)(MarqueeStartPos.X, MarqueeCurrentPos.X);
			float MaxX = (std::max)(MarqueeStartPos.X, MarqueeCurrentPos.X);
			float MinY = (std::min)(MarqueeStartPos.Y, MarqueeCurrentPos.Y);
			float MaxY = (std::max)(MarqueeStartPos.Y, MarqueeCurrentPos.Y);

			// 사각형 크기가 너무 작으면 일반 클릭으로 간주하거나 무시
			if (std::abs(MaxX - MinX) > 2.0f || std::abs(MaxY - MinY) > 2.0f)
			{
				UWorld* World = GetWorld();
				if (World && SelectionManager)
				{
					if (!Input.GetKey(VK_CONTROL))
					{
						SelectionManager->ClearSelection();
					}

					FMatrix VP = Camera->GetViewProjectionMatrix();
					
					for (AActor* Actor : World->GetActors())
					{
						if (!Actor || !Actor->IsVisible() || Actor->IsA<UGizmoComponent>()) continue;

						FVector WorldPos = Actor->GetActorLocation();
						FVector ClipSpace = VP.TransformPositionWithW(WorldPos);

						// NDX to Screen
						float ScreenX = (ClipSpace.X * 0.5f + 0.5f) * VPWidth + ViewportScreenRect.X;
						float ScreenY = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * VPHeight + ViewportScreenRect.Y;

						if (ScreenX >= MinX && ScreenX <= MaxX && ScreenY >= MinY && ScreenY <= MaxY)
						{
							SelectionManager->ToggleSelect(Actor);
						}
					}
				}
			}
		}
		else
		{
			Gizmo->DragEnd();
		}
	}
	else if (Input.GetKeyUp(VK_LBUTTON))
	{
		// 드래그 threshold 미달로 DragEnd가 호출되지 않는 경우 처리
		Gizmo->SetPressedOnHandle(false);
		bIsMarqueeSelecting = false;
	}
}

/**
 * Picking , 마우스 좌클릭 시 Gizmo 핸들과의 충돌을 우선적으로 검사하며 드래그 시작 여부 결정
 * 
 * \param Ray
 */
void FEditorViewportClient::HandleDragStart(const FRay& Ray)
{
	FScopeCycleCounter PickCounter; //시간측정용 카운터 시작

	FHitResult HitResult{};
	//먼저 Ray와 기즈모의 충돌을 감지하고 
	if (FRayUtils::RaycastComponent(Gizmo, Ray, HitResult))
	{
		Gizmo->SetPressedOnHandle(true);
	}
	else
	{
		//기즈모와 충돌하지 않았다면 월드 BVH를 통해 가장 가까운 프리미티브를 찾음
		AActor* BestActor = nullptr;
		if (UWorld* W = GetWorld())
		{
			W->RaycastPrimitives(Ray, HitResult, BestActor); //BVH 시작
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
				// 컨트롤 키가 눌려있으면 다중 선택 토글
				SelectionManager->ToggleSelect(BestActor);
			}
			else
			{
				if (SelectionManager->GetPrimarySelection() == BestActor)
				{
					if (HitResult.HitComponent)
					{
						SelectionManager->SelectComponent(HitResult.HitComponent);
					}
				}
				else
				{
					// 새로운 선택이면 기본 액터 단위 선택
					SelectionManager->Select(BestActor);
				}
			}
		}
	}

	if (OverlayStatSystem)
	{
		const uint64 PickCycles = PickCounter.Finish();
		const double ElapsedMs = FPlatformTime::ToMilliseconds(PickCycles);
		OverlayStatSystem->RecordPickingAttempt(ElapsedMs);
	}
}

void FEditorViewportClient::UpdateLayoutRect()
{
	if (!LayoutWindow) return;

	const FRect& R = LayoutWindow->GetRect();
	ViewportScreenRect = R;

	// FViewport 리사이즈 요청 (슬롯 크기와 RT 크기 동기화)
	if (Viewport)
	{
		uint32 SlotW = static_cast<uint32>(R.Width);
		uint32 SlotH = static_cast<uint32>(R.Height);
		if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight()))
		{
			Viewport->RequestResize(SlotW, SlotH);
		}
	}
}

void FEditorViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
	if (!Viewport || !Viewport->GetSRV()) return;

	const FRect& R = ViewportScreenRect;
	if (R.Width <= 0 || R.Height <= 0) return;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 Min(R.X, R.Y);
	ImVec2 Max(R.X + R.Width, R.Y + R.Height);

	DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);

	// 활성 뷰포트 테두리 강조
	if (bIsActiveViewport)
	{
		DrawList->AddRect(Min, Max, IM_COL32(255, 200, 0, 200), 0.0f, 0, 2.0f);
	}

	// Marquee Selection 사각형 렌더링
	if (bIsMarqueeSelecting)
	{
		ImDrawList* ForegroundDrawList = ImGui::GetForegroundDrawList();

		ImVec2 RectMin((std::min)(MarqueeStartPos.X, MarqueeCurrentPos.X), (std::min)(MarqueeStartPos.Y, MarqueeCurrentPos.Y));
		ImVec2 RectMax((std::max)(MarqueeStartPos.X, MarqueeCurrentPos.X), (std::max)(MarqueeStartPos.Y, MarqueeCurrentPos.Y));

		ForegroundDrawList->AddRectFilled(RectMin, RectMax, IM_COL32(0, 0, 0, 0)); // 투명 채우기
		ForegroundDrawList->AddRect(RectMin, RectMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 5.0f); // 하얀색 테두리
	}
}

bool FEditorViewportClient::GetCursorViewportPosition(uint32& OutX, uint32& OutY) const
{
	if (!bIsActive) return false;

	ImVec2 MousePos = ImGui::GetIO().MousePos;
	float LocalX = MousePos.x - ViewportScreenRect.X;
	float LocalY = MousePos.y - ViewportScreenRect.Y;

	if (LocalX >= 0 && LocalY >= 0 && LocalX < ViewportScreenRect.Width && LocalY < ViewportScreenRect.Height)
	{
		OutX = static_cast<uint32>(LocalX);
		OutY = static_cast<uint32>(LocalY);
		return true;
	}
	return false;
}
