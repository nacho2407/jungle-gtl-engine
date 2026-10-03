#include "Editor/UI/EditorTabManager.h"

#include <algorithm>
#include <cstdio>
#include <utility>

bool FEditorTabId::Matches(const FEditorTabId& Other) const
{
	return Kind == Other.Kind && PayloadId == Other.PayloadId;
}

FEditorTabId MakeEditorViewerTabId(const FString& ViewerFileName, const void* FallbackAddress)
{
	FEditorTabId TabId;
	TabId.Kind = EEditorTabKind::SkeletalMeshViewer;
	TabId.PayloadId = ViewerFileName;
	if (TabId.PayloadId.empty() && FallbackAddress)
	{
		char Buffer[64];
		snprintf(Buffer, sizeof(Buffer), "__Viewer_%p", FallbackAddress);
		TabId.PayloadId = Buffer;
	}
	return TabId;
}

FString MakeEditorViewerTabLabel(const FString& ViewerFileName)
{
	if (ViewerFileName.empty())
	{
		return "Skeletal Mesh Viewer";
	}

	const size_t SlashIndex = ViewerFileName.find_last_of("/\\");
	return SlashIndex == FString::npos ? ViewerFileName : ViewerFileName.substr(SlashIndex + 1);
}

FEditorTabId MakeRuntimeUIPreviewTabId()
{
	FEditorTabId TabId;
	TabId.Kind = EEditorTabKind::RuntimeUIPreview;
	TabId.PayloadId = "__RuntimeUIPreview";
	return TabId;
}

FString MakeRuntimeUIPreviewTabLabel(const FString& DocumentPath)
{
	if (DocumentPath.empty())
	{
		return "Runtime UI Preview";
	}

	const size_t SlashIndex = DocumentPath.find_last_of("/\\");
	const FString FileName = SlashIndex == FString::npos ? DocumentPath : DocumentPath.substr(SlashIndex + 1);
	return FileName.empty() ? "Runtime UI Preview" : FileName;
}

void FEditorTabManager::ResetToLevelEditor()
{
	Tabs.clear();

	FEditorTabEntry LevelEditorTab;
	LevelEditorTab.Id.Kind = EEditorTabKind::LevelEditor;
	LevelEditorTab.Label = "Untitled";
	LevelEditorTab.bCanClose = false;

	Tabs.emplace_back(std::move(LevelEditorTab));
	ActiveTabIndex = 0;
}

const FEditorTabEntry& FEditorTabManager::OpenOrFocusTab(const FEditorTabId& Id, const FString& Label, bool bCanClose)
{
	const int32 ExistingIndex = FindTabIndex(Id);
	if (ExistingIndex >= 0)
	{
		ActiveTabIndex = ExistingIndex;
		return Tabs[ExistingIndex];
	}

	FEditorTabEntry NewTab;
	NewTab.Id = Id;
	NewTab.Label = Label;
	NewTab.bCanClose = bCanClose;

	Tabs.emplace_back(std::move(NewTab));
	ActiveTabIndex = static_cast<int32>(Tabs.size()) - 1;
	return Tabs[ActiveTabIndex];
}

bool FEditorTabManager::CloseTab(const FEditorTabId& Id)
{
	const int32 Index = FindTabIndex(Id);
	if (Index < 0 || !Tabs[Index].bCanClose)
	{
		return false;
	}

	Tabs.erase(Tabs.begin() + Index);
	if (Tabs.empty())
	{
		ResetToLevelEditor();
		return true;
	}

	if (ActiveTabIndex >= Index)
	{
		ActiveTabIndex = std::max(0, ActiveTabIndex - 1);
	}
	if (ActiveTabIndex >= static_cast<int32>(Tabs.size()))
	{
		ActiveTabIndex = static_cast<int32>(Tabs.size()) - 1;
	}

	return true;
}

bool FEditorTabManager::SetActiveTab(const FEditorTabId& Id)
{
	const int32 Index = FindTabIndex(Id);
	if (Index < 0)
	{
		return false;
	}

	ActiveTabIndex = Index;
	return true;
}

bool FEditorTabManager::SetTabLabel(const FEditorTabId& Id, const FString& Label)
{
	const int32 Index = FindTabIndex(Id);
	if (Index < 0)
	{
		return false;
	}

	Tabs[Index].Label = Label;
	return true;
}

const FEditorTabEntry* FEditorTabManager::GetActiveTab() const
{
	if (ActiveTabIndex < 0 || ActiveTabIndex >= static_cast<int32>(Tabs.size()))
	{
		return nullptr;
	}

	return &Tabs[ActiveTabIndex];
}

EEditorTabKind FEditorTabManager::GetActiveTabKind() const
{
	const FEditorTabEntry* ActiveTab = GetActiveTab();
	return ActiveTab ? ActiveTab->Id.Kind : EEditorTabKind::LevelEditor;
}

const TArray<FEditorTabEntry>& FEditorTabManager::GetTabs() const
{
	return Tabs;
}

bool FEditorTabManager::MoveTab(int32 FromIndex, int32 ToIndex)
{
	if (FromIndex <= 0 || ToIndex <= 0 ||
		FromIndex >= static_cast<int32>(Tabs.size()) ||
		ToIndex >= static_cast<int32>(Tabs.size()) ||
		FromIndex == ToIndex)
	{
		return false;
	}

	FEditorTabId ActiveId;
	const bool bHadActive = GetActiveTab() != nullptr;
	if (bHadActive)
	{
		ActiveId = Tabs[ActiveTabIndex].Id;
	}

	FEditorTabEntry MovingTab = std::move(Tabs[FromIndex]);
	Tabs.erase(Tabs.begin() + FromIndex);
	Tabs.insert(Tabs.begin() + ToIndex, std::move(MovingTab));

	if (bHadActive)
	{
		ActiveTabIndex = FindTabIndex(ActiveId);
	}
	return true;
}

bool FEditorTabManager::MoveTab(const FEditorTabId& FromId, const FEditorTabId& ToId)
{
	const int32 FromIndex = FindTabIndex(FromId);
	const int32 ToIndex = FindTabIndex(ToId);
	return MoveTab(FromIndex, ToIndex);
}

bool FEditorTabManager::SetTabDetached(const FEditorTabId& Id, bool bDetached)
{
	const int32 Index = FindTabIndex(Id);
	if (Index <= 0 || Index >= static_cast<int32>(Tabs.size()))
	{
		return false;
	}

	Tabs[Index].bDetached = bDetached;
	return true;
}

bool FEditorTabManager::IsTabDetached(const FEditorTabId& Id) const
{
	const int32 Index = FindTabIndex(Id);
	return Index >= 0 && Tabs[Index].bDetached;
}

const FEditorTabEntry* FEditorTabManager::FindTab(const FEditorTabId& Id) const
{
	const int32 Index = FindTabIndex(Id);
	return Index >= 0 ? &Tabs[Index] : nullptr;
}

int32 FEditorTabManager::FindTabIndex(const FEditorTabId& Id) const
{
	for (int32 Index = 0; Index < static_cast<int32>(Tabs.size()); ++Index)
	{
		if (Tabs[Index].Id.Matches(Id))
		{
			return Index;
		}
	}

	return -1;
}
