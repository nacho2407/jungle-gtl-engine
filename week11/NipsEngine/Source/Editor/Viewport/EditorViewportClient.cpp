#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Input/EditorViewportInputContexts.h"
#include "Editor/Input/EditorViewportInputMapping.h"
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
#include <unordered_set>

namespace
{
bool IsEditorCameraKeyboardKey(int VK)
{
	switch (VK)
	{
	case 'W':
	case 'A':
	case 'S':
	case 'D':
	case 'Q':
	case 'E':
	case VK_LEFT:
	case VK_RIGHT:
	case VK_UP:
	case VK_DOWN:
		return true;
	default:
		return false;
	}
}

bool IsRuntimeGameInputCaptured()
{
	return GEngine
		&& GEngine->GetRuntimeInputMode() == ERuntimeInputMode::GameOnly
		&& GEngine->IsRuntimeCursorLocked();
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
			return ResolveActorForSelection(LocalX, LocalY, OutActor);
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
	InputRouter.GetEditorWorldController().SetWorld(InWorld);
	InputRouter.GetGameInputBridge().SetCamera(&Camera); // re-sync Yaw/Pitch
    InputRouter.GetGameInputBridge().SetTargetLocation(InputRouter.GetEditorWorldController().GetTargetLocation());
	InputRouter.SetActiveController(EActiveEditorController::GameInputBridge);
	if (Editor)
	{
		Editor->GetPIESession().SetControlMode(EPIESessionControlMode::Possessed);
	}
	bControlLocked = false;
	ClearPIEMouseFocusReleased();
	TriggerPIEStartOutlineFlash();
}

void FEditorViewportClient::EndPIE(UWorld* InWorld)
{
	World = InWorld;
	if (InputRouter.GetActiveController() == EActiveEditorController::GameInputBridge)
	{
		InputRouter.GetEditorWorldController().SetTargetLocation(InputRouter.GetGameInputBridge().GetTargetLocation());
	}
	InputRouter.GetEditorWorldController().SetWorld(InWorld);
	InputRouter.SetActiveController(EActiveEditorController::EditorWorldController);
	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
	ClearPIEPlayerController();
	InputSystem::Get().LockMouse(false);
	bControlLocked = false;
	ClearPIEMouseFocusReleased();
	if (Editor)
	{
		Editor->GetPIESession().ClearControlMode();
	}
}

bool FEditorViewportClient::IsPIEPossessed() const
{
	if (!IsPIEActive())
	{
		return false;
	}

	if (Editor)
	{
		return Editor->GetPIESession().GetControlMode() == EPIESessionControlMode::Possessed;
	}

	return InputRouter.GetActiveController() == EActiveEditorController::GameInputBridge;
}

bool FEditorViewportClient::IsPIEEditorControlMode() const
{
	if (!IsPIEActive())
	{
		return false;
	}

	if (Editor)
	{
		return Editor->GetPIESession().GetControlMode() == EPIESessionControlMode::EditorControl;
	}

	return InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController;
}

bool FEditorViewportClient::AllowsEditorWorldControl() const
{
	if (!IsPIEActive())
	{
		return InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController;
	}

	if (Editor)
	{
		return Editor->GetPIESession().GetControlMode() == EPIESessionControlMode::EditorControl;
	}

	return InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController;
}

APlayerController* FEditorViewportClient::GetPIEPlayerController() const
{
	return Editor ? Editor->GetPIESession().GetPlayerController() : InputRouter.GetGameInputBridge().GetPlayerController();
}

void FEditorViewportClient::SetPIEPlayerController(APlayerController* InController)
{
	if (Editor)
	{
		Editor->GetPIESession().SetPlayerController(InController);
	}
	InputRouter.GetGameInputBridge().SetPlayerController(InController);
}

void FEditorViewportClient::ClearPIEPlayerController()
{
	if (Editor)
	{
		Editor->GetPIESession().ClearPlayerController();
	}
	InputRouter.GetGameInputBridge().ClearPlayerController();
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
	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
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
	ResolveActorForSelection(LocalX, LocalY, BestActor);

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

bool FEditorViewportClient::ResolveActorForSelection(float LocalX, float LocalY, AActor*& OutActor)
{
	OutActor = nullptr;
	const FEditorSettings& EditorSettings = FEditorSettings::Get();
	if (EditorSettings.PickingMode == EEditorPickingMode::IdBuffer && PickActorByIdAtViewportLocalPoint(LocalX, LocalY, OutActor))
	{
		return OutActor != nullptr;
	}

	return PickActorByRayAtViewportLocalPoint(LocalX, LocalY, OutActor);
}

bool FEditorViewportClient::PickActorByIdAtViewportLocalPoint(float LocalX, float LocalY, AActor*& OutActor)
{
	OutActor = nullptr;
	if (!Viewport || !Editor)
	{
		return false;
	}

	ID3D11DeviceContext* Context = Editor->GetRenderer().GetFD3DDevice().GetDeviceContext();
	if (!Context)
	{
		return false;
	}

	const int32 BaseX = static_cast<int32>(std::round(LocalX));
	const int32 BaseY = static_cast<int32>(std::round(LocalY));
	static constexpr POINT SampleOffsets[] =
	{
		{ 0, 0 },
		{ 1, 0 },
		{ -1, 0 },
		{ 0, 1 },
		{ 0, -1 }
	};

	for (const POINT& Offset : SampleOffsets)
	{
		uint32 PickId = 0;
		const int32 SampleX = BaseX + static_cast<int32>(Offset.x);
		const int32 SampleY = BaseY + static_cast<int32>(Offset.y);
		const uint32 X = static_cast<uint32>(std::max<int32>(0, SampleX));
		const uint32 Y = static_cast<uint32>(std::max<int32>(0, SampleY));
		if (!Viewport->ReadEditorIdPickAt(X, Y, Context, PickId) || PickId == 0)
		{
			continue;
		}

		OutActor = Viewport->GetEditorIdPickActor(PickId);
		return OutActor != nullptr;
	}

	return true;
}

bool FEditorViewportClient::PickActorByRayAtViewportLocalPoint(float LocalX, float LocalY, AActor*& OutActor)
{
	OutActor = nullptr;
	if (!World || !bHasCamera || !Viewport)
	{
		return false;
	}

	const FViewportRect& Rect = Viewport->GetRect();
	if (Rect.Width <= 0 || Rect.Height <= 0)
	{
		return false;
	}

	const FRay Ray = Camera.DeprojectScreenToWorld(LocalX, LocalY, static_cast<float>(Rect.Width), static_cast<float>(Rect.Height));
	float ClosestDist = FLT_MAX;

	FWorldSpatialIndex::FPrimitiveRayQueryScratch RayQueryScratch;
	TArray<UPrimitiveComponent*> CandidatePrimitives;
	TArray<float> CandidateTs;
	World->GetSpatialIndex().RayQueryPrimitives(Ray, CandidatePrimitives, CandidateTs, RayQueryScratch);

	for (int32 CandidateIndex = 0; CandidateIndex < static_cast<int32>(CandidatePrimitives.size()); ++CandidateIndex)
	{
		if (CandidateTs[CandidateIndex] > ClosestDist)
		{
			break;
		}

		UPrimitiveComponent* PrimitiveComp = CandidatePrimitives[CandidateIndex];
		AActor* Actor = PrimitiveComp ? PrimitiveComp->GetOwner() : nullptr;
		if (!Actor || !Actor->GetRootComponent())
		{
			continue;
		}

		FHitResult HitResult{};
		if (PrimitiveComp->Raycast(Ray, HitResult) && HitResult.Distance < ClosestDist)
		{
			ClosestDist = HitResult.Distance;
			OutActor = Actor;
		}
	}

	return true;
}

bool FEditorViewportClient::ProcessInput(FViewportInputContext& Context)
{
	if (!bHasCamera)
		return false;

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
	// Orthographic views reset rotation so the existing value doesn't interfere with LookAt.
	Camera.SetRotation(FRotator(0.f, 0.f, 0.f));

	switch (ViewportType)
	{
	case EVT_Perspective:
		Camera.SetProjectionType(EViewportProjectionType::Perspective);
		Camera.ClearCustomLookDir();
		Camera.SetLocation(FVector(5.f, 3.f, 5.f));
		Camera.SetLookAt(FVector(0.f, 0.f, 0.f));
		break;

	// Orthographic views (X=Forward, Y=Right, Z=Up)

	case EVT_OrthoTop:			// top-down (-Z), screen-up = +X
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 0.f, 1000.f));
		Camera.SetCustomLookDir(FVector(0.f, 0.f, -1.f), FVector(1.f, 0.f, 0.f));
		break;

	case EVT_OrthoBottom:		// bottom-up (+Z), screen-up = +X
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 0.f, -1000.f));
		Camera.SetCustomLookDir(FVector(0.f, 0.f, 1.f), FVector(1.f, 0.f, 0.f));
		break;

	case EVT_OrthoFront:		// front (-X->+X), screen-up = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(1000.f, 0.f, 0.f));
		Camera.SetCustomLookDir(FVector(-1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoBack:			// back (+X->-X), screen-up = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(-1000.f, 0.f, 0.f));
		Camera.SetCustomLookDir(FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoLeft:			// left (-Y->+Y), screen-up = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, -1000.f, 0.f));
		Camera.SetCustomLookDir(FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoRight:		// right (+Y->-Y), screen-up = +Z
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 1000.f, 0.f));
		Camera.SetCustomLookDir(FVector(0.f, -1.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	default:
		break;
	}

	// Reset lerp target immediately so accumulated TargetLocation doesn't
	// move the camera on the next Tick after a mode switch.
	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
}

// ── Input tick sub-steps ──────────────────────────────────────────────────────

void FEditorViewportClient::TickInput(const FViewportInputContext& Context)
{
	if (!bHasCamera)
		return;

	const float VX = State ? static_cast<float>(Viewport->GetRect().X) : 0.f;
	const float VY = State ? static_cast<float>(Viewport->GetRect().Y) : 0.f;

	InputRouter.SetViewportDim(VX, VY, WindowWidth, WindowHeight);
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
	static constexpr int WatchKeys[] = {
		'W', 'A', 'S', 'D', 'Q', 'E',
		VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
		VK_SPACE,
	};

	if (bControlLocked) return;
	if (InputRouter.GetActiveController() == EActiveEditorController::GameInputBridge && !IsRuntimeGameInputCaptured())
	{
		return;
	}
	const bool bEditorWorld = InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController;
	const bool bKeyboardNavigationChord =
		Context.Frame.IsDown(VK_RBUTTON)
		|| (Context.Frame.IsDown(VK_LBUTTON) && !Context.Frame.IsCtrlDown() && !Context.Frame.IsAltDown())
		|| Context.Frame.IsDown(VK_MBUTTON)
		|| Context.bRelativeMouseMode;

	for (int VK : WatchKeys)
	{
		if (bEditorWorld && IsEditorCameraKeyboardKey(VK) && !bKeyboardNavigationChord)
		{
			continue;
		}
		if (Context.WasPressed(VK)) InputRouter.RouteKeyboardInput(EKeyInputType::KeyPressed, VK);
		if (Context.Frame.IsDown(VK)) InputRouter.RouteKeyboardInput(EKeyInputType::KeyDown, VK);
		if (Context.WasReleased(VK)) InputRouter.RouteKeyboardInput(EKeyInputType::KeyReleased, VK);
	}
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
		Editor->GetUndoSystem().Undo();
		return;
	}

	if (!bInPIE && EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::Redo) && Editor)
	{
		Editor->GetUndoSystem().Redo();
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
	if (InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController
		&& Context.Frame.IsDown(VK_RBUTTON)
		&& !MathUtil::IsNearlyZero(Context.Frame.WheelNotches))
	{
		const float WheelScale = std::pow(1.15f, static_cast<float>(Context.Frame.WheelNotches));
		SetMoveSpeed(MathUtil::Clamp(GetMoveSpeed() * WheelScale, 0.1f, 5000.0f));
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
	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController || bControlLocked || !bHasCamera)
	{
		return false;
	}
	if (IsPointerInViewportInputDeadZone(Context) || Context.bImGuiCapturedMouse || Camera.IsOrthographic())
	{
		return false;
	}

	const bool bLeftNavigationChord =
		TransformMode != ETransformMode::Select
		&& !bBoxSelecting
		&& !IsBoxSelectionChordActive(Context)
		&& Context.Frame.IsDown(VK_LBUTTON)
		&& !Context.Frame.IsCtrlDown()
		&& !Context.Frame.IsAltDown()
		&& !Context.Frame.IsShiftDown();
	if (!bLeftNavigationChord)
	{
		return false;
	}

	if (Gizmo && (Gizmo->IsHolding() || Gizmo->IsPressedOnHandle() || Gizmo->IsHovered()))
	{
		return false;
	}

	const bool bLeftGesture =
		Context.Frame.bLeftDragging
		|| Context.WasPointerDragStarted(EPointerButton::Left)
		|| Context.bRelativeMouseMode;
	if (!bLeftGesture)
	{
		return false;
	}

	float DeltaX = static_cast<float>(Context.Frame.MouseDelta.x);
	float DeltaY = static_cast<float>(Context.Frame.MouseDelta.y);
	if (MathUtil::IsNearlyZero(DeltaX) && MathUtil::IsNearlyZero(DeltaY))
	{
		DeltaX = static_cast<float>(Context.MouseLocalDelta.x);
		DeltaY = static_cast<float>(Context.MouseLocalDelta.y);
	}
	if (MathUtil::IsNearlyZero(DeltaX) && MathUtil::IsNearlyZero(DeltaY))
	{
		return true;
	}

	const FVector Forward = Camera.GetForwardVector().GetSafeNormal();
	float Pitch = MathUtil::RadiansToDegrees(std::asin(MathUtil::Clamp(Forward.Z, -1.0f, 1.0f)));
	float Yaw = MathUtil::RadiansToDegrees(std::atan2(Forward.Y, Forward.X));

	const FEditorSettings& Settings = FEditorSettings::Get();
	const float RotationSpeed = Settings.CameraRotationSpeed / 400.0f;
	Yaw += DeltaX * RotationSpeed;
	const float PitchRad = MathUtil::DegreesToRadians(Pitch);
	const float YawRad = MathUtil::DegreesToRadians(Yaw);
	FVector NextForward(
		std::cos(PitchRad) * std::cos(YawRad),
		std::cos(PitchRad) * std::sin(YawRad),
		std::sin(PitchRad));
	NextForward = NextForward.GetSafeNormal();

	FVector Right = FVector::CrossProduct(FVector::UpVector, NextForward).GetSafeNormal();
	FQuat AppliedRotation = Camera.GetRotation();
	if (!Right.IsNearlyZero())
	{
		FVector Up = FVector::CrossProduct(NextForward, Right).GetSafeNormal();
		FMatrix RotMat = FMatrix::Identity;
		RotMat.SetAxes(NextForward, Right, Up);
		FQuat NextRotation(RotMat);
		NextRotation.Normalize();
		Camera.SetRotation(NextRotation);
		AppliedRotation = NextRotation;
	}

	const float ForwardScale = GetMoveSpeed() * Settings.CameraDollySpeedScale * 0.01f;
	const FVector NextLocation = Camera.GetLocation() + NextForward * (-DeltaY * ForwardScale);
	Camera.SetLocation(NextLocation);
	InputRouter.GetEditorWorldController().SetTargetLocation(NextLocation);
	InputRouter.GetEditorWorldController().SetTargetRotation(AppliedRotation);
	return true;
}

bool FEditorViewportClient::HandleAltNavigationInput(const FViewportInputContext& Context)
{
	using EEditorViewportAction = EditorViewportInputMapping::EEditorViewportAction;

	if (InputRouter.GetActiveController() != EActiveEditorController::EditorWorldController || bControlLocked || !bHasCamera)
	{
		return false;
	}

	const bool bAltOrbit = EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavOrbitAltLeftDown);
	const bool bAltDolly = EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavDollyAltRightDown);
	const bool bAltPan = EditorViewportInputMapping::IsTriggered(Context, EEditorViewportAction::NavPanAltMiddleDown);
	if (!(bAltOrbit || bAltDolly || bAltPan))
	{
		return false;
	}

	const float DeltaX = static_cast<float>(Context.MouseLocalDelta.x);
	const float DeltaY = static_cast<float>(Context.MouseLocalDelta.y);
	if (MathUtil::IsNearlyZero(DeltaX) && MathUtil::IsNearlyZero(DeltaY))
	{
		return true;
	}

	if (bAltPan)
	{
		const FEditorSettings& Settings = FEditorSettings::Get();
		const float PanScale = Camera.IsOrthographic()
			? Camera.GetOrthoHeight() * 0.002f * Settings.CameraPanSpeedScale
			: GetMoveSpeed() * 0.002f * Settings.CameraPanSpeedScale;
		const FVector Right = Camera.GetEffectiveRight();
		const FVector Up = Camera.GetEffectiveUp();
		Camera.SetLocation(Camera.GetLocation() + Right * (DeltaX * PanScale) + Up * (-DeltaY * PanScale));
		SyncCameraTarget();
		return true;
	}

	if (bAltDolly)
	{
		if (Camera.IsOrthographic())
		{
			const float NextHeight = Camera.GetOrthoHeight() + DeltaY * Camera.GetOrthoHeight() * 0.003f;
			Camera.SetOrthoHeight(MathUtil::Clamp(NextHeight, 0.1f, 5000.0f));
		}
		else
		{
			const float DollyScale = GetMoveSpeed() * FEditorSettings::Get().CameraDollySpeedScale * 0.01f;
			Camera.SetLocation(Camera.GetLocation() + Camera.GetForwardVector().GetSafeNormal() * (-DeltaY * DollyScale));
			SyncCameraTarget();
		}
		return true;
	}

	if (bAltOrbit && !Camera.IsOrthographic())
	{
		FVector Pivot = Camera.GetLocation() + Camera.GetForwardVector().GetSafeNormal() * 10.0f;
		if (SelectionManager)
		{
			if (AActor* Primary = SelectionManager->GetPrimarySelection())
			{
				Pivot = Primary->GetActorLocation();
			}
		}

		const FVector Offset = Camera.GetLocation() - Pivot;
		float Radius = Offset.Size();
		if (Radius < 1.0f)
		{
			Radius = 10.0f;
		}

		float Azimuth = std::atan2(Offset.Y, Offset.X);
		float Elevation = std::asin(MathUtil::Clamp(Offset.Z / Radius, -1.0f, 1.0f));
		constexpr float OrbitSpeed = 0.0035f;
		Azimuth += DeltaX * OrbitSpeed;
		Elevation = MathUtil::Clamp(Elevation + DeltaY * OrbitSpeed, -1.4f, 1.4f);

		const float CosElevation = std::cos(Elevation);
		const FVector NextOffset(
			Radius * CosElevation * std::cos(Azimuth),
			Radius * CosElevation * std::sin(Azimuth),
			Radius * std::sin(Elevation));

		Camera.SetLocation(Pivot + NextOffset);
		Camera.SetLookAt(Pivot);
		SyncCameraTarget();
		return true;
	}

	return true;
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
	switch (TransformMode)
	{
	case ETransformMode::Select:
		SetTransformMode(ETransformMode::Translate);
		break;
	case ETransformMode::Translate:
		SetTransformMode(ETransformMode::Rotate);
		break;
	case ETransformMode::Rotate:
		SetTransformMode(ETransformMode::Scale);
		break;
	case ETransformMode::Scale:
	default:
		SetTransformMode(ETransformMode::Select);
		break;
	}
}

