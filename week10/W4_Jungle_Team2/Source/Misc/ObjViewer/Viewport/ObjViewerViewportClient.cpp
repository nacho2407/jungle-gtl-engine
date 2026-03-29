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

namespace
{
	void MoveCameraLocal(FViewportCamera& Camera, const FVector& LocalDelta)
	{
		const FVector WorldDelta =
			Camera.GetForwardVector() * LocalDelta.X +
			Camera.GetRightVector() * LocalDelta.Y +
			Camera.GetUpVector() * LocalDelta.Z;
		Camera.SetLocation(Camera.GetLocation() + WorldDelta);
	}

	// 짐벌 락 해결을 위해 쿼터니언 방식으로 교체
	void LookAt(FViewportCamera& Camera, const FVector& Target)
	{
		FVector Forward = (Target - Camera.GetLocation()).GetSafeNormal();
		if (Forward.IsNearlyZero()) return;

		FVector UpRef = FVector::UpVector;	
		if (std::abs(Forward.DotProduct(UpRef)) > 0.99f)
		{
			UpRef = FVector(1.0f, 0.0f, 0.0f);
		}

		FVector Right = FVector::CrossProduct(UpRef, Forward).GetSafeNormal();
		FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();

		FMatrix RotMat = FMatrix::Identity;
		RotMat.SetAxes(Forward, Right, Up);

		FQuat QuatRot(RotMat);
		QuatRot.Normalize();
		Camera.SetRotation(QuatRot);
	}
}

void FObjViewerViewportClient::Initialize(FWindowsWindow* InWindow)
{
    Window = InWindow;
    // UE_LOG("Hello ZZup Engine! %d", 2026);
}

void FObjViewerViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = new FViewportCamera();
}

void FObjViewerViewportClient::DestroyCamera()
{
	if (Camera)
	{
		delete Camera;
		Camera = nullptr;
	}
}

// 카메라를 초기 위치로 이동시킨다.
void FObjViewerViewportClient::ResetCamera()
{
    if (!Camera || !Settings)
        return;

	ObjViewerModelInfo ModelInfo = GetModelInfo();
	float ModelRadius = ModelInfo.ModelRadius;
	FVector Center = ModelInfo.ModelCenter;
	FVector Offset(ModelRadius, ModelRadius, ModelRadius);

	float DistanceMultiplier = 4.0f;
    FVector CameraPos = Center + (Offset * DistanceMultiplier);

	Camera->SetLocation(CameraPos);
	LookAt(*Camera, Center);
}

// 카메라를 초기 위치로 애니메이션과 함께 부드럽게 이동시킨다.
void FObjViewerViewportClient::ResetCameraSmoothly()
{
    if (!Camera || !Settings)
        return;

	ObjViewerModelInfo ModelInfo = GetModelInfo();
	float ModelRadius = ModelInfo.ModelRadius;
	FVector Center = ModelInfo.ModelCenter;
	FVector Offset(ModelRadius, ModelRadius, ModelRadius);

	float DistanceMultiplier = 3.0f;
	FVector TargetPos = Center + (Offset * DistanceMultiplier);

    // 1. 목표 회전값(Target Rotation)을 쿼터니언으로 미리 계산
    FVector Forward = (Center - TargetPos).GetSafeNormal();
    FVector UpRef = FVector::UpVector;
    if (std::abs(Forward.DotProduct(UpRef)) > 0.99f) UpRef = FVector(1.0f, 0.0f, 0.0f);
    
    FVector Right = FVector::CrossProduct(UpRef, Forward).GetSafeNormal();
    FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();
    
    FMatrix RotMat = FMatrix::Identity;
    RotMat.SetAxes(Forward, Right, Up);
    FQuat TargetRot(RotMat);
    TargetRot.Normalize();

    // 2. 현재 상태를 '시작점'으로, 계산된 값을 '목표점'으로 저장
    CameraGUIParams.ResetStartLocation = Camera->GetLocation();
    CameraGUIParams.ResetStartRotation = Camera->GetRotation();
    CameraGUIParams.ResetStartOrbitPivot = OrbitPivot;
    CameraGUIParams.ResetStartOrbitDistance = OrbitDistance;
    CameraGUIParams.ResetTargetLocation = TargetPos;
    CameraGUIParams.ResetTargetRotation = TargetRot;
    CameraGUIParams.ResetTargetOrbitPivot = Center;
    CameraGUIParams.ResetTargetOrbitDistance = (TargetPos - Center).Size();

    // 3. 애니메이션 시작 트리거 ON
    CameraGUIParams.bIsResettingCamera = true;
    CameraGUIParams.ResetCameraProgress = 0.0f;
}

