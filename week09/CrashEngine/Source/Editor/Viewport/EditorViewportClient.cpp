// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/Viewport/EditorViewportClient.h"

#include "Core/RayTypes.h"
#include "Editor/Settings/EditorSettings.h"

#include "Component/CameraComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/World.h"
#include "Viewport/Viewport.h"

#include "Editor/Selection/SelectionManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/DirectionalLightActor.h"
#include "GameFramework/PointLightActor.h"
#include "GameFramework/SpotLightActor.h"
#include "ImGui/imgui.h"
#include "Object/Object.h"

#include "Editor/Input/EditorViewportInputController.h"

UWorld* FEditorViewportClient::GetWorld() const
{
    return GEngine ? GEngine->GetWorld() : nullptr;
}

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow)
{
    (void)InWindow;
    InputController = std::make_unique<FEditorViewportInputController>(this);
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
    {
        return;
    }

    UpdateViewFromPilotedActor();

    if (InputController)
    {
        InputController->HandleInput(DeltaTime);
    }

    ApplyViewToPilotedActor();
}

void FEditorViewportClient::PilotSelectedActor(AActor* Actor)
{
    if (!Actor || !Camera)
    {
        return;
    }

    if (!bIsPilotingActor)
    {
        SavedViewLocation = Camera->GetWorldLocation();
        SavedViewRotation = Camera->GetRelativeRotation();
        SavedViewportType = RenderOptions.ViewportType;
    }

    if (SelectionManager && PilotedActor && PilotedActor != Actor)
    {
        SelectionManager->RemoveSelectionBlock(PilotedActor);
    }

    PilotedActor = Actor;
    bIsPilotingActor = true;

    if (SelectionManager)
    {
        SelectionManager->AddSelectionBlock(PilotedActor);
    }

    if (RenderOptions.ViewportType != ELevelViewportType::Perspective)
    {
        SetViewportType(ELevelViewportType::Perspective);
    }

    UpdateViewFromPilotedActor();
}

void FEditorViewportClient::StopPilotingActor()
{
    if (!bIsPilotingActor)
    {
        return;
    }

    if (SelectionManager && PilotedActor)
    {
        SelectionManager->RemoveSelectionBlock(PilotedActor);
    }

    PilotedActor = nullptr;
    bIsPilotingActor = false;

    if (Camera)
    {
        SetViewportType(SavedViewportType);
        Camera->SetWorldLocation(SavedViewLocation);
        Camera->SetRelativeRotation(SavedViewRotation);
    }
}

void FEditorViewportClient::UpdateViewFromPilotedActor()
{
    if (!bIsPilotingActor || !PilotedActor || !Camera)
    {
        return;
    }

    Camera->SetWorldLocation(PilotedActor->GetActorLocation());
    Camera->SetRelativeRotation(PilotedActor->GetActorRotation());
}

void FEditorViewportClient::ApplyViewToPilotedActor()
{
    if (!bIsPilotingActor || !PilotedActor || !Camera)
    {
        return;
    }

    PilotedActor->SetActorLocation(Camera->GetWorldLocation());
    PilotedActor->SetActorRotation(Camera->GetRelativeRotation());
}

FString FEditorViewportClient::GetActorDisplayName(const AActor* Actor) const
{
    if (!Actor)
    {
        return FString();
    }

    FString ActorName = Actor->GetFName().ToString();
    if (!ActorName.empty())
    {
        return ActorName;
    }

    return Actor->GetClass() ? FString(Actor->GetClass()->GetName()) : FString("Actor");
}

FString FEditorViewportClient::GetPilotedActorDisplayName() const
{
    return GetActorDisplayName(PilotedActor);
}

FString FEditorViewportClient::GetPilotOverlayText() const
{
    if (!IsPilotingActor())
    {
        return FString();
    }

    return "[ Pilot Active: " + GetPilotedActorDisplayName() + " ]";
}