void FEditorViewportClient::CycleGizmoTransformMode()
{
	switch (TransformMode)
	{
	case ETransformMode::Translate:
		SetTransformMode(ETransformMode::Rotate);
		break;
	case ETransformMode::Rotate:
		SetTransformMode(ETransformMode::Scale);
		break;
	case ETransformMode::Select:
	case ETransformMode::Scale:
	default:
		SetTransformMode(ETransformMode::Translate);
		break;
	}
}

void FEditorViewportClient::ApplyTransformModeToGizmo()
{
	if (!Gizmo)
	{
		return;
	}

	if (TransformMode == ETransformMode::Select)
	{
		Gizmo->DragEnd();
		Gizmo->SetVisibility(false);
		return;
	}

	Gizmo->SetVisibility(Gizmo->HasTarget());

	switch (TransformMode)
	{
	case ETransformMode::Translate:
		Gizmo->SetTranslateMode();
		break;
	case ETransformMode::Rotate:
		Gizmo->SetRotateMode();
		break;
	case ETransformMode::Scale:
		Gizmo->SetScaleMode();
		break;
	case ETransformMode::Select:
	default:
		break;
	}
}

void FEditorViewportClient::SyncGizmoVisualState()
{
	if (!bHasCamera || !Gizmo)
	{
		return;
	}

	if (World && World->GetWorldType() == EWorldType::PIE)
	{
		return;
	}

	if (State && !State->bHovered && !bRoutedInputProcessedThisFrame)
	{
		return;
	}

	ApplyTransformModeToGizmo();

	if (TransformMode == ETransformMode::Select || !Gizmo->IsVisible())
	{
		return;
	}

	if (Camera.IsOrthographic())
	{
		Gizmo->ApplyScreenSpaceScalingOrtho(Camera.GetOrthoHeight());
	}
	else
	{
		Gizmo->ApplyScreenSpaceScaling(Camera.GetLocation());
	}
}

