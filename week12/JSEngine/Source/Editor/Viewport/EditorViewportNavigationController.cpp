#include "Editor/Viewport/EditorViewportNavigationController.h"

#include "Camera/ViewportCamera.h"
#include "Component/GizmoComponent.h"
#include "Editor/Input/EditorInputRouter.h"
#include "Editor/Input/EditorViewportInputMapping.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "GameFramework/AActor.h"
#include "Math/Utils.h"

#include <cmath>

bool FEditorViewportNavigationController::FocusPrimarySelection(
	FSelectionManager* SelectionManager,
	FViewportCamera* Camera)
{
	if (!SelectionManager || !Camera)
	{
		return false;
	}

	AActor* Primary = SelectionManager->GetPrimarySelection();
	if (!Primary)
	{
		return false;
	}

	const FVector Target = Primary->GetActorLocation();

	if (Camera->IsOrthographic())
	{
		const FVector Forward = Camera->GetEffectiveForward().GetSafeNormal();
		float Distance = FVector::DotProduct(Camera->GetLocation() - Target, Forward);
		if (MathUtil::IsNearlyZero(Distance))
		{
			Distance = 1000.f;
		}
		Camera->SetLocation(Target + Forward * Distance);
	}
	else
	{
		const FVector Forward = Camera->GetForwardVector().GetSafeNormal();
		Camera->SetLocation(Target - Forward * 5.f);
		Camera->SetLookAt(Target);
	}

	return true;
}

bool FEditorViewportNavigationController::ResetCamera(
	FViewportCamera* Camera,
	const FVector& InitViewPos,
	const FVector& InitLookAt)
{
	if (!Camera)
	{
		return false;
	}

	Camera->SetLocation(InitViewPos);

	const FVector Forward = (InitLookAt - InitViewPos).GetSafeNormal();
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
			Camera->SetRotation(NewRotation);
		}
	}

	return true;
}

void FEditorViewportNavigationController::ApplyCameraMode(FViewportCamera& Camera, int32 ViewportType)
{
	Camera.SetRotation(FRotator(0.f, 0.f, 0.f));

	switch (ViewportType)
	{
	case EVT_Perspective:
		Camera.SetProjectionType(EViewportProjectionType::Perspective);
		Camera.ClearCustomLookDir();
		Camera.SetLocation(FVector(5.f, 3.f, 5.f));
		Camera.SetLookAt(FVector(0.f, 0.f, 0.f));
		break;

	case EVT_OrthoTop:
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 0.f, 1000.f));
		Camera.SetCustomLookDir(FVector(0.f, 0.f, -1.f), FVector(1.f, 0.f, 0.f));
		break;

	case EVT_OrthoBottom:
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 0.f, -1000.f));
		Camera.SetCustomLookDir(FVector(0.f, 0.f, 1.f), FVector(1.f, 0.f, 0.f));
		break;

	case EVT_OrthoFront:
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(1000.f, 0.f, 0.f));
		Camera.SetCustomLookDir(FVector(-1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoBack:
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(-1000.f, 0.f, 0.f));
		Camera.SetCustomLookDir(FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoLeft:
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, -1000.f, 0.f));
		Camera.SetCustomLookDir(FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	case EVT_OrthoRight:
		Camera.SetProjectionType(EViewportProjectionType::Orthographic);
		Camera.SetLocation(FVector(0.f, 1000.f, 0.f));
		Camera.SetCustomLookDir(FVector(0.f, -1.f, 0.f), FVector(0.f, 0.f, 1.f));
		break;

	default:
		break;
	}
}