// 모델의 크기와 비례하게 카메라의 이동 범위를 제한한다.
void FObjViewerViewportClient::ClampCameraPosition()
{
	if (!Camera || !World) return;

    // 허용되는 최대 거리를 설정
	ObjViewerModelInfo ModelInfo = GetModelInfo();
	float ModelRadius = ModelInfo.ModelRadius;
	FVector ModelCenter = ModelInfo.ModelCenter;
    float MaxAllowedDistance = ModelRadius * 6.0f;

    // 카메라 위치를 확인하고 이동 범위 제한(Clamp)을 적용
    FVector CamPos = Camera->GetLocation();
    float CurrentDistance = (CamPos - ModelCenter).Size();
    if (CurrentDistance > MaxAllowedDistance)
    {
        FVector Direction = CamPos - ModelCenter;
        Direction.Normalize();

        FVector ClampedPosition = ModelCenter + Direction * MaxAllowedDistance;
        Camera->SetLocation(ClampedPosition);
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
		Camera->OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
	}
}

void FObjViewerViewportClient::Tick(float DeltaTime)
{
	TickCameraReset(DeltaTime);
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

	const float RotateSensitivity = Settings ? Settings->CameraRotateSensitivity : 1.f;

	// Mouse sensitivity is degrees per pixel (do not multiply by DeltaTime)
	float MouseRotationSpeed = 0.5f * RotateSensitivity;
	if (bIsOrbiting && InputSystem::Get().GetRightDragging())
{
    float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
    float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());

    // 1. Quat -> Rotator 역변환의 부작용을 피하기 위해, Forward 벡터에서 직접 Pitch/Yaw 추출
    FVector Forward = Camera->GetForwardVector().GetSafeNormal();
    float CurrentPitch = MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.0f, 1.0f)));
    float CurrentYaw = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));

    // 2. 마우스 입력값 적용 및 제한 (Clamp)
    float TargetPitch = MathUtil::Clamp(CurrentPitch - DeltaY * MouseRotationSpeed, -89.0f, 89.0f);
    float TargetYaw = CurrentYaw + DeltaX * MouseRotationSpeed;

    // 3. 구면 좌표계를 다시 방향 벡터(NewForward)로 변환
    float PitchRad = MathUtil::DegreesToRadians(TargetPitch);
    float YawRad = MathUtil::DegreesToRadians(TargetYaw);
    
    FVector NewForward(
        std::cos(PitchRad) * std::cos(YawRad),
        std::cos(PitchRad) * std::sin(YawRad),
        std::sin(PitchRad)
    );
    NewForward = NewForward.GetSafeNormal();

    // 4. 새로운 방향을 기반으로 직교하는 Right, Up 벡터 계산
    FVector NewRight = FVector::CrossProduct(FVector::UpVector, NewForward).GetSafeNormal();
    if (NewRight.IsNearlyZero()) { NewRight = Camera->GetRightVector(); }
    FVector NewUp = FVector::CrossProduct(NewForward, NewRight).GetSafeNormal();

    // 5. 회전 행렬을 통해 아주 깨끗한 Quaternion 생성
    FMatrix RotMat = FMatrix::Identity;
    RotMat.SetAxes(NewForward, NewRight, NewUp);

    FQuat NewRotation(RotMat);
    NewRotation.Normalize();

    // 6. 카메라에 최종 적용
    Camera->SetRotation(NewRotation);
    Camera->SetLocation(OrbitPivot - NewForward * OrbitDistance);
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

	if (!Camera || !World)
	{
		return;
	}

	// 수정된 부분: UI(ImGui)가 마우스를 차지했을 때의 예외 처리
	if (InputSystem::Get().GetGuiInputState().bUsingMouse)
	{
		// 뷰포트 조작 중 마우스가 UI로 넘어갔는데 커서가 꺼져있다면 강제로 복구합니다.
		if (!bIsCursorVisible)
		{
			while (ShowCursor(TRUE) < 0);
			bIsCursorVisible = true;
			
			// 눌려있던 상태 변수들도 강제로 초기화
			CursorOverlayState.bPressed = false;
			CursorOverlayState.TargetRadius = 0.0f;
			bIsOrbiting = false;
		}
		return;
	}

	// 뷰포트를 더블 클릭하면 카메라가 모델을 향해 부드럽게 리셋된다.
	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        ResetCameraSmoothly();
    }

	// Viewer에서는 Zoom이 일어나는 대신 카메라를 전후로 이동한다.
    const float ForwardSpeed = Settings ? Settings->CameraForwardSpeed : 500.f;
    float ScrollNotches = InputSystem::Get().GetScrollNotches();
    if (ScrollNotches != 0.0f)
    {
		ObjViewerModelInfo ModelInfo = GetModelInfo();
		float ModelRadius = ModelInfo.ModelRadius;
		float DynamicForwardSpeed = ForwardSpeed * ModelRadius;

		if (Camera->GetProjectionType() == EViewportProjectionType::Orthographic)
		{
			float NewHeight = Camera->GetOrthoHeight() - ScrollNotches * DynamicForwardSpeed * DeltaTime;
			Camera->SetOrthoHeight(MathUtil::Clamp(NewHeight, 0.1f, 1000.0f));
		}
		else
		{
			float MoveAmount = ScrollNotches * DynamicForwardSpeed * DeltaTime;
			MoveCameraLocal(*Camera, FVector(MoveAmount, 0.0f, 0.0f));
		}
	}

    POINT MousePoint = InputSystem::Get().GetMousePos();
    MousePoint = Window->ScreenToClientPoint(MousePoint);

    // 위젯이 없는 뷰포트 영역(Central Node)을 기준으로 마우스 좌표 보정
    float LocalMouseX = static_cast<float>(MousePoint.x) - ViewportX;
    float LocalMouseY = static_cast<float>(MousePoint.y) - ViewportY;

    // 보정된 마우스 좌표로 Raycast 수행
    FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, WindowWidth, WindowHeight);
    CursorOverlayState.ScreenX = LocalMouseX;
    CursorOverlayState.ScreenY = LocalMouseY;

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
		ObjViewerModelInfo ModelInfo = GetModelInfo();
		FVector CameraLocation = Camera->GetLocation();
		FVector CameraDirection = Camera->GetForwardVector();

		// 현재 카메라 위치에서 OrbitDistance만큼 떨어진 곳을 회전축(Pivot)으로 삼는다.
		OrbitDistance = (CameraLocation - ModelInfo.ModelCenter).Size();
		if (OrbitDistance <= MathUtil::Epsilon)
		{
			OrbitDistance = std::max(10.0f, ModelInfo.ModelRadius * 2.0f);
		}
		OrbitPivot = CameraLocation + (CameraDirection * OrbitDistance);

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
		FVector OldPos = Camera->GetLocation();
		float OldDist = (OldPos - Center).Size();

		// MoveLocal은 로컬 좌표계를 사용한다. (X: 전진, Y: 우측, Z: 상단)
		// 마우스를 우측(+)으로 끌면 화면은 좌측(-)으로, 마우스를 아래(+)로 끌면 화면은 위(+)로 이동합니다.
		FVector Move(0.0f, -DeltaX * PanSpeed, DeltaY * PanSpeed);
		MoveCameraLocal(*Camera, Move);

		// 파고들기 방지: 이동 후 거리가 줄어들었다면 원래 거리로 밀어냄
		FVector NewPos = Camera->GetLocation();
		float NewDist = (NewPos - Center).Size();

		if (NewDist < OldDist)
		{
			FVector Dir = NewPos - Center;
			Dir.Normalize();

			// 거리를 OldDist로 강제 복구
			Camera->SetLocation(Center + Dir * OldDist);
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
	ObjViewerModelInfo ModelInfo = GetModelInfo();

	float ModelRadius = ModelInfo.ModelRadius;
    FVector ModelCenter = ModelInfo.ModelCenter;
    FVector CamPos = Camera->GetLocation();
    FVector CamFwd = Camera->GetForwardVector();
    FVector CamRight = Camera->GetRightVector();
    FVector CamUp = Camera->GetUpVector();

    FVector ToObject = ModelCenter - CamPos;
    float DistZ = ToObject.DotProduct(CamFwd);
    float DistX = ToObject.DotProduct(CamRight);
    float DistY = ToObject.DotProduct(CamUp);

	// 피타고라스의 정리를 이용해 '화면 중앙에서의 2D 직선 거리' 계산
	float CurrentPanDist = std::sqrt(DistX * DistX + DistY * DistY);

	// 직교 투영, 원근 투영에 따라 화면을 벗어나지 않도록 허용할 최대 이탈 거리 계산
	float MaxAllowedPan = 0.0f;
	if (Camera->GetProjectionType() == EViewportProjectionType::Orthographic)
	{
		MaxAllowedPan = Camera->GetOrthoHeight() * 0.5f * 0.8f;
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
		Camera->SetLocation(CamPos + Correction);
	}
}