// ── Interaction (gizmo scaling + box selection) ───────────────────────────────

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

	// ── Box selection (Ctrl+Alt+LMB drag) ────────────────────────────────────
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

bool FEditorViewportClient::TryProjectWorldToViewport(const FVector& WorldPos, float& OutViewportX, float& OutViewportY, float& OutDepth) const
{
	const FVector4 Clip = FMatrix::Identity.TransformVector4(FVector4(WorldPos, 1.0f), Camera.GetViewProjectionMatrix());
	if (MathUtil::IsNearlyZero(Clip.W))
		return false;

	const float InvW = 1.0f / Clip.W;
	const float NdcX = Clip.X * InvW;
	const float NdcY = Clip.Y * InvW;
	const float NdcZ = Clip.Z * InvW;
	if (NdcX < -1.0f || NdcX > 1.0f || NdcY < -1.0f || NdcY > 1.0f)
		return false;

	OutViewportX = (NdcX * 0.5f + 0.5f) * WindowWidth;
	OutViewportY = (1.0f - (NdcY * 0.5f + 0.5f)) * WindowHeight;
	OutDepth     = NdcZ;
	return true;
}

void FEditorViewportClient::HandleBoxSelection()
{
	if (!SelectionManager || !World)
		return;

	const int32 MinX   = std::min(BoxSelectStart.x, BoxSelectEnd.x);
	const int32 MinY   = std::min(BoxSelectStart.y, BoxSelectEnd.y);
	const int32 MaxX   = std::max(BoxSelectStart.x, BoxSelectEnd.x);
	const int32 MaxY   = std::max(BoxSelectStart.y, BoxSelectEnd.y);
	const int32 Width  = MaxX - MinX;
	const int32 Height = MaxY - MinY;

	if (Width < 2 || Height < 2)
		return;

	if (!InputSystem::Get().GetKey(VK_SHIFT))
		SelectionManager->ClearSelection();

	TArray<UPrimitiveComponent*> CandidatePrimitives;
	World->GetSpatialIndex().FrustumQueryPrimitives(Camera.GetFrustum(), CandidatePrimitives, FrustumQueryScratch);

	std::unordered_set<AActor*> SeenActors;
	SeenActors.reserve(CandidatePrimitives.size());

	for (UPrimitiveComponent* Primitive : CandidatePrimitives)
	{
		AActor* Actor = (Primitive != nullptr) ? Primitive->GetOwner() : nullptr;
		if (!Actor || !Actor->GetRootComponent())
			continue;

		if (!SeenActors.insert(Actor).second)
			continue;

		float ViewportX = 0.f, ViewportY = 0.f, Depth = 0.f;
		if (!TryProjectWorldToViewport(Actor->GetActorLocation(), ViewportX, ViewportY, Depth))
			continue;

		if (Depth < 0.f || Depth > 1.f)
			continue;

		const int32 Px = static_cast<int32>(ViewportX);
		const int32 Py = static_cast<int32>(ViewportY);
		if (Px >= MinX && Px <= MaxX && Py >= MinY && Py <= MaxY)
			SelectionManager->AddSelect(Actor);
	}
}