FString FEditorViewportClient::GetPilotHintText() const
{
    if (!IsPilotingActor() || !PilotedActor)
    {
        return FString();
    }

    if (PilotedActor->IsA<ASpotLightActor>())
    {
        return "Moving and rotating the viewport edits the light position and direction.";
    }

    if (PilotedActor->IsA<ADirectionalLightActor>())
    {
        return "Rotation controls light direction.";
    }

    if (PilotedActor->IsA<APointLightActor>())
    {
        return "Position changes the light origin; rotation has no lighting effect.";
    }

    return FString();
}

AActor* FEditorViewportClient::PickActorAtScreenPoint(float ScreenX, float ScreenY) const
{
    if (!Camera || !Viewport)
    {
        return nullptr;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    const float LocalMouseX = ScreenX - ViewportScreenRect.X;
    const float LocalMouseY = ScreenY - ViewportScreenRect.Y;
    if (LocalMouseX < 0.0f || LocalMouseY < 0.0f ||
        LocalMouseX > ViewportScreenRect.Width || LocalMouseY > ViewportScreenRect.Height)
    {
        return nullptr;
    }

    const float VPWidth = static_cast<float>(Viewport->GetWidth());
    const float VPHeight = static_cast<float>(Viewport->GetHeight());
    if (VPWidth <= 0.0f || VPHeight <= 0.0f)
    {
        return nullptr;
    }

    const FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
    FHitResult HitResult{};
    AActor* BestActor = nullptr;
    const_cast<UWorld*>(World)->RaycastEditorPicking(Ray, HitResult, BestActor);
    return BestActor;
}

void FEditorViewportClient::UpdateLayoutRect()
{
    if (!LayoutWindow)
        return;

    const FRect& PaneRect = LayoutWindow->GetRect();
    ViewportFrameRect = PaneRect;

    const float ToolbarHeight = (PaneToolbarHeight > 0.0f) ? PaneToolbarHeight : 0.0f;
    ViewportScreenRect = PaneRect;
    ViewportScreenRect.Y += ToolbarHeight;
    ViewportScreenRect.Height -= ToolbarHeight;

    // FViewport 리사이즈 요청 (렌더 타깃은 툴바를 제외한 실제 렌더 영역 크기와 동기화)
    if (Viewport)
    {
        if (ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
        {
            return;
        }

        uint32 SlotW = static_cast<uint32>(ViewportScreenRect.Width);
        uint32 SlotH = static_cast<uint32>(ViewportScreenRect.Height);

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

    const FRect& R = ViewportScreenRect;
    if (R.Width <= 0 || R.Height <= 0)
        return;

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    DrawList->AddImage((ImTextureID)Viewport->GetSRV(), ImVec2(R.X, R.Y), ImVec2(R.X + R.Width, R.Y + R.Height));
}

void FEditorViewportClient::RenderViewportBorder()
{
    const FRect& R = ViewportScreenRect;
    if (R.Width <= 0 || R.Height <= 0)
        return;

    ImU32 BorderColor = 0;
    float BorderThickness = 0.0f;

    switch (PlayState)
    {
    case EEditorViewportPlayState::Paused:
        BorderColor = IM_COL32(255, 230, 80, 255);
        BorderThickness = 4.0f;
        break;
    case EEditorViewportPlayState::Playing:
        BorderColor = IM_COL32(80, 220, 120, 255); // green
        BorderThickness = 4.0f;
        break;
    case EEditorViewportPlayState::Stopped:
    default:
        if (bIsActive)
        {
            BorderColor = IM_COL32(255, 100, 0, 255);
            BorderThickness = 4.0f;
        }
        break;
    }

    if (BorderThickness <= 0.0f)
        return;

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const float HalfThickness = BorderThickness * 0.5f;

    DrawList->AddRect(
        ImVec2(R.X + HalfThickness, R.Y + HalfThickness),
        ImVec2(R.X + R.Width - HalfThickness, R.Y + R.Height - HalfThickness),
        BorderColor,
        0.0f,
        0,
        BorderThickness);
}

bool FEditorViewportClient::InputKey(const FViewportKeyEvent& Event)
{
    if (Event.Key < 0 || Event.Key >= 256)
    {
        return false;
    }

    CurrentInput.Modifiers = Event.Modifiers;

    switch (Event.Type)
    {
    case EKeyEventType::Pressed:
        CurrentInput.KeyPressed[Event.Key] = true;
        CurrentInput.KeyDown[Event.Key] = true;
        break;

    case EKeyEventType::Released:
        CurrentInput.KeyReleased[Event.Key] = true;
        CurrentInput.KeyDown[Event.Key] = false;
        break;

    case EKeyEventType::Repeat:
        CurrentInput.KeyRepeated[Event.Key] = true;
        CurrentInput.KeyDown[Event.Key] = true;
        break;
    }

    // 입력 이벤트는 프레임 상태로 누적하고, 실제 해석은 editor viewport tool 계층에서 수행합니다.

    return false;
}

bool FEditorViewportClient::InputAxis(const FViewportAxisEvent& Event)
{
    CurrentInput.Modifiers = Event.Modifiers;

    switch (Event.Axis)
    {
    case EInputAxis::MouseX:
        CurrentInput.MouseAxisDelta.x += static_cast<LONG>(Event.Value);
        break;

    case EInputAxis::MouseY:
        CurrentInput.MouseAxisDelta.y += static_cast<LONG>(Event.Value);
        break;

    case EInputAxis::MouseWheel:
        CurrentInput.MouseWheelNotches += Event.Value;
        break;

    default:
        break;
    }

    // 입력 이벤트는 프레임 상태로 누적하고, 실제 해석은 editor viewport tool 계층에서 수행합니다.

    return false;
}

bool FEditorViewportClient::InputPointer(const FViewportPointerEvent& Event)
{
    CurrentInput.Modifiers = Event.Modifiers;

    CurrentInput.MouseLocalPos = Event.LocalPos;
    CurrentInput.MouseClientPos = Event.ClientPos;
    CurrentInput.MouseScreenPos = Event.ScreenPos;

    switch (Event.Button)
    {
    case EPointerButton::Left:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bLeftPressed = true;
            CurrentInput.bLeftDown = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bLeftReleased = true;
            CurrentInput.bLeftDown = false;
        }
        break;

    case EPointerButton::Right:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bRightPressed = true;
            CurrentInput.bRightDown = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bRightReleased = true;
            CurrentInput.bRightDown = false;
        }
        break;

    case EPointerButton::Middle:
        if (Event.Type == EPointerEventType::Pressed)
        {
            CurrentInput.bMiddlePressed = true;
            CurrentInput.bMiddleDown = true;
        }
        else if (Event.Type == EPointerEventType::Released)
        {
            CurrentInput.bMiddleReleased = true;
            CurrentInput.bMiddleDown = false;
        }
        break;

    case EPointerButton::None:
    default:
        break;
    }

    // 입력 이벤트는 프레임 상태로 누적하고, 실제 해석은 editor viewport tool 계층에서 수행합니다.

    return false;
}

void FEditorViewportClient::BeginInputFrame()
{
    for (int32 VK = 0; VK < 256; ++VK)
    {
        CurrentInput.KeyPressed[VK] = false;
        CurrentInput.KeyReleased[VK] = false;
        CurrentInput.KeyRepeated[VK] = false;
    }

    CurrentInput.MouseAxisDelta = { 0, 0 };
    CurrentInput.MouseWheelNotches = 0.0f;

    CurrentInput.bLeftPressed = false;
    CurrentInput.bLeftReleased = false;

    CurrentInput.bRightPressed = false;
    CurrentInput.bRightReleased = false;

    CurrentInput.bMiddlePressed = false;
    CurrentInput.bMiddleReleased = false;
}
