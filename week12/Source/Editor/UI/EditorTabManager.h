#pragma once

#include "Core/CoreMinimal.h"

enum class EEditorTabKind : uint8
{
	LevelEditor,
	StaticMeshViewer,
	SkeletalMeshViewer,
	AnimSequenceViewer,
	ParticleViewer,
	MaterialEditor,
	CurveEditor,
	ActorSequencer,
	RuntimeUIPreview,
	AnimGraphEditor,
};

struct FEditorTabId
{
	EEditorTabKind Kind = EEditorTabKind::LevelEditor;
	FString PayloadId;

	bool Matches(const FEditorTabId& Other) const;
};

struct FEditorTabEntry
{
	FEditorTabId Id;
	FString Label;
	bool bCanClose = true;
	bool bDirty = false;
	bool bDetached = false;
};

FEditorTabId MakeEditorViewerTabId(const FString& ViewerFileName, const void* FallbackAddress = nullptr);
FString MakeEditorViewerTabLabel(const FString& ViewerFileName);
FEditorTabId MakeRuntimeUIPreviewTabId();
FString MakeRuntimeUIPreviewTabLabel(const FString& DocumentPath);
FEditorTabId MakeAnimGraphEditorTabId(const FString& AnimGraphPath);
FString MakeAnimGraphEditorTabLabel(const FString& AnimGraphPath);

class FEditorTabManager
{
public:
	void ResetToLevelEditor();

	const FEditorTabEntry& OpenOrFocusTab(const FEditorTabId& Id, const FString& Label, bool bCanClose = true);
	bool CloseTab(const FEditorTabId& Id);
	bool SetActiveTab(const FEditorTabId& Id);
	bool ReplaceTab(const FEditorTabId& OldId, const FEditorTabId& NewId, const FString& Label);
	bool SetTabLabel(const FEditorTabId& Id, const FString& Label);
	bool SetTabDirty(const FEditorTabId& Id, bool bDirty);

	const FEditorTabEntry* GetActiveTab() const;
	EEditorTabKind GetActiveTabKind() const;
	const TArray<FEditorTabEntry>& GetTabs() const;
	bool MoveTab(int32 FromIndex, int32 ToIndex);
	bool MoveTab(const FEditorTabId& FromId, const FEditorTabId& ToId);
	bool SetTabDetached(const FEditorTabId& Id, bool bDetached);
	bool IsTabDetached(const FEditorTabId& Id) const;
	const FEditorTabEntry* FindTab(const FEditorTabId& Id) const;

private:
	int32 FindTabIndex(const FEditorTabId& Id) const;

	TArray<FEditorTabEntry> Tabs;
	int32 ActiveTabIndex = -1;
};