void FEditorViewportClient::FocusPrimarySelection()
{
	if (!SelectionManager || !bHasCamera)
		return;

	AActor* Primary = SelectionManager->GetPrimarySelection();
	if (!Primary)
		return;

	const FVector Target = Primary->GetActorLocation();

	if (Camera.IsOrthographic())
	{
		const FVector Forward  = Camera.GetEffectiveForward().GetSafeNormal();
		float         Distance = FVector::DotProduct(Camera.GetLocation() - Target, Forward);
		if (MathUtil::IsNearlyZero(Distance))
			Distance = 1000.f;
		Camera.SetLocation(Target + Forward * Distance);
	}
	else
	{
		const FVector Forward = Camera.GetForwardVector().GetSafeNormal();
		Camera.SetLocation(Target - Forward * 5.f);
		Camera.SetLookAt(Target);
	}

	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
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
	if (Editor)
	{
		Editor->StopPlaySession();
	}
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

	MarkPIEMouseFocusReleased();
	bControlLocked = false;
	InputSystem& IS = InputSystem::Get();
	IS.SetCursorVisibility(true);
	IS.LockMouse(false);
}

bool FEditorViewportClient::ExecutePIEShellCommand(EPIESessionShellCommand Command)
{
	return Editor && Editor->GetPIESession().ExecuteShellCommand(Command, *this);
}

