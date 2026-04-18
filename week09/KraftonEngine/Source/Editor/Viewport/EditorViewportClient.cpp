#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/Settings/EditorSettings.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/UI/EditorConsolePanel.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Profiling/PlatformTime.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "Component/CameraComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/World.h"
#include "Viewport/Viewport.h"

UWorld *FEditorViewportClient::GetWorld() const
{
    return GEngine ? GEngine->GetWorld() : nullptr;
}
#include "Collision/RayUtils.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "GameFramework/AActor.h"
#include "ImGui/imgui.h"
#include "Object/Object.h"

void FEditorViewportClient::Initialize(FWindowsWindow *InWindow)
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
    if (!Camera || !Settings)
        return;
    Camera->SetWorldLocation(Settings->InitViewPos);
    Camera->LookAt(Settings->InitLookAt);
}

void FEditorViewportClient::SetViewportType(ELevelViewportType NewType)
{
    if (!Camera)
        return;

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
        Rotation = FVector(0, 90.0f, 0); // Pitch down (positive pitch = look -Z)
        break;
    case ELevelViewportType::Bottom:
        Position = FVector(0, 0, -OrthoDistance);
        Rotation = FVector(0, -90.0f, 0); // Pitch up (negative pitch = look +Z)
        break;
    case ELevelViewportType::Front:
        Position = FVector(OrthoDistance, 0, 0);
        Rotation = FVector(0, 0, 180.0f); // Yaw to look -X
        break;
    case ELevelViewportType::Back:
        Position = FVector(-OrthoDistance, 0, 0);
        Rotation = FVector(0, 0, 0.0f); // Yaw to look +X
        break;
    case ELevelViewportType::Left:
        Position = FVector(0, -OrthoDistance, 0);
        Rotation = FVector(0, 0, 90.0f); // Yaw to look +Y
        break;
    case ELevelViewportType::Right:
        Position = FVector(0, OrthoDistance, 0);
        Rotation = FVector(0, 0, -90.0f); // Yaw to look -Y
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
    if (!bIsActive)
        return;

    TickEditorShortcuts();
    TickInput(DeltaTime);
    TickInteraction(DeltaTime);
}

