// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/Viewport/EditorViewportClient.h"

#include "Core/Logging/LogMacros.h"
#include "Editor/Settings/EditorSettings.h"

#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
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
    UE_LOG(Viewport, Debug, "Editor viewport client initialized.");
}

void FEditorViewportClient::CreateCamera()
{
    DestroyCamera();
    Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
    UE_LOG(Viewport, Debug, "Viewport camera created.");
}

void FEditorViewportClient::DestroyCamera()
{
    if (Camera)
    {
        UE_LOG(Viewport, Debug, "Viewport camera destroyed.");
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
    UE_LOG(Viewport, Debug, "Viewport camera reset to initial editor view.");
}

void FEditorViewportClient::SetViewportType(ELevelViewportType NewType)
{
    if (!Camera)
        return;

    RenderOptions.ViewportType = NewType;
    UE_LOG(Viewport, Debug, "Viewport type changed to %d.", static_cast<int32>(NewType));

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
        SetPilotedActorEditorHelpersVisible(true);
        SelectionManager->RemoveSelectionBlock(PilotedActor);
    }

    PilotedActor = Actor;
    bIsPilotingActor = true;
    UE_LOG(Viewport, Info, "Started piloting actor '%s'.", GetActorDisplayName(Actor).c_str());

    if (SelectionManager)
    {
        SelectionManager->AddSelectionBlock(PilotedActor);
    }

    if (RenderOptions.ViewportType != ELevelViewportType::Perspective)
    {
        SetViewportType(ELevelViewportType::Perspective);
    }

    SetPilotedActorEditorHelpersVisible(false);
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

    SetPilotedActorEditorHelpersVisible(true);
    PilotedActor = nullptr;
    bIsPilotingActor = false;
    UE_LOG(Viewport, Info, "Stopped piloting actor.");

    if (Camera)
    {
        SetViewportType(SavedViewportType);
        Camera->SetWorldLocation(SavedViewLocation);
        Camera->SetRelativeRotation(SavedViewRotation);
    }
}

void FEditorViewportClient::SetPilotedActorEditorHelpersVisible(bool bVisible)
{
    if (!PilotedActor)
    {
        SavedPilotedHelperVisibility.clear();
        return;
    }

    if (!bVisible)
    {
        SavedPilotedHelperVisibility.clear();

        for (UPrimitiveComponent* Primitive : PilotedActor->GetPrimitiveComponents())
        {
            if (!Primitive || !Primitive->IsEditorHelper())
            {
                continue;
            }

            SavedPilotedHelperVisibility.push_back({ Primitive, Primitive->IsVisibleInEditor() });
            Primitive->SetVisibleInEditor(false);
        }
        return;
    }

    for (const FPilotedHelperVisibilityState& SavedState : SavedPilotedHelperVisibility)
    {
        if (SavedState.Component)
        {
            SavedState.Component->SetVisibleInEditor(SavedState.bWasVisibleInEditor);
        }
    }

    SavedPilotedHelperVisibility.clear();
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
    return InputController ? InputController->InputKey(Event) : false;
}

bool FEditorViewportClient::InputAxis(const FViewportAxisEvent& Event)
{
    return InputController ? InputController->InputAxis(Event) : false;
}

bool FEditorViewportClient::InputPointer(const FViewportPointerEvent& Event)
{
    return InputController ? InputController->InputPointer(Event) : false;
}

void FEditorViewportClient::ResetInputState()
{
    if (InputController)
    {
        InputController->ResetInputState();
    }
}

void FEditorViewportClient::ResetKeyboardInputState()
{
    if (InputController)
    {
        InputController->ResetKeyboardInputState();
    }
}

void FEditorViewportClient::BeginInputFrame()
{
    if (InputController)
    {
        InputController->BeginInputFrame();
    }
}

bool FEditorViewportClient::ConsumeContextMenuRequest(FEditorViewportContextMenuRequest& OutRequest)
{
    return InputController && InputController->ConsumeContextMenuRequest(OutRequest);
}
