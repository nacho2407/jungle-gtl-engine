#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Input/EditorViewportInputContexts.h"
#include "Editor/Input/EditorViewportInputMapping.h"
#include "Editor/Viewport/EditorBoxSelectionService.h"
#include "Editor/Viewport/EditorViewportNavigationController.h"
#include "Editor/Viewport/EditorPickingService.h"
#include "Editor/Viewport/EditorViewportPIEController.h"
#include "Editor/Viewport/EditorTransformInteraction.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Slate/SlateApplication.h"
#include "EditorEngine.h"

#include "GameFramework/World.h"
#include "GameFramework/PlayerController.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Object/Object.h"
#include "Object/ActorIterator.h"
#include "Editor/Selection/SelectionManager.h"
#include "Runtime/SceneView.h"
#include "EditorUtils.h"
#include "Math/Vector4.h"
#include "Slate/SWidget.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
bool IsRuntimeGameInputCaptured()
{
	return GEngine
		&& GEngine->GetRuntimeInputMode() == ERuntimeInputMode::GameOnly
		&& GEngine->IsRuntimeCursorLocked();
}

bool IsPassiveEditorViewportHover(const FViewportInputContext& Context)
{
	if (!Context.bHovered || Context.bCaptured || Context.bRelativeMouseMode)
	{
		return false;
	}

	if (Context.Frame.WheelNotches != 0.0f ||
		Context.Frame.bLeftDragging ||
		Context.Frame.bMiddleDragging ||
		Context.Frame.bRightDragging)
	{
		return false;
	}

	if (!Context.Events.empty())
	{
		return false;
	}

	for (int32 Key = 0; Key < 256; ++Key)
	{
		if (Context.Frame.IsDown(Key))
		{
			return false;
		}
	}

	return !Context.HasEvent(EInputEventType::PointerDragStarted) &&
		!Context.HasEvent(EInputEventType::PointerDragEnded);
}
}

static FString TrimEditorActorName(const FString& Name)
{
	size_t Begin = 0;
	while (Begin < Name.size() && std::isspace(static_cast<unsigned char>(Name[Begin])) != 0)
	{
		++Begin;
	}

	size_t End = Name.size();
	while (End > Begin && std::isspace(static_cast<unsigned char>(Name[End - 1])) != 0)
	{
		--End;
	}

	if (Begin >= End)
	{
		return "";
	}
	return Name.substr(Begin, End - Begin);
}

static bool ParseEditorNameNumber(const FString& Text, int32& OutNumber)
{
	if (Text.empty())
	{
		return false;
	}

	int32 Value = 0;
	for (char Ch : Text)
	{
		if (!std::isdigit(static_cast<unsigned char>(Ch)))
		{
			return false;
		}
		Value = Value * 10 + (Ch - '0');
	}

	OutNumber = Value;
	return true;
}

static bool SplitEditorGeneratedNameSuffix(const FString& Name, FString& OutBaseName, int32& OutNumber)
{
	const FString TrimmedName = TrimEditorActorName(Name);
	if (TrimmedName.empty())
	{
		return false;
	}

	if (TrimmedName.back() == ')')
	{
		const size_t OpenParen = TrimmedName.rfind(" (");
		if (OpenParen != FString::npos && OpenParen + 2 < TrimmedName.size() - 1)
		{
			const FString NumberText = TrimmedName.substr(OpenParen + 2, TrimmedName.size() - OpenParen - 3);
			if (ParseEditorNameNumber(NumberText, OutNumber))
			{
				OutBaseName = TrimEditorActorName(TrimmedName.substr(0, OpenParen));
				return !OutBaseName.empty();
			}
		}
	}

	size_t NumberBegin = TrimmedName.size();
	while (NumberBegin > 0 && std::isdigit(static_cast<unsigned char>(TrimmedName[NumberBegin - 1])) != 0)
	{
		--NumberBegin;
	}

	if (NumberBegin == TrimmedName.size() || NumberBegin == 0 || TrimmedName[NumberBegin - 1] != '_')
	{
		return false;
	}

	if (ParseEditorNameNumber(TrimmedName.substr(NumberBegin), OutNumber))
	{
		OutBaseName = TrimEditorActorName(TrimmedName.substr(0, NumberBegin - 1));
		return !OutBaseName.empty();
	}
	return false;
}

static FString StripEditorGeneratedNameSuffixes(const FString& Name)
{
	FString BaseName = TrimEditorActorName(Name);
	for (;;)
	{
		FString NextBaseName;
		int32 IgnoredNumber = 0;
		if (!SplitEditorGeneratedNameSuffix(BaseName, NextBaseName, IgnoredNumber))
		{
			return BaseName;
		}
		BaseName = NextBaseName;
	}
}

static bool IsEditorActorNameTaken(UWorld* World, AActor* TargetActor, const FString& CandidateName)
{
	if (!World || CandidateName.empty())
	{
		return false;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || Actor == TargetActor)
		{
			continue;
		}
		if (Actor->GetFName() == FName(CandidateName))
		{
			return true;
		}
	}
	return false;
}

static FString MakeUniqueEditorDuplicateActorName(UWorld* World, AActor* TargetActor, const FString& RequestedName)
{
	FString BaseName = StripEditorGeneratedNameSuffixes(RequestedName);
	if (BaseName.empty())
	{
		BaseName = TargetActor && TargetActor->GetTypeInfo() ? TargetActor->GetTypeInfo()->name : "Actor";
	}

	int32 HighestSuffix = 0;
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || Actor == TargetActor)
		{
			continue;
		}

		const FString ExistingName = TrimEditorActorName(Actor->GetFName().ToString());
		if (ExistingName == BaseName)
		{
			continue;
		}

		FString ExistingBaseName;
		int32 ExistingSuffix = 0;
		if (SplitEditorGeneratedNameSuffix(ExistingName, ExistingBaseName, ExistingSuffix)
			&& StripEditorGeneratedNameSuffixes(ExistingBaseName) == BaseName)
		{
			HighestSuffix = std::max(HighestSuffix, ExistingSuffix);
		}
	}

	int32 Suffix = std::max(HighestSuffix + 1, 1);
	FString Candidate;
	do
	{
		Candidate = BaseName + "_" + std::to_string(Suffix++);
	}
	while (IsEditorActorNameTaken(World, TargetActor, Candidate));

	return Candidate;
}

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow, UEditorEngine* InEditor)
{
	FViewportClient::Initialize(InWindow);
	Editor = InEditor;
	InputRouter.SetActiveController(EActiveEditorController::EditorWorldController);
	InputRouter.GetEditorWorldController().SetSelectionPickResolver(
		[this](float LocalX, float LocalY, AActor*& OutActor)
		{
			return FEditorPickingService::ResolveActorForSelection(World, GetCamera(), Viewport, Editor, LocalX, LocalY, OutActor);
		});
	InitializeInputTools();
}