bool FEditorViewportClient::IsPIEMouseFocusReleased() const
{
	return Editor && Editor->GetPIESession().IsMouseFocusReleased();
}

void FEditorViewportClient::MarkPIEMouseFocusReleased()
{
	if (Editor)
	{
		Editor->GetPIESession().MarkMouseFocusReleased();
	}
}

void FEditorViewportClient::ClearPIEMouseFocusReleased()
{
	if (Editor)
	{
		Editor->GetPIESession().ClearMouseFocusReleased();
	}
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
	if (!IsPIEActive())
	{
		return;
	}

	InputRouter.GetEditorWorldController().SetWorld(World);
	InputRouter.GetEditorWorldController().SetCamera(&Camera);
	InputRouter.GetEditorWorldController().SetTargetLocation(InputRouter.GetGameInputBridge().GetTargetLocation());
	InputRouter.GetEditorWorldController().SetTargetRotation(Camera.GetRotation());
	InputRouter.SetActiveController(EActiveEditorController::EditorWorldController);
	if (World)
	{
		World->SetActiveCamera(&Camera);
	}

	bControlLocked = false;
	ClearPIEMouseFocusReleased();
	if (Editor)
	{
		Editor->GetPIESession().SetControlMode(EPIESessionControlMode::EditorControl);
	}
	InputSystem& IS = InputSystem::Get();
	IS.SetCursorVisibility(true);
	IS.LockMouse(false);
}

void FEditorViewportClient::EnterPIEPossessedMode()
{
	if (!IsPIEActive())
	{
		return;
	}

	InputRouter.GetGameInputBridge().SetCamera(&Camera);
	InputRouter.GetGameInputBridge().SetTargetLocation(InputRouter.GetEditorWorldController().GetTargetLocation());
	InputRouter.SetActiveController(EActiveEditorController::GameInputBridge);
	if (Editor)
	{
		Editor->GetPIESession().SetControlMode(EPIESessionControlMode::Possessed);
	}
	if (GEngine)
	{
		GEngine->SetRuntimeInputMode(ERuntimeInputMode::GameOnly);
	}
	if (World)
	{
		APlayerController* PlayerController = GetPIEPlayerController();
		World->SetActiveCamera(PlayerController ? PlayerController->GetRuntimeCamera() : &Camera);
	}

	bControlLocked = false;
	ClearPIEMouseFocusReleased();
	InputSystem& IS = InputSystem::Get();
	IS.SetCursorVisibility(false);
	LockCursorToViewport();
}

void FEditorViewportClient::SaveCameraSnapshot()
{
	FViewportCamera CameraStae;
}

void FEditorViewportClient::RestoreCameraSnapshot()
{

}