void FEditorViewportClient::TickEditorShortcuts()
{
    UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine);
    if (!EditorEngine)
    {
        return;
    }

    // PIE 중 ESC로 종료 (UE 동작과 동일)
    if (EditorEngine->IsPlayingInEditor() && InputSystem::Get().GetKeyDown(VK_ESCAPE))
    {
        EditorEngine->RequestEndPlayMap();
    }

    // Ctrl+D — 선택된 액터 복제
    if (SelectionManager && InputSystem::Get().GetKey(VK_CONTROL) && InputSystem::Get().GetKeyDown('D'))
    {
        const TArray<AActor *> ToDuplicate = SelectionManager->GetSelectedActors();
        if (!ToDuplicate.empty())
        {
            TArray<AActor *> NewSelection;
            for (AActor *Src : ToDuplicate)
            {
                if (!Src)
                    continue;
                AActor *Dup = Cast<AActor>(Src->Duplicate(nullptr));
                if (Dup)
                {
                    NewSelection.push_back(Dup);
                }
            }
            SelectionManager->ClearSelection();
            for (AActor *Actor : NewSelection)
            {
                SelectionManager->ToggleSelect(Actor);
            }
        }
    }
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

    const FCameraState &CameraState = Camera->GetCameraState();
    const bool bIsOrtho = CameraState.bIsOrthogonal;

    const float MoveSensitivity = RenderOptions.CameraMoveSensitivity;
    const float CameraSpeed = (Settings ? Settings->CameraSpeed : 10.f) * MoveSensitivity;
    const float PanMouseScale = CameraSpeed * 0.01f;

    if (!bIsOrtho)
    {
        // ── Perspective: 키보드 이동 + 중클릭 로컬 팬 ──
        FVector LocalMove = FVector(0, 0, 0);
        float WorldVerticalMove = 0.0f;

        if (InputSystem::Get().GetKey('W'))
            LocalMove.X += CameraSpeed;
        if (InputSystem::Get().GetKey('A'))
            LocalMove.Y -= CameraSpeed;
        if (InputSystem::Get().GetKey('S'))
            LocalMove.X -= CameraSpeed;
        if (InputSystem::Get().GetKey('D'))
            LocalMove.Y += CameraSpeed;
        if (InputSystem::Get().GetKey('Q'))
            WorldVerticalMove -= CameraSpeed;
        if (InputSystem::Get().GetKey('E'))
            WorldVerticalMove += CameraSpeed;

        LocalMove *= DeltaTime;
        Camera->MoveLocal(LocalMove);
        if (WorldVerticalMove != 0.0f)
        {
            Camera->AddWorldOffset(FVector(0.0f, 0.0f, WorldVerticalMove * DeltaTime));
        }

        // pan 패닝
        if (InputSystem::Get().GetKey(VK_MBUTTON))
        {
            float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
            float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());
            Camera->MoveLocal(FVector(0.0f, -DeltaX * PanMouseScale * 0.05f, DeltaY * PanMouseScale * 0.05f));
        }

        // ── Perspective: 키보드 회전 ──
        FVector Rotation = FVector(0, 0, 0);

        const float RotateSensitivity = RenderOptions.CameraRotateSensitivity;
        const float AngleVelocity = (Settings ? Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
        if (InputSystem::Get().GetKey(VK_UP))
            Rotation.Z -= AngleVelocity;
        if (InputSystem::Get().GetKey(VK_LEFT))
            Rotation.Y -= AngleVelocity;
        if (InputSystem::Get().GetKey(VK_DOWN))
            Rotation.Z += AngleVelocity;
        if (InputSystem::Get().GetKey(VK_RIGHT))
            Rotation.Y += AngleVelocity;

        // ── Perspective: 마우스 우클릭 → 회전 ──
        FVector MouseRotation = FVector(0, 0, 0);
        float MouseRotationSpeed = 0.15f * RotateSensitivity;

        if (InputSystem::Get().GetKey(VK_RBUTTON))
        {
            float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
            float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());

            MouseRotation.Y += DeltaX * MouseRotationSpeed;
            MouseRotation.Z += DeltaY * MouseRotationSpeed;
        }

        Rotation *= DeltaTime;
        Camera->Rotate(Rotation.Y + MouseRotation.Y, Rotation.Z + MouseRotation.Z);
    }
    else
    {
        // ── Orthographic: 마우스 우클릭 드래그 → 평행이동 (Pan) ──
        if (InputSystem::Get().GetKey(VK_RBUTTON))
        {
            float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
            float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());

            // OrthoWidth 기준으로 감도 스케일 (줌 레벨에 비례)
            float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;

            // 카메라 로컬 Right/Up 방향으로 이동
            Camera->MoveLocal(FVector(0, -DeltaX * PanScale, DeltaY * PanScale));
        }
    }

    if (InputSystem::Get().GetKeyUp(VK_SPACE))
        Gizmo->SetNextMode();
}

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
    (void)DeltaTime;

    if (!Camera || !Gizmo || !GetWorld())
    {
        return;
    }

    // 기즈모 비활성화하는 설정. 일단은 PIE 중에도 기즈모가 생김.
    // UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
    // if (EditorEngine && EditorEngine->IsPlayingInEditor())
    //{
    //	Gizmo->Deactivate();
    //	return;
    // }

    Gizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(), Camera->IsOrthogonal(), Camera->GetOrthoWidth());

    // LineTrace 히트 판정용 AxisMask 갱신 (렌더링은 Proxy가 per-viewport로 직접 계산)
    Gizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(RenderOptions.ViewportType, Gizmo->GetMode()));

    // 기즈모 드래그 중에는 마우스가 뷰포트 밖으로 나가도 드래그 종료를 처리해야 함
    if (InputSystem::Get().GetGuiInputState().bUsingMouse && !Gizmo->IsHolding())
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
            // foot zoom 발줌은 절대 delta time를 곱하지 않음. 노치당 이동 거리가 일정해야 하기 때문.
            Camera->MoveLocal(FVector(ScrollNotches * ZoomSpeed * 0.015f, 0.0f, 0.0f));
        }
    }

    // 마우스 좌표를 뷰포트 슬롯 로컬 좌표로 변환
    // (ImGui screen space = 윈도우 클라이언트 좌표)
    POINT MousePoint = InputSystem::Get().GetMousePos();
    MousePoint = Window->ScreenToClientPoint(MousePoint);

    float LocalMouseX = static_cast<float>(MousePoint.x) - ViewportScreenRect.X;
    float LocalMouseY = static_cast<float>(MousePoint.y) - ViewportScreenRect.Y;

    // 커서 숨김 제거: ShowCursor는 전역 레퍼런스 카운터라 멀티 뷰포트에서
    // active 전환 시 GetKeyUp이 처리되지 않아 커서가 영구 숨김될 수 있음

    // FViewport 크기 기준으로 디프로젝션 (슬롯 크기와 동기화됨)
    float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
    float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
    // 성능 향상을 위해 필요할 때만 아래 분기에서 Ray 생성.
    FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
    FHitResult HitResult;

    // 기즈모 hovering 효과를 주석처리해 일단 fps를 개선합니다
    FRayUtils::RaycastComponent(Gizmo, Ray, HitResult);

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
    else if (InputSystem::Get().GetKeyUp(VK_LBUTTON))
    {
        // 드래그 threshold 미달로 DragEnd가 호출되지 않는 경우 처리
        Gizmo->SetPressedOnHandle(false);
    }
}