void FEditorViewportClient::SetWorld(UWorld* InWorld)
{
	World = InWorld;
	InputRouter.GetEditorWorldController().SetWorld(InWorld);
}

void FEditorViewportClient::StartPIE(UWorld* InWorld)
{
	World = InWorld;
	FEditorViewportPIEController::StartPIE(Editor, World, InputRouter, Camera, bControlLocked);
	TriggerPIEStartOutlineFlash();
}

void FEditorViewportClient::EndPIE(UWorld* InWorld)
{
	World = InWorld;
	FEditorViewportPIEController::EndPIE(Editor, World, InputRouter, Camera, bControlLocked);
}

bool FEditorViewportClient::IsPIEPossessed() const
{
	return FEditorViewportPIEController::IsPossessed(IsPIEActive(), Editor, InputRouter);
}

bool FEditorViewportClient::IsPIEEditorControlMode() const
{
	return FEditorViewportPIEController::IsEditorControlMode(IsPIEActive(), Editor, InputRouter);
}

bool FEditorViewportClient::AllowsEditorWorldControl() const
{
	return FEditorViewportPIEController::AllowsEditorWorldControl(IsPIEActive(), Editor, InputRouter);
}

APlayerController* FEditorViewportClient::GetPIEPlayerController() const
{
	return FEditorViewportPIEController::GetPlayerController(Editor, InputRouter);
}

void FEditorViewportClient::SetPIEPlayerController(APlayerController* InController)
{
	FEditorViewportPIEController::SetPlayerController(Editor, InputRouter, InController);
}

void FEditorViewportClient::ClearPIEPlayerController()
{
	FEditorViewportPIEController::ClearPlayerController(Editor, InputRouter);
}

void FEditorViewportClient::SetSelectionManager(FSelectionManager* InSelectionManager)
{
    SelectionManager = InSelectionManager;
    InputRouter.GetEditorWorldController().SetSelectionManager(InSelectionManager);
}

void FEditorViewportClient::CreateCamera()
{
	bHasCamera = true;
	Camera = FViewportCamera();
	Camera.OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
	InputRouter.GetEditorWorldController().SetCamera(&Camera);
	InputRouter.GetGameInputBridge().SetCamera(&Camera);
	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
	InputRouter.GetGameInputBridge().ResetTargetLocation();
}

void FEditorViewportClient::DestroyCamera()
{
	bHasCamera = false;
	InputRouter.GetEditorWorldController().NullifyCamera();
}

void FEditorViewportClient::ResetCamera()
{
	if (!bHasCamera || !Settings)
		return;

	if (FEditorViewportNavigationController::ResetCamera(&Camera, Settings->InitViewPos, Settings->InitLookAt))
	{
		InputRouter.GetEditorWorldController().ResetTargetFromCamera();
	}
}

FViewportCamera* FEditorViewportClient::GetRenderCamera()
{
	if (!bHasCamera)
	{
		return nullptr;
	}

	if (IsPIEPossessed() && World)
	{
		if (FViewportCamera* ActiveCamera = World->GetActiveCamera())
		{
			return ActiveCamera;
		}
	}

	return &Camera;
}

const FViewportCamera* FEditorViewportClient::GetRenderCamera() const
{
	return const_cast<FEditorViewportClient*>(this)->GetRenderCamera();
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	FViewportClient::SetViewportSize(InWidth, InHeight);

	if (bHasCamera)
		Camera.OnResize(static_cast<uint32>(WindowWidth), static_cast<uint32>(WindowHeight));
}

void FEditorViewportClient::SetGizmo(UGizmoComponent* InGizmo)
{
	Gizmo = InGizmo;
	InputRouter.GetEditorWorldController().SetGizmo(InGizmo);
	ApplyTransformModeToGizmo();
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	if (PIEStartOutlineFlashRemaining > 0.0f)
	{
		PIEStartOutlineFlashRemaining = std::max(0.0f, PIEStartOutlineFlashRemaining - DeltaTime);
	}

	SyncGizmoVisualState();

	if (bRoutedInputProcessedThisFrame)
	{
		TickInteraction(DeltaTime);
		bRoutedInputProcessedThisFrame = false;
		return;
	}

	if (State && !State->bHovered)
	{
		return;
	}

	TickInteraction(DeltaTime);
}

void FEditorViewportClient::TriggerPIEStartOutlineFlash(float DurationSeconds)
{
	PIEStartOutlineFlashDuration = std::max(0.01f, DurationSeconds);
	PIEStartOutlineFlashRemaining = PIEStartOutlineFlashDuration;
}

float FEditorViewportClient::GetPIEStartOutlineFlashAlpha() const
{
	if (PIEStartOutlineFlashDuration <= 0.0f || PIEStartOutlineFlashRemaining <= 0.0f)
	{
		return 0.0f;
	}
	return MathUtil::Clamp(PIEStartOutlineFlashRemaining / PIEStartOutlineFlashDuration, 0.0f, 1.0f);
}

void FEditorViewportClient::RequestToggleCoordinateSpace()
{
	if (Gizmo)
	{
		Gizmo->SetWorldSpace(!Gizmo->IsWorldSpace());
	}
}