bool FEditorViewportNavigationController::HandleLeftNavigationInput(
	const FViewportInputContext& Context,
	FViewportCamera& Camera,
	FEditorInputRouter& InputRouter,
	UGizmoComponent* Gizmo,
	bool bEditorWorldControllerActive,
	bool bControlLocked,
	bool bHasCamera,
	bool bPointerInDeadZone,
	bool bLeftNavigationChord,
	float MoveSpeed)
{
	if (!bEditorWorldControllerActive || bControlLocked || !bHasCamera)
	{
		return false;
	}
	if (bPointerInDeadZone || Context.bImGuiCapturedMouse || Camera.IsOrthographic())
	{
		return false;
	}
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

	const float ForwardScale = MoveSpeed * Settings.CameraDollySpeedScale * 0.01f;
	const FVector NextLocation = Camera.GetLocation() + NextForward * (-DeltaY * ForwardScale);
	Camera.SetLocation(NextLocation);
	InputRouter.GetEditorWorldController().SetTargetLocation(NextLocation);
	InputRouter.GetEditorWorldController().SetTargetRotation(AppliedRotation);
	return true;
}

bool FEditorViewportNavigationController::HandleMoveSpeedWheel(
	const FViewportInputContext& Context,
	bool bEditorWorldControllerActive,
	float CurrentMoveSpeed,
	float& OutMoveSpeed)
{
	if (!bEditorWorldControllerActive
		|| !Context.Frame.IsDown(VK_RBUTTON)
		|| MathUtil::IsNearlyZero(Context.Frame.WheelNotches))
	{
		return false;
	}

	const float WheelScale = std::pow(1.15f, static_cast<float>(Context.Frame.WheelNotches));
	OutMoveSpeed = MathUtil::Clamp(CurrentMoveSpeed * WheelScale, 0.1f, 5000.0f);
	return true;
}

bool FEditorViewportNavigationController::HandleAltNavigationInput(
	const FViewportInputContext& Context,
	FViewportCamera& Camera,
	FSelectionManager* SelectionManager,
	bool bEditorWorldControllerActive,
	bool bControlLocked,
	bool bHasCamera,
	float MoveSpeed,
	bool& bOutSyncCameraTarget)
{
	using EEditorViewportAction = EditorViewportInputMapping::EEditorViewportAction;

	bOutSyncCameraTarget = false;

	if (!bEditorWorldControllerActive || bControlLocked || !bHasCamera)
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
			: MoveSpeed * 0.002f * Settings.CameraPanSpeedScale;
		const FVector Right = Camera.GetEffectiveRight();
		const FVector Up = Camera.GetEffectiveUp();
		Camera.SetLocation(Camera.GetLocation() + Right * (DeltaX * PanScale) + Up * (-DeltaY * PanScale));
		bOutSyncCameraTarget = true;
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
			const float DollyScale = MoveSpeed * FEditorSettings::Get().CameraDollySpeedScale * 0.01f;
			Camera.SetLocation(Camera.GetLocation() + Camera.GetForwardVector().GetSafeNormal() * (-DeltaY * DollyScale));
			bOutSyncCameraTarget = true;
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
		bOutSyncCameraTarget = true;
		return true;
	}

	return true;
}

void FEditorViewportNavigationController::RouteKeyboardNavigationInput(
	const FViewportInputContext& Context,
	FEditorInputRouter& InputRouter,
	bool bControlLocked,
	bool bRuntimeGameInputCaptured)
{
	static constexpr int WatchKeys[] = {
		'W', 'A', 'S', 'D', 'Q', 'E',
		VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
		VK_SPACE,
	};

	if (bControlLocked)
	{
		return;
	}
	if (InputRouter.GetActiveController() == EActiveEditorController::GameInputBridge && !bRuntimeGameInputCaptured)
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
		if (Context.WasPressed(VK))
		{
			InputRouter.RouteKeyboardInput(EKeyInputType::KeyPressed, VK);
		}
		if (Context.Frame.IsDown(VK))
		{
			InputRouter.RouteKeyboardInput(EKeyInputType::KeyDown, VK);
		}
		if (Context.WasReleased(VK))
		{
			InputRouter.RouteKeyboardInput(EKeyInputType::KeyReleased, VK);
		}
	}
}

bool FEditorViewportNavigationController::IsEditorCameraKeyboardKey(int VK)
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
