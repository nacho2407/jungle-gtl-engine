#include "Editor/Viewport/EditorViewportPIEController.h"

#include "Camera/ViewportCamera.h"
#include "Editor/Input/EditorInputRouter.h"
#include "EditorEngine.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"

void FEditorViewportPIEController::StartPIE(
	UEditorEngine* Editor,
	UWorld* World,
	FEditorInputRouter& InputRouter,
	FViewportCamera& Camera,
	bool& bControlLocked)
{
	InputRouter.GetEditorWorldController().SetWorld(World);
	InputRouter.GetGameInputBridge().SetCamera(&Camera);
	InputRouter.GetGameInputBridge().SetTargetLocation(InputRouter.GetEditorWorldController().GetTargetLocation());
	InputRouter.SetActiveController(EActiveEditorController::GameInputBridge);
	if (Editor)
	{
		Editor->GetPIESession().SetControlMode(EPIESessionControlMode::Possessed);
		Editor->GetPIESession().ClearMouseFocusReleased();
	}
	bControlLocked = false;
}

void FEditorViewportPIEController::EndPIE(
	UEditorEngine* Editor,
	UWorld* World,
	FEditorInputRouter& InputRouter,
	FViewportCamera& Camera,
	bool& bControlLocked)
{
	if (InputRouter.GetActiveController() == EActiveEditorController::GameInputBridge)
	{
		InputRouter.GetEditorWorldController().SetTargetLocation(InputRouter.GetGameInputBridge().GetTargetLocation());
	}
	InputRouter.GetEditorWorldController().SetWorld(World);
	InputRouter.SetActiveController(EActiveEditorController::EditorWorldController);
	InputRouter.GetEditorWorldController().ResetTargetFromCamera();
	ClearPlayerController(Editor, InputRouter);
	InputSystem::Get().LockMouse(false);
	bControlLocked = false;
	ClearMouseFocusReleased(Editor);
	if (Editor)
	{
		Editor->GetPIESession().ClearControlMode();
	}
}

bool FEditorViewportPIEController::IsPossessed(
	bool bPIEActive,
	UEditorEngine* Editor,
	const FEditorInputRouter& InputRouter)
{
	if (!bPIEActive)
	{
		return false;
	}

	if (Editor)
	{
		return Editor->GetPIESession().GetControlMode() == EPIESessionControlMode::Possessed;
	}

	return InputRouter.GetActiveController() == EActiveEditorController::GameInputBridge;
}

bool FEditorViewportPIEController::IsEditorControlMode(
	bool bPIEActive,
	UEditorEngine* Editor,
	const FEditorInputRouter& InputRouter)
{
	if (!bPIEActive)
	{
		return false;
	}

	if (Editor)
	{
		return Editor->GetPIESession().GetControlMode() == EPIESessionControlMode::EditorControl;
	}

	return InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController;
}

bool FEditorViewportPIEController::AllowsEditorWorldControl(
	bool bPIEActive,
	UEditorEngine* Editor,
	const FEditorInputRouter& InputRouter)
{
	if (!bPIEActive)
	{
		return InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController;
	}

	if (Editor)
	{
		return Editor->GetPIESession().GetControlMode() == EPIESessionControlMode::EditorControl;
	}

	return InputRouter.GetActiveController() == EActiveEditorController::EditorWorldController;
}

APlayerController* FEditorViewportPIEController::GetPlayerController(
	UEditorEngine* Editor,
	const FEditorInputRouter& InputRouter)
{
	return Editor ? Editor->GetPIESession().GetPlayerController() : InputRouter.GetGameInputBridge().GetPlayerController();
}

void FEditorViewportPIEController::SetPlayerController(
	UEditorEngine* Editor,
	FEditorInputRouter& InputRouter,
	APlayerController* Controller)
{
	if (Editor)
	{
		Editor->GetPIESession().SetPlayerController(Controller);
	}
	InputRouter.GetGameInputBridge().SetPlayerController(Controller);
}

void FEditorViewportPIEController::ClearPlayerController(UEditorEngine* Editor, FEditorInputRouter& InputRouter)
{
	if (Editor)
	{
		Editor->GetPIESession().ClearPlayerController();
	}
	InputRouter.GetGameInputBridge().ClearPlayerController();
}

void FEditorViewportPIEController::RequestEndPIE(UEditorEngine* Editor)
{
	if (Editor)
	{
		Editor->StopPlaySession();
	}
}

bool FEditorViewportPIEController::ExecuteShellCommand(
	UEditorEngine* Editor,
	EPIESessionShellCommand Command,
	IPIESessionShellCommandHandler& Handler)
{
	return Editor && Editor->GetPIESession().ExecuteShellCommand(Command, Handler);
}

bool FEditorViewportPIEController::IsMouseFocusReleased(UEditorEngine* Editor)
{
	return Editor && Editor->GetPIESession().IsMouseFocusReleased();
}

void FEditorViewportPIEController::MarkMouseFocusReleased(UEditorEngine* Editor)
{
	if (Editor)
	{
		Editor->GetPIESession().MarkMouseFocusReleased();
	}
}

void FEditorViewportPIEController::ClearMouseFocusReleased(UEditorEngine* Editor)
{
	if (Editor)
	{
		Editor->GetPIESession().ClearMouseFocusReleased();
	}
}

void FEditorViewportPIEController::ReleaseMouseFocus(UEditorEngine* Editor, bool& bControlLocked)
{
	MarkMouseFocusReleased(Editor);
	bControlLocked = false;
	InputSystem& IS = InputSystem::Get();
	IS.SetCursorVisibility(true);
	IS.LockMouse(false);
}

void FEditorViewportPIEController::EnterEditorControlMode(
	bool bPIEActive,
	UEditorEngine* Editor,
	UWorld* World,
	FEditorInputRouter& InputRouter,
	FViewportCamera& Camera,
	bool& bControlLocked)
{
	if (!bPIEActive)
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
	ClearMouseFocusReleased(Editor);
	if (Editor)
	{
		Editor->GetPIESession().SetControlMode(EPIESessionControlMode::EditorControl);
	}
	InputSystem& IS = InputSystem::Get();
	IS.SetCursorVisibility(true);
	IS.LockMouse(false);
}

void FEditorViewportPIEController::EnterPossessedMode(
	bool bPIEActive,
	UEditorEngine* Editor,
	UWorld* World,
	FEditorInputRouter& InputRouter,
	FViewportCamera& Camera,
	bool& bControlLocked)
{
	if (!bPIEActive)
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
		APlayerController* PlayerController = GetPlayerController(Editor, InputRouter);
		World->SetActiveCamera(PlayerController ? PlayerController->GetRuntimeCamera() : &Camera);
	}

	bControlLocked = false;
	ClearMouseFocusReleased(Editor);
	InputSystem::Get().SetCursorVisibility(false);
}