void FEditorViewportClient::RequestSelectAtViewportLocalPoint(float LocalX, float LocalY, bool bToggle, bool bAdditive)
{
	if (!World || !SelectionManager || !bHasCamera || !Viewport)
	{
		return;
	}

	const InputSystem& IS = InputSystem::Get();
	if (IS.GetKey(VK_LBUTTON)
		|| IS.GetKey(VK_RBUTTON)
		|| IS.GetLeftDragging()
		|| IS.GetRightDragging())
	{
		return;
	}

	const FViewportRect& Rect = Viewport->GetRect();
	if (Rect.Width <= 0 || Rect.Height <= 0)
	{
		return;
	}

	AActor* BestActor = nullptr;
	FEditorPickingService::ResolveActorForSelection(World, GetCamera(), Viewport, Editor, LocalX, LocalY, BestActor);

	if (!BestActor)
	{
		if (!bToggle && !bAdditive)
		{
			SelectionManager->ClearSelection();
		}
		return;
	}

	if (bToggle)
	{
		SelectionManager->ToggleSelect(BestActor);
	}
	else if (bAdditive)
	{
		SelectionManager->AddSelect(BestActor);
	}
	else
	{
		SelectionManager->Select(BestActor);
	}
}

bool FEditorViewportClient::ProcessInput(FViewportInputContext& Context)
{
	if (!bHasCamera)
		return false;

	if (State)
	{
		State->bHovered = Context.bHovered;
	}

	TickInput(Context);
	bRoutedInputProcessedThisFrame = true;
	return true;
}

bool FEditorViewportClient::WantsRelativeMouseMode(const FViewportInputContext& Context, POINT& OutRestoreScreenPos) const
{
	OutRestoreScreenPos = Context.Frame.MouseScreenPos;

	if (!bHasCamera || bControlLocked)
	{
		return false;
	}

	if (Context.bImGuiCapturedMouse || IsPointerInViewportInputDeadZone(Context))
	{
		return false;
	}

	if (InputRouter.GetActiveController() == EActiveEditorController::GameInputBridge)
	{
		return IsPIEActive()
			&& IsRuntimeGameInputCaptured()
			&& !IsPIEMouseFocusReleased()
			&& (Context.bFocused || Context.bCaptured || Context.bHovered || Context.bRelativeMouseMode);
	}

	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
	{
		return false;
	}

	const bool bGizmoInteractionActive = Gizmo && (Gizmo->IsHolding() || Gizmo->IsPressedOnHandle());
	if (bGizmoInteractionActive)
	{
		return false;
	}

	bool bGizmoBlocksLeftRelativeDrag = Gizmo && Gizmo->IsHovered();
	if (!bGizmoBlocksLeftRelativeDrag && Gizmo && Viewport && Context.Frame.IsDown(VK_LBUTTON))
	{
		const FViewportRect& Rect = Viewport->GetRect();
		if (Rect.Width > 0 && Rect.Height > 0)
		{
			const float LocalMouseX = static_cast<float>(Context.MouseLocalPos.x);
			const float LocalMouseY = static_cast<float>(Context.MouseLocalPos.y);
			const FRay MouseRay = Camera.DeprojectScreenToWorld(LocalMouseX, LocalMouseY, static_cast<float>(Rect.Width), static_cast<float>(Rect.Height));
			FHitResult GizmoHit{};
			bGizmoBlocksLeftRelativeDrag = Gizmo->HitTestMesh(MouseRay, GizmoHit);
		}
	}

	const bool bPlainPerspectiveLeftLookChord =
		!Camera.IsOrthographic()
		&& TransformMode != ETransformMode::Select
		&& !bBoxSelecting
		&& !IsBoxSelectionChordActive(Context)
		&& Context.Frame.IsDown(VK_LBUTTON)
		&& !Context.Frame.IsAltDown()
		&& !Context.Frame.IsCtrlDown()
		&& !Context.Frame.IsShiftDown();
	const bool bLeftGesture =
		Context.Frame.bLeftDragging
		|| Context.WasPointerDragStarted(EPointerButton::Left)
		|| Context.bRelativeMouseMode;
	const bool bLeftLookDrag =
		bPlainPerspectiveLeftLookChord
		&& bLeftGesture
		&& !bGizmoBlocksLeftRelativeDrag;
	const bool bRightLookDrag =
		Context.Frame.IsDown(VK_RBUTTON)
		&& (Context.Frame.bRightDragging || Context.WasPointerDragStarted(EPointerButton::Right) || Context.bRelativeMouseMode);
	const bool bMouseOwnedByViewport = Context.bCaptured || Context.bHovered || Context.bRelativeMouseMode;
	if (!bMouseOwnedByViewport)
	{
		return false;
	}

	return bRightLookDrag
		|| Context.Frame.IsDown(VK_MBUTTON)
		|| (Context.Frame.IsDown(VK_LBUTTON) && Context.Frame.IsAltDown() && !Context.Frame.IsCtrlDown())
		|| Context.Frame.bMiddleDragging
		|| bLeftLookDrag;
}

bool FEditorViewportClient::WantsAbsoluteMouseClip(const FViewportInputContext& Context, RECT& OutClipScreenRect) const
{
	(void)Context;

	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
	{
		return false;
	}
	if (!Window || !Viewport || !Gizmo || !Gizmo->IsVisible() || !Gizmo->IsHolding())
	{
		return false;
	}

	const FViewportRect& ViewportRect = Viewport->GetRect();
	if (ViewportRect.Width <= 0 || ViewportRect.Height <= 0)
	{
		return false;
	}

	POINT TopLeft =
	{
		static_cast<LONG>(ViewportRect.X),
		static_cast<LONG>(ViewportRect.Y + ViewportInputDeadZoneTop)
	};
	POINT BottomRight =
	{
		static_cast<LONG>(ViewportRect.X + ViewportRect.Width),
		static_cast<LONG>(ViewportRect.Y + ViewportRect.Height)
	};

	::ClientToScreen(Window->GetHWND(), &TopLeft);
	::ClientToScreen(Window->GetHWND(), &BottomRight);

	OutClipScreenRect.left = TopLeft.x;
	OutClipScreenRect.top = TopLeft.y;
	OutClipScreenRect.right = BottomRight.x;
	OutClipScreenRect.bottom = BottomRight.y;
	return OutClipScreenRect.right > OutClipScreenRect.left && OutClipScreenRect.bottom > OutClipScreenRect.top;
}