// 모델의 크기에 따라 카메라 조작 속도를 변경하도록 ModelRadius를 계산한다.
ObjViewerModelInfo FObjViewerViewportClient::GetModelInfo()
{
	FVector MinAABB(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxAABB(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool bHasValidMesh = false;

	ObjViewerModelInfo ModelInfo;

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
	ModelInfo.ModelRadius = 2.0f;
    if (bHasValidMesh)
    {
        ModelInfo.ModelRadius = (MaxAABB - MinAABB).Size() * 0.5f; 
    }
	if (bHasValidMesh)
	{
		ModelInfo.ModelCenter = (MaxAABB + MinAABB) / 2.0f;
	}
	else
	{
		ModelInfo.ModelCenter = FVector::ZeroVector;
	}

	return ModelInfo;
}

void FObjViewerViewportClient::SetViewportRect(float InX, float InY, float InWidth, float InHeight)
{
    ViewportX = InX;
    ViewportY = InY;
    
    // 최소 1 픽셀 크기를 보장하여 0으로 나누기(Divide by Zero) 에러 방지
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

void FObjViewerViewportClient::TickCameraReset(float DeltaTime)
{
	FCameraGUIParameters& params = CameraGUIParams;

    if (!params.bIsResettingCamera || !Camera) return;
	
    // 진행도 업데이트 (0.0 ~ 1.0 Clamp)
    params.ResetCameraProgress += DeltaTime * params.ResetCameraSpeed;
    if (params.ResetCameraProgress >= 1.0f)
    {
        params.ResetCameraProgress = 1.0f;
        params.bIsResettingCamera = false; // 애니메이션 종료
    }

    // 부드러운 가감속을 위한 Smoothstep 공식 적용
    float t = params.ResetCameraProgress;
    float Alpha = t * t * (3.0f - 2.0f * t);

    // 위치 보간 (Lerp)
    FVector CurrentLocation = params.ResetStartLocation + (params.ResetTargetLocation - params.ResetStartLocation) * Alpha;
    Camera->SetLocation(CurrentLocation);

    // 회전 보간 (Slerp - 구면 선형 보간)
    FQuat CurrentRotation = FQuat::Slerp(params.ResetStartRotation, params.ResetTargetRotation, Alpha);
    Camera->SetRotation(CurrentRotation);

    // 회전 중심축(Pivot) 보간
    OrbitPivot = params.ResetStartOrbitPivot + (params.ResetTargetOrbitPivot - params.ResetStartOrbitPivot) * Alpha;
    OrbitDistance = params.ResetStartOrbitDistance + (params.ResetTargetOrbitDistance - params.ResetStartOrbitDistance) * Alpha;
}