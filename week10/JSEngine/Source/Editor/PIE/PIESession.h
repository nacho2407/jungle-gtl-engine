#pragma once

#include "Core/Containers/Map.h"
#include "Core/EngineTypes.h"
#include "Object/FName.h"

class APlayerController;

enum class EPIESessionShellCommand
{
	EndPlay,
	TogglePossessEject,
	ReleaseMouseFocus
};

enum class EPIESessionControlMode
{
	Inactive,
	Possessed,
	EditorControl
};

class IPIESessionShellCommandHandler
{
public:
	virtual ~IPIESessionShellCommandHandler() = default;

	virtual void RequestEndPIE() = 0;
	virtual void TogglePIEPossessEject() = 0;
	virtual void ReleasePIEMouseFocus() = 0;
};

class FPIESession
{
public:
	int32 GetActiveViewportIndex() const { return ActiveViewportIndex; }
	void SetActiveViewportIndex(int32 ViewportIndex) { ActiveViewportIndex = ViewportIndex; }
	void ClearActiveViewportIndex() { ActiveViewportIndex = -1; }
	EPIESessionControlMode GetControlMode() const { return ControlMode; }
	void SetControlMode(EPIESessionControlMode InControlMode) { ControlMode = InControlMode; }
	void ClearControlMode() { ControlMode = EPIESessionControlMode::Inactive; }
	bool IsPossessed() const { return ControlMode == EPIESessionControlMode::Possessed; }
	bool IsEditorControl() const { return ControlMode == EPIESessionControlMode::EditorControl; }
	bool IsMouseFocusReleased() const { return bMouseFocusReleased; }
	void MarkMouseFocusReleased() { bMouseFocusReleased = true; }
	void ClearMouseFocusReleased() { bMouseFocusReleased = false; }
	APlayerController* GetPlayerController() const { return PlayerController; }
	void SetPlayerController(APlayerController* InPlayerController) { PlayerController = InPlayerController; }
	void ClearPlayerController() { PlayerController = nullptr; }

	int32 ResolveActiveViewportIndex(int32 FallbackViewportIndex) const;
	int32 ResolveRegisteredViewportIndex(int32 PreferredViewportIndex) const;

	void RegisterViewportWorld(int32 ViewportIndex, const FName& WorldHandle);
	bool FindViewportWorldHandle(int32 ViewportIndex, FName& OutWorldHandle) const;
	bool RemoveViewportWorld(int32 ViewportIndex, FName& OutWorldHandle);
	bool HasViewportWorld(int32 ViewportIndex) const;
	bool HasAnyViewportWorld() const { return !ViewportWorldHandles.empty(); }

	void RequestViewportInputFocus(int32 FrameCount);
	bool HasPendingViewportInputFocus() const { return PendingViewportFocusFrames > 0; }
	void ConsumeViewportInputFocusFrame();
	bool ExecuteShellCommand(
		EPIESessionShellCommand Command,
		IPIESessionShellCommandHandler& Handler) const;

private:
	TMap<int32, FName> ViewportWorldHandles;
	EPIESessionControlMode ControlMode = EPIESessionControlMode::Inactive;
	APlayerController* PlayerController = nullptr;
	int32 ActiveViewportIndex = -1;
	int32 PendingViewportFocusFrames = 0;
	bool bMouseFocusReleased = false;
};