void FEditorViewportClient::BuildSceneView(FSceneView& OutView) const
{
	if (!bHasCamera) return;

	const FViewportCamera* RenderCamera = GetRenderCamera();
	if (!RenderCamera)
	{
		return;
	}

	if (Viewport)
	{
		const FViewportRect& Rect = Viewport->GetRect();
		if (Rect.Width > 0 && Rect.Height > 0)
		{
			const_cast<FViewportCamera*>(RenderCamera)->OnResize(
				static_cast<uint32>(Rect.Width),
				static_cast<uint32>(Rect.Height));
		}
	}

	OutView.ViewMatrix           = RenderCamera->GetViewMatrix();
	OutView.ProjectionMatrix     = RenderCamera->GetProjectionMatrix();
	OutView.ViewProjectionMatrix = OutView.ViewMatrix * OutView.ProjectionMatrix;

	OutView.CameraPosition = RenderCamera->GetLocation();
	OutView.CameraForward  = RenderCamera->GetForwardVector();
	OutView.CameraRight    = RenderCamera->GetRightVector();
	OutView.CameraUp       = RenderCamera->GetUpVector();

	OutView.bOrthographic = RenderCamera->IsOrthographic();

    OutView.CameraOrthoHeight = RenderCamera->GetOrthoHeight();

	OutView.CameraFrustum = RenderCamera->GetFrustum();

	if (State)
	{
		OutView.ViewRect = Viewport->GetRect();
		OutView.ViewMode = State->ViewMode;
		OutView.LightCullMode = State->LightCullMode;
	}
}

void FEditorViewportClient::ApplyCameraMode()
{
	FEditorViewportNavigationController::ApplyCameraMode(Camera, static_cast<int32>(ViewportType));
	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
}

// Input tick sub-steps

void FEditorViewportClient::TickInput(const FViewportInputContext& Context)
{
	if (!bHasCamera)
		return;

	const float VX = State ? static_cast<float>(Viewport->GetRect().X) : 0.f;
	const float VY = State ? static_cast<float>(Viewport->GetRect().Y) : 0.f;

	InputRouter.SetViewportDim(VX, VY, WindowWidth, WindowHeight);
	if (InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController &&
		IsPassiveEditorViewportHover(Context))
	{
		InputRouter.GetEditorWorldController().ResetTargetFromCamera();
		return;
	}
	InputRouter.Tick(Context.DeltaSeconds);
	if (TryReacquirePIEMouseFocusOnViewportClick(Context))
	{
		bRoutedInputProcessedThisFrame = true;
		return;
	}
	TickInputContexts(Context);
}

void FEditorViewportClient::TickCursorCapture()
{
	// Cursor ownership is now centralized in FInputRouter/FCursorControl.
	// This viewport only releases stale editor-world locks after buttons are released.
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
	{
		return;
	}

    const InputSystem& IS = InputSystem::Get();
    const bool bAnyMouseReleased = IS.GetKeyUp(VK_LBUTTON) || IS.GetKeyUp(VK_RBUTTON) || IS.GetKeyUp(VK_MBUTTON);
    if (bAnyMouseReleased && !IS.GetKey(VK_LBUTTON) && !IS.GetKey(VK_RBUTTON) && !IS.GetKey(VK_MBUTTON))
    {
        InputSystem::Get().LockMouse(false);
    }
}

void FEditorViewportClient::TickKeyboardInput(const FViewportInputContext& Context)
{
	FEditorViewportNavigationController::RouteKeyboardNavigationInput(
		Context,
		InputRouter,
		bControlLocked,
		IsRuntimeGameInputCaptured());
}

void FEditorViewportClient::TickEditorShortcuts(const FViewportInputContext& Context)
{
	using EEditorViewportAction = EditorViewportInputMapping::EEditorViewportAction;
	const bool bInPIE = IsPIEActive();

	const bool bMouseNavigationActive =
		EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavLookRightDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavLookMiddleDown)
		|| (Context.Frame.IsDown(VK_LBUTTON) && !Context.Frame.IsCtrlDown() && !Context.Frame.IsAltDown())
		|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavOrbitAltLeftDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavDollyAltRightDown)
		|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavPanAltMiddleDown);

	// These shortcuts only apply in the editor world, not during PIE.
	if (InputRouter.GetActiveController() == EActiveEditorController::GameInputBridge)
	{
		if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::EndPIE))
		{
			ExecutePIEShellCommand(EPIESessionShellCommand::EndPlay);
		}
		if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::TogglePIEPossessEject))
		{
			ExecutePIEShellCommand(EPIESessionShellCommand::TogglePossessEject);
		}
		if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::ReleasePIEMouseFocus))
		{
			ExecutePIEShellCommand(EPIESessionShellCommand::ReleaseMouseFocus);
		}
		return;
	}

	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::CycleMode))
	{
		CycleTransformMode();
	}

	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::CycleGizmoMode))
	{
		CycleGizmoTransformMode();
	}

	if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::ToggleCoordinateSpace) && Gizmo)
	{
		Gizmo->SetWorldSpace(!Gizmo->IsWorldSpace());
	}

	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SetModeSelect))
	{
		SetTransformMode(ETransformMode::Select);
	}
	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SetModeTranslate))
	{
		SetTransformMode(ETransformMode::Translate);
	}
	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SetModeRotate))
	{
		SetTransformMode(ETransformMode::Rotate);
	}
	if (!bMouseNavigationActive && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SetModeScale))
	{
		SetTransformMode(ETransformMode::Scale);
	}

	if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::FocusSelection))
	{
		FocusPrimarySelection();
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::Undo) && Editor)
	{
		Editor->GetCommandSystem().Execute(EEditorCommand::Undo);
		return;
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::Redo) && Editor)
	{
		Editor->GetCommandSystem().Execute(EEditorCommand::Redo);
		return;
	}

	if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::DeleteSelection))
	{
		DeleteSelectedActors();
	}

	if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SelectAll))
	{
		SelectAllActors();
	}

	if (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::DuplicateSelection))
	{
		DuplicateSelection();
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NewScene) && Editor)
	{
		Editor->GetMainPanel().RequestNewScene();
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::LoadScene) && Editor)
	{
		Editor->GetMainPanel().RequestLoadSceneWithDialog();
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SaveScene) && Editor)
	{
		Editor->GetMainPanel().RequestSaveScene();
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::SaveSceneAs) && Editor)
	{
		Editor->GetMainPanel().RequestSaveSceneAsWithDialog();
	}
}

void FEditorViewportClient::TickPIEShortCuts(const FViewportInputContext& Context)
{
	if (!IsPIEActive()) return;

	if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::EndPIE))
	{
		ExecutePIEShellCommand(EPIESessionShellCommand::EndPlay);
	}
	if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::TogglePIEPossessEject))
	{
		ExecutePIEShellCommand(EPIESessionShellCommand::TogglePossessEject);
	}
	if (EditorViewportInputMapping::IsTriggered(Context, EditorViewportInputMapping::EEditorViewportAction::ReleasePIEMouseFocus))
	{
		ExecutePIEShellCommand(EPIESessionShellCommand::ReleaseMouseFocus);
	}
}

void FEditorViewportClient::InitializeInputTools()
{
	InputTools.clear();
	InputTools.push_back(std::make_unique<FEditorViewportCommandTool>());
	InputTools.push_back(std::make_unique<FEditorViewportGizmoTool>());
	InputTools.push_back(std::make_unique<FEditorViewportSelectionTool>());
	InputTools.push_back(std::make_unique<FEditorViewportNavigationTool>());

	std::sort(InputTools.begin(), InputTools.end(),
		[](const std::unique_ptr<IEditorViewportTool>& Lhs, const std::unique_ptr<IEditorViewportTool>& Rhs)
		{
			return Lhs->GetPriority() > Rhs->GetPriority();
		});
}

bool FEditorViewportClient::TickInputContexts(const FViewportInputContext& Context)
{
	if (InputTools.empty())
	{
		InitializeInputTools();
	}

	for (const std::unique_ptr<IEditorViewportTool>& Tool : InputTools)
	{
		if (Tool && Tool->HandleInput(*this, Context))
		{
			return true;
		}
	}

	return false;
}

bool FEditorViewportClient::HandleCommandInput(const FViewportInputContext& Context)
{
	using EEditorViewportAction = EditorViewportInputMapping::EEditorViewportAction;

	const bool bInPIE = IsPIEActive();
	const bool bPIEPossessed = IsPIEPossessed();
	const TArray<EEditorViewportAction> CommandActions = bPIEPossessed
		? TArray<EEditorViewportAction>
		{
			EEditorViewportAction::EndPIE,
			EEditorViewportAction::TogglePIEPossessEject,
			EEditorViewportAction::ReleasePIEMouseFocus
		}
		: (bInPIE
		? TArray<EEditorViewportAction>
		{
			EEditorViewportAction::EndPIE,
			EEditorViewportAction::TogglePIEPossessEject,
			EEditorViewportAction::ReleasePIEMouseFocus,
			EEditorViewportAction::CycleMode,
			EEditorViewportAction::SetModeSelect,
			EEditorViewportAction::CycleGizmoMode,
			EEditorViewportAction::ToggleCoordinateSpace,
			EEditorViewportAction::SetModeTranslate,
			EEditorViewportAction::SetModeRotate,
			EEditorViewportAction::SetModeScale,
			EEditorViewportAction::FocusSelection,
			EEditorViewportAction::DeleteSelection,
			EEditorViewportAction::SelectAll,
			EEditorViewportAction::DuplicateSelection
		}
		: TArray<EEditorViewportAction>
		{
			EEditorViewportAction::CycleMode,
			EEditorViewportAction::SetModeSelect,
			EEditorViewportAction::CycleGizmoMode,
			EEditorViewportAction::ToggleCoordinateSpace,
			EEditorViewportAction::SetModeTranslate,
			EEditorViewportAction::SetModeRotate,
			EEditorViewportAction::SetModeScale,
			EEditorViewportAction::FocusSelection,
			EEditorViewportAction::Undo,
			EEditorViewportAction::Redo,
			EEditorViewportAction::DeleteSelection,
			EEditorViewportAction::SelectAll,
			EEditorViewportAction::DuplicateSelection,
			EEditorViewportAction::NewScene,
			EEditorViewportAction::LoadScene,
			EEditorViewportAction::SaveScene,
			EEditorViewportAction::SaveSceneAs
		});

	bool bTriggered = false;
	for (EEditorViewportAction Action : CommandActions)
	{
		bTriggered |= EditorViewportInputMapping::IsTriggered(Context, Action);
	}

	if (!bTriggered)
	{
		return false;
	}

	if (bPIEPossessed)
	{
		TickPIEShortCuts(Context);
	}
	else if (bInPIE
		&& (EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::EndPIE)
			|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::TogglePIEPossessEject)
			|| EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::ReleasePIEMouseFocus)))
	{
		TickPIEShortCuts(Context);
	}
	else
	{
		TickEditorShortcuts(Context);
	}
	return true;
}

bool FEditorViewportClient::HandleGizmoInput(const FViewportInputContext& Context)
{
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
	{
		return false;
	}

	const bool bGizmoDragArmed = Gizmo && (Gizmo->IsPressedOnHandle() || Gizmo->IsHolding());
	if (IsPointerInViewportInputDeadZone(Context)
		|| !Gizmo
		|| !Gizmo->IsVisible()
		|| TransformMode == ETransformMode::Select
		|| (IsBoxSelectionChordActive(Context) && !bGizmoDragArmed))
	{
		return false;
	}

	if (Context.Frame.bLeftDragging && bGizmoDragArmed)
	{
		if (!bGizmoDragUndoCaptured && Editor)
		{
			Editor->GetUndoSystem().CaptureSnapshot("Transform Actors");
			bGizmoDragUndoCaptured = true;
		}
		const float LocalX = static_cast<float>(Context.MouseLocalPos.x);
		const float LocalY = static_cast<float>(Context.MouseLocalPos.y);
		InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseDragged, LocalX, LocalY);
		return true;
	}

	if (Context.WasPointerDragEnded(EPointerButton::Left) && Gizmo->IsHolding())
	{
		const float LocalX = static_cast<float>(Context.MouseLocalPos.x);
		const float LocalY = static_cast<float>(Context.MouseLocalPos.y);
		InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseDragEnded, LocalX, LocalY);
		bGizmoDragUndoCaptured = false;
		return true;
	}

	if (Context.WasReleased(VK_LBUTTON))
	{
		bGizmoDragUndoCaptured = false;
	}

	return false;
}

bool FEditorViewportClient::HandleSelectionInput(const FViewportInputContext& Context)
{
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController)
	{
		return false;
	}

	if (IsPointerInViewportInputDeadZone(Context))
	{
		return false;
	}

	const bool bLeftReleased =
		Context.WasPointerDragEnded(EPointerButton::Left) ||
		Context.WasReleased(VK_LBUTTON);
	const bool bLeftActive =
		Context.Frame.IsDown(VK_LBUTTON) ||
		Context.Frame.bLeftDragging ||
		Context.WasPointerDragStarted(EPointerButton::Left);
	const bool bLeftDragBeyondThreshold =
		Context.Frame.bLeftDragging ||
		Context.WasPointerDragStarted(EPointerButton::Left);
	const bool bSelectModePlainDrag =
		TransformMode == ETransformMode::Select
		&& Context.Frame.IsDown(VK_LBUTTON)
		&& !Context.Frame.IsAltDown()
		&& !Context.Frame.IsCtrlDown()
		&& !Context.Frame.IsShiftDown()
		&& bLeftDragBeyondThreshold;
	const bool bBoxSelectionChord = IsBoxSelectionChordActive(Context) || bSelectModePlainDrag;
	const auto GetClampedLocalPoint = [&](float InLocalX, float InLocalY) -> POINT
	{
		const float ClampedX = MathUtil::Clamp(
			InLocalX,
			0.0f,
			std::max(0.0f, WindowWidth - 1.0f));
		const float ClampedY = MathUtil::Clamp(
			InLocalY,
			0.0f,
			std::max(0.0f, WindowHeight - 1.0f));
		return POINT{ static_cast<LONG>(ClampedX), static_cast<LONG>(ClampedY) };
	};
	const auto GetCurrentLocalPoint = [&]() -> POINT
	{
		return GetClampedLocalPoint(
			static_cast<float>(Context.MouseLocalPos.x),
			static_cast<float>(Context.MouseLocalPos.y));
	};
	const auto GetDragStartLocalPoint = [&]() -> POINT
	{
		return GetClampedLocalPoint(
			static_cast<float>(Context.MouseLocalPos.x - Context.Frame.LeftDragVector.x),
			static_cast<float>(Context.MouseLocalPos.y - Context.Frame.LeftDragVector.y));
	};

	if (bBoxSelecting)
	{
		BoxSelectEnd = GetCurrentLocalPoint();
		if (bLeftReleased)
		{
			HandleBoxSelection();
			bBoxSelecting = false;
		}
		else if (!bLeftActive)
		{
			bBoxSelecting = false;
		}
		return true;
	}

	if (bBoxSelectionChord)
	{
		if (bLeftDragBeyondThreshold)
		{
			bBoxSelecting = true;
			BoxSelectStart = GetDragStartLocalPoint();
			BoxSelectEnd = GetCurrentLocalPoint();
		}
		return true;
	}

	return false;
}

bool FEditorViewportClient::HandleNavigationInput(const FViewportInputContext& Context)
{
	float NextMoveSpeed = GetMoveSpeed();
	if (FEditorViewportNavigationController::HandleMoveSpeedWheel(
		Context,
		InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController,
		GetMoveSpeed(),
		NextMoveSpeed))
	{
		SetMoveSpeed(NextMoveSpeed);
		return true;
	}

	if (HandleAltNavigationInput(Context))
	{
		return true;
	}
	if (HandleLeftNavigationInput(Context))
	{
		return true;
	}

	TickKeyboardInput(Context);
	TickPIEShortCuts(Context);
	TickMouseInput(Context);
	return true;
}

bool FEditorViewportClient::HandleLeftNavigationInput(const FViewportInputContext& Context)
{
	const bool bLeftNavigationChord =
		TransformMode != ETransformMode::Select
		&& !bBoxSelecting
		&& !IsBoxSelectionChordActive(Context)
		&& Context.Frame.IsDown(VK_LBUTTON)
		&& !Context.Frame.IsCtrlDown()
		&& !Context.Frame.IsAltDown()
		&& !Context.Frame.IsShiftDown();
	return FEditorViewportNavigationController::HandleLeftNavigationInput(
		Context,
		Camera,
		InputRouter,
		Gizmo,
		InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController,
		bControlLocked,
		bHasCamera,
		IsPointerInViewportInputDeadZone(Context),
		bLeftNavigationChord,
		GetMoveSpeed());
}

bool FEditorViewportClient::HandleAltNavigationInput(const FViewportInputContext& Context)
{
	bool bSyncCameraTarget = false;
	const bool bHandled = FEditorViewportNavigationController::HandleAltNavigationInput(
		Context,
		Camera,
		SelectionManager,
		InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController,
		bControlLocked,
		bHasCamera,
		GetMoveSpeed(),
		bSyncCameraTarget);
	if (bSyncCameraTarget)
	{
		SyncCameraTarget();
	}
	return bHandled;
}

void FEditorViewportClient::TickMouseInput(const FViewportInputContext& Context)
{
	if (bControlLocked) return;
	if (InputRouter.GetActiveController() == EActiveEditorController::GameInputBridge && IsPIEMouseFocusReleased())
	{
		return;
	}
	if (InputRouter.GetActiveController() == EActiveEditorController::GameInputBridge && !IsRuntimeGameInputCaptured())
	{
		return;
	}

	const float LocalX = static_cast<float>(Context.MouseLocalPos.x);
	const float LocalY = static_cast<float>(Context.MouseLocalPos.y);
	const float DX = static_cast<float>(Context.MouseLocalDelta.x);
	const float DY = static_cast<float>(Context.MouseLocalDelta.y);

	if (IsPointerInViewportInputDeadZone(Context))
	{
		return;
	}

	const bool bSuppressPassiveFeedback = IsPassiveViewportFeedbackSuppressed(Context);
	if (bSuppressPassiveFeedback)
	{
		ClearPassiveGizmoHover();
	}

	if (DX != 0.0f || DY != 0.0f)
	{
		InputRouter.RouteMouseInput(EMouseInputType::E_MouseMoved, DX, DY);
		if (!bSuppressPassiveFeedback)
		{
			InputRouter.RouteMouseInput(EMouseInputType::E_MouseMovedAbsolute, LocalX, LocalY);
		}
	}

	if (Context.WasPressed(VK_RBUTTON))
		InputRouter.RouteMouseInput(EMouseInputType::E_RightMouseClicked, DX, DY);
	if (Context.Frame.bRightDragging || (Context.bRelativeMouseMode && Context.Frame.IsDown(VK_RBUTTON)))
		InputRouter.RouteMouseInput(EMouseInputType::E_RightMouseDragged, DX, DY);
	if (Context.Frame.bMiddleDragging || (Context.bRelativeMouseMode && Context.Frame.IsDown(VK_MBUTTON)))
		InputRouter.RouteMouseInput(EMouseInputType::E_MiddleMouseDragged, DX, DY);
	if (!MathUtil::IsNearlyZero(Context.Frame.WheelNotches))
		InputRouter.RouteMouseInput(EMouseInputType::E_MouseWheelScrolled, Context.Frame.WheelNotches, 0.f);

	if (!IsBoxSelectionChordActive(Context))
	{
		if (Context.WasPressed(VK_LBUTTON))
			InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseClicked, LocalX, LocalY);
		if (Context.Frame.bLeftDragging)
			InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseDragged, LocalX, LocalY);
		if (Context.WasPointerDragEnded(EPointerButton::Left))
			InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseDragEnded, LocalX, LocalY);
		if (Context.WasReleased(VK_LBUTTON) && !Context.WasPointerDragEnded(EPointerButton::Left))
			InputRouter.RouteMouseInput(EMouseInputType::E_LeftMouseButtonUp, LocalX, LocalY);
	}
}

bool FEditorViewportClient::IsBoxSelectionChordActive(const FViewportInputContext& Context) const
{
	if (IsPointerInViewportInputDeadZone(Context))
	{
		return false;
	}

	if (Camera.IsOrthographic()
		&& Context.Frame.IsDown(VK_LBUTTON)
		&& Context.Frame.bLeftDragging
		&& !Context.Frame.IsAltDown()
		&& !Context.Frame.IsCtrlDown())
	{
		return true;
	}

	return Context.Frame.IsCtrlDown()
		&& Context.Frame.IsAltDown()
		&& Context.Frame.IsDown(VK_LBUTTON);
}

bool FEditorViewportClient::IsPointerInViewportInputDeadZone(float LocalY) const
{
	return ViewportInputDeadZoneTop > 0.0f
		&& LocalY >= 0.0f
		&& LocalY < ViewportInputDeadZoneTop;
}

bool FEditorViewportClient::IsPointerInViewportInputDeadZone(const FViewportInputContext& Context) const
{
	return IsPointerInViewportInputDeadZone(static_cast<float>(Context.MouseLocalPos.y));
}

bool FEditorViewportClient::IsPassiveViewportFeedbackSuppressed(const FViewportInputContext& Context) const
{
	return bBoxSelecting ||
		!Context.SideEffects.bAllowPicking ||
		!Context.SideEffects.bAllowGizmoHover ||
		!Context.SideEffects.bAllowSelectionFeedback;
}

void FEditorViewportClient::ClearPassiveGizmoHover() const
{
	if (Gizmo && !Gizmo->IsHolding() && !Gizmo->IsPressedOnHandle())
	{
		Gizmo->UpdateHoveredAxis(-1);
	}
}

void FEditorViewportClient::SetTransformMode(ETransformMode InMode)
{
	TransformMode = InMode;
	ApplyTransformModeToGizmo();
}

void FEditorViewportClient::CycleTransformMode()
{
	SetTransformMode(FEditorTransformInteraction::GetNextTransformMode(TransformMode));
}

void FEditorViewportClient::CycleGizmoTransformMode()
{
	SetTransformMode(FEditorTransformInteraction::GetNextGizmoTransformMode(TransformMode));
}

void FEditorViewportClient::ApplyTransformModeToGizmo()
{
	FEditorTransformInteraction::ApplyTransformModeToGizmo(TransformMode, Gizmo);
}

void FEditorViewportClient::SyncGizmoVisualState()
{
	FEditorTransformInteraction::SyncGizmoVisualState(
		TransformMode,
		Gizmo,
		Camera,
		bHasCamera,
		World && World->GetWorldType() == EWorldType::PIE,
		!State || State->bHovered,
		bRoutedInputProcessedThisFrame);
}

// Interaction (gizmo scaling + box selection)

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;

	if (!bHasCamera)
		return;

	if (World && World->GetWorldType() == EWorldType::PIE)
		return;

	if (!World || !SelectionManager)
		return;

	if (bRoutedInputProcessedThisFrame)
	{
		return;
	}

	// Box selection (Ctrl+Alt+LMB drag)
	POINT MousePoint = InputSystem::Get().GetMousePos();

	if (bBoxSelecting)
	{
		const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
		if (!GuiState.IsInViewportHost(MousePoint.x, MousePoint.y))
		{
			bBoxSelecting = false;
			return;
		}
	}

	if (Window) MousePoint = Window->ScreenToClientPoint(MousePoint);
    const float VX = State ? static_cast<float>(Viewport->GetRect().X) : 0.f;
    const float VY = State ? static_cast<float>(Viewport->GetRect().Y) : 0.f;
	const float LocalX = static_cast<float>(MousePoint.x) - VX;
	const float LocalY = static_cast<float>(MousePoint.y) - VY;

	if (bBoxSelecting && (LocalX < 0.f || LocalY < 0.f || LocalX > WindowWidth || LocalY > WindowHeight))
	{
		bBoxSelecting = false;
		return;
	}

	const InputSystem& IS        = InputSystem::Get();
	const bool         bCtrlDown = IS.GetKey(VK_CONTROL);
	const bool         bAltDown  = IS.GetKey(VK_MENU);

	const bool bLeftDragBeyondThreshold = IS.GetLeftDragging();
	const bool bBoxSelectionChord =
		bLeftDragBeyondThreshold
		&& ((bCtrlDown && bAltDown)
			|| (Camera.IsOrthographic() && !bCtrlDown && !bAltDown));

	if (!bBoxSelecting && bBoxSelectionChord)
	{
		const POINT DragVector = IS.GetLeftDragVector();
		bBoxSelecting  = true;
		BoxSelectStart = POINT{
			static_cast<LONG>(MathUtil::Clamp(LocalX - static_cast<float>(DragVector.x), 0.0f, std::max(0.0f, WindowWidth - 1.0f))),
			static_cast<LONG>(MathUtil::Clamp(LocalY - static_cast<float>(DragVector.y), 0.0f, std::max(0.0f, WindowHeight - 1.0f)))
		};
		BoxSelectEnd = POINT{
			static_cast<LONG>(MathUtil::Clamp(LocalX, 0.0f, std::max(0.0f, WindowWidth - 1.0f))),
			static_cast<LONG>(MathUtil::Clamp(LocalY, 0.0f, std::max(0.0f, WindowHeight - 1.0f)))
		};
		return;
	}

	if (bBoxSelecting)
	{
		if (IS.GetLeftDragging())
			BoxSelectEnd = POINT{ static_cast<LONG>(LocalX), static_cast<LONG>(LocalY) };
		else if (IS.GetLeftDragEnd())
		{
			HandleBoxSelection();
			bBoxSelecting = false;
		}
		else if (IS.GetKeyUp(VK_LBUTTON))
			bBoxSelecting = false;
	}
}

