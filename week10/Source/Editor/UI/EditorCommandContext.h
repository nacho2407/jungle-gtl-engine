#pragma once

#include "Core/CoreMinimal.h"

#include <functional>

enum class EEditorCommandId : uint8
{
	None,

	Save,
	SaveAs,
	CloseTab,

	PlayPIE,
	StopPIE,
	BuildGame,

	AddTrack,
	AddKey,
	DeleteSelection,

	ResetPreviewCamera,
	ResetPose,
	ToggleBones,
	ToggleBounds,
	ToggleGrid,
	ToggleSockets,
	ToggleBonePicking,
};

struct FEditorShortcut
{
	int32 Key = 0;
	bool bCtrl = false;
	bool bShift = false;
	bool bAlt = false;

	bool IsValid() const;
	bool Matches(const FEditorShortcut& Other) const;
};

struct FEditorCommandBinding
{
	EEditorCommandId CommandId = EEditorCommandId::None;
	FEditorShortcut Shortcut;
	std::function<void()> Execute;
	std::function<bool()> CanExecute;
};

class FEditorCommandList
{
public:
	void Clear();
	void MapAction(
		EEditorCommandId CommandId,
		const FEditorShortcut& Shortcut,
		std::function<void()> Execute,
		std::function<bool()> CanExecute = {});

	bool TryExecuteShortcut(const FEditorShortcut& Shortcut) const;
	bool TryExecuteCommand(EEditorCommandId CommandId) const;
	bool CanExecute(EEditorCommandId CommandId) const;

private:
	TArray<FEditorCommandBinding> Bindings;
};