/**
 * Picking , 마우스 좌클릭 시 Gizmo 핸들과의 충돌을 우선적으로 검사하며 드래그 시작 여부 결정
 *
 * \param Ray
 */
void FEditorViewportClient::HandleDragStart(const FRay &Ray)
{
    FScopeCycleCounter PickCounter; // 시간측정용 카운터 시작

    FHitResult HitResult{};
    // 먼저 Ray와 기즈모의 충돌을 감지하고
    if (FRayUtils::RaycastComponent(Gizmo, Ray, HitResult))
    {
        Gizmo->SetPressedOnHandle(true);
    }
    else
    {
        // 기즈모와 충돌하지 않았다면 월드 BVH를 통해 가장 가까운 프리미티브를 찾음
        AActor *BestActor = nullptr;
        if (UWorld *W = GetWorld())
        {
            W->RaycastPrimitives(Ray, HitResult, BestActor); // BVH 시작
        }

        // 멀티픽킹은 성능을 위해 일단 비활성화
        // bool bCtrlHeld = InputSystem::Get().GetKey(VK_CONTROL);

        if (BestActor == nullptr)
        {
            SelectionManager->ClearSelection();
        }
        else
        {
            // if (bCtrlHeld)
            // {
            // 	SelectionManager->ToggleSelect(BestActor);
            // }
            // else
            {
                SelectionManager->Select(BestActor);
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
    if (!LayoutWindow)
        return;

    const FRect &R = LayoutWindow->GetRect();
    ViewportScreenRect = R;

    // FViewport 리사이즈 요청 (슬롯 크기와 RT 크기 동기화)
    if (Viewport)
    {
        if (R.Width <= 0.0f || R.Height <= 0.0f)
        {
            return;
        }

        uint32 SlotW = static_cast<uint32>(R.Width);
        uint32 SlotH = static_cast<uint32>(R.Height);

        if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight()))
        {
            Viewport->RequestResize(SlotW, SlotH);
        }
    }
}

void FEditorViewportClient::RenderViewportImage()
{
    if (!Viewport || !Viewport->GetSRV())
        return;

    const FRect &R = ViewportScreenRect;
    if (R.Width <= 0 || R.Height <= 0)
        return;

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    ImVec2 Min(R.X, R.Y);
    ImVec2 Max(R.X + R.Width, R.Y + R.Height);

    DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);

    ImU32 BorderColor = 0;
    float BorderThickness = 0.0f;

    switch (PlayState)
    {
    case EEditorViewportPlayState::Paused:
        BorderColor = IM_COL32(255, 230, 80, 255); // Pause: 노랑
        BorderThickness = 4.0f;
        break;

    case EEditorViewportPlayState::Playing:
        BorderColor = IM_COL32(80, 220, 120, 255); // PIE: 초록
        BorderThickness = 4.0f;
        break;

    case EEditorViewportPlayState::Stopped:
    default:
        if (bIsActive)
        {
            BorderColor = IM_COL32(255, 200, 0, 255); // Active: 주황
            BorderThickness = 3.0f;
        }
        break;
    }

    if (BorderThickness > 0.0f)
    {
        const float HalfThickness = BorderThickness * 0.5f;

        DrawList->AddRect(ImVec2(Min.x + HalfThickness, Min.y + HalfThickness),
                          ImVec2(Max.x - HalfThickness, Max.y - HalfThickness), BorderColor, 0.0f, 0, BorderThickness);
    }
}