void FEditorViewportClient::LockCursorToViewport()
{
	// State->Rect is in client space; LockMouse needs screen space.
    POINT Origin = { Viewport->GetRect().X, Viewport->GetRect().Y };
	if (Window)
		::ClientToScreen(Window->GetHWND(), &Origin);
	InputSystem::Get().LockMouse(true, (float)Origin.x, (float)Origin.y,
                                 (float)Viewport->GetRect().Width, (float)Viewport->GetRect().Height);
}

void FEditorViewportClient::HandleBoxSelection()
{
	FEditorBoxSelectionService::SelectActorsInBox(
		World,
		SelectionManager,
		GetCamera(),
		BoxSelectStart,
		BoxSelectEnd,
		WindowWidth,
		WindowHeight,
		FrustumQueryScratch);
}

void FEditorViewportClient::FocusPrimarySelection()
{
	if (FEditorViewportNavigationController::FocusPrimarySelection(SelectionManager, GetCamera()))
	{
		InputRouter.GetEditorWorldController().ResetTargetFromCamera();
	}
}

void FEditorViewportClient::DeleteSelectedActors()
{
	if (!SelectionManager)
		return;

	const TArray<AActor*> SelectedActors = SelectionManager->GetSelectedActors();
	if (SelectedActors.empty())
		return;

	if (Editor)
	{
		Editor->DeleteActors(SelectedActors);
	}
}

void FEditorViewportClient::SelectAllActors()
{
	if (!SelectionManager || !World)
		return;

	SelectionManager->ClearSelection();
	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		if (AActor* Actor = *Iter)
			SelectionManager->AddSelect(Actor);
	}
}

void FEditorViewportClient::DuplicateSelection()
{
	if (!SelectionManager || !World)
		return;

	const TArray<AActor*> SelectedActors = SelectionManager->GetSelectedActors();
	if (SelectedActors.empty())
		return;

	if (Editor)
	{
		Editor->GetUndoSystem().CaptureSnapshot("Duplicate Actors");
	}

	TArray<AActor*> NewSelection;
	for (AActor* SourceActor : SelectedActors)
	{
		if (!SourceActor)
			continue;

		AActor* DuplicatedActor = Cast<AActor>(SourceActor->Duplicate());
		if (!DuplicatedActor)
			continue;

		DuplicatedActor->SetFName(FName(MakeUniqueEditorDuplicateActorName(
			World,
			DuplicatedActor,
			SourceActor->GetFName().ToString())));
		DuplicatedActor->SetWorld(World);
		if (ULevel* Level = World->GetPersistentLevel())
		{
			Level->AddActor(DuplicatedActor);
		}
		DuplicatedActor->AddActorWorldOffset(FVector(0.25f, 0.25f, 0.0f));
		NewSelection.push_back(DuplicatedActor);
	}

	if (NewSelection.empty())
		return;

	SelectionManager->ClearSelection();
	for (AActor* Actor : NewSelection)
	{
		SelectionManager->AddSelect(Actor);
	}

	World->RebuildSpatialIndex();
}

void FEditorViewportClient::RequestEndPIE()
{
	FEditorViewportPIEController::RequestEndPIE(Editor);
}

void FEditorViewportClient::TogglePIEPossessEject()
{
	if (!IsPIEActive())
		return;

	if (IsPIEPossessed())
	{
		EnterPIEEditorControlMode();
	}
	else
	{
		EnterPIEPossessedMode();
	}
}

void FEditorViewportClient::ReleasePIEMouseFocus()
{
	if (!IsPIEPossessed())
		return;

	FEditorViewportPIEController::ReleaseMouseFocus(Editor, bControlLocked);
}

bool FEditorViewportClient::ExecutePIEShellCommand(EPIESessionShellCommand Command)
{
	return FEditorViewportPIEController::ExecuteShellCommand(Editor, Command, *this);
}

bool FEditorViewportClient::IsPIEMouseFocusReleased() const
{
	return FEditorViewportPIEController::IsMouseFocusReleased(Editor);
}

void FEditorViewportClient::MarkPIEMouseFocusReleased()
{
	FEditorViewportPIEController::MarkMouseFocusReleased(Editor);
}

void FEditorViewportClient::ClearPIEMouseFocusReleased()
{
	FEditorViewportPIEController::ClearMouseFocusReleased(Editor);
}

void FEditorViewportClient::ReacquirePIEMouseFocus()
{
	if (!IsPIEActive())
		return;

	EnterPIEPossessedMode();
}

bool FEditorViewportClient::TryReacquirePIEMouseFocusOnViewportClick(const FViewportInputContext& Context)
{
	if (!IsPIEPossessed()
		|| !IsPIEMouseFocusReleased()
		|| !Context.WasPressed(VK_LBUTTON)
		|| Context.bImGuiCapturedMouse
		|| IsPointerInViewportInputDeadZone(Context))
	{
		return false;
	}

	ReacquirePIEMouseFocus();
	return true;
}

void FEditorViewportClient::EnterPIEEditorControlMode()
{
	FEditorViewportPIEController::EnterEditorControlMode(IsPIEActive(), Editor, World, InputRouter, Camera, bControlLocked);
}

void FEditorViewportClient::EnterPIEPossessedMode()
{
	FEditorViewportPIEController::EnterPossessedMode(IsPIEActive(), Editor, World, InputRouter, Camera, bControlLocked);
	LockCursorToViewport();
}

void FEditorViewportClient::SaveCameraSnapshot()
{
	FViewportCamera CameraStae;
}

void FEditorViewportClient::RestoreCameraSnapshot()
{

}
