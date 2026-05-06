#include "Editor/Undo/EditorUndoSystem.h"

#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorMainPanel.h"

#include <utility>

bool FEditorUndoSystem::CaptureSnapshot(const char* Reason)
{
	if (Owner == nullptr || IsRestoring() || Owner->GetEditorState() != EViewportPlayState::Editing)
	{
		return false;
	}

	FString Snapshot = Owner->CaptureSceneSnapshot();
	if (Snapshot.empty())
	{
		return false;
	}

	bool bClearedRedo = false;
	const bool bCaptured = PushSnapshot(std::move(Snapshot), Reason, bClearedRedo);
	if (bClearedRedo)
	{
		Owner->GetMainPanel().PushFooterLog("Redo history cleared");
	}
	return bCaptured;
}

bool FEditorUndoSystem::Undo()
{
	if (Owner == nullptr || IsRestoring() || Owner->GetEditorState() != EViewportPlayState::Editing)
	{
		return false;
	}

	FString Current = Owner->CaptureSceneSnapshot();
	FUndoSnapshotEntry Previous;
	if (!PopUndoSnapshot(std::move(Current), Previous))
	{
		return false;
	}

	const bool bRestored = Owner->RestoreSceneSnapshot(Previous.Snapshot);
	if (bRestored)
	{
		Owner->GetMainPanel().PushFooterLog("Undo: " + Previous.Label);
	}
	return bRestored;
}

bool FEditorUndoSystem::Redo()
{
	if (Owner == nullptr || IsRestoring() || Owner->GetEditorState() != EViewportPlayState::Editing)
	{
		return false;
	}

	FString Current = Owner->CaptureSceneSnapshot();
	FUndoSnapshotEntry Next;
	if (!PopRedoSnapshot(std::move(Current), Next))
	{
		return false;
	}

	const bool bRestored = Owner->RestoreSceneSnapshot(Next.Snapshot);
	if (bRestored)
	{
		Owner->GetMainPanel().PushFooterLog("Redo: " + Next.Label);
	}
	return bRestored;
}

bool FEditorUndoSystem::RestoreHistoryIndex(int32 Index)
{
	if (Owner == nullptr || IsRestoring() || Owner->GetEditorState() != EViewportPlayState::Editing)
	{
		return false;
	}

	FString Current = Owner->CaptureSceneSnapshot();
	FUndoSnapshotEntry Target;
	if (!RestoreHistorySnapshotIndex(Index, std::move(Current), Target))
	{
		return false;
	}

	const bool bRestored = Owner->RestoreSceneSnapshot(Target.Snapshot);
	if (bRestored)
	{
		Owner->GetMainPanel().PushFooterLog("History restored: " + Target.Label);
	}
	return bRestored;
}

void FEditorUndoSystem::ClearHistory()
{
	if (Owner == nullptr)
	{
		ClearStorage();
		return;
	}

	if (ClearStorage())
	{
		Owner->GetMainPanel().PushFooterLog("Undo history cleared");
	}
}

bool FEditorUndoSystem::PushSnapshot(FString Snapshot, const char* Reason, bool& bOutClearedRedo)
{
	bOutClearedRedo = false;
	if (Snapshot.empty())
	{
		return false;
	}

	if (!UndoHistory.empty() && UndoHistory.back().Snapshot == Snapshot)
	{
		return false;
	}

	FUndoSnapshotEntry Entry;
	Entry.Label = (Reason && Reason[0] != '\0') ? Reason : "Scene Edit";
	Entry.Snapshot = std::move(Snapshot);
	PushWithLimit(UndoHistory, std::move(Entry));

	if (!RedoHistory.empty())
	{
		RedoHistory.clear();
		bOutClearedRedo = true;
	}
	return true;
}

bool FEditorUndoSystem::PopUndoSnapshot(FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry)
{
	if (UndoHistory.empty())
	{
		return false;
	}

	OutEntry = std::move(UndoHistory.back());
	UndoHistory.pop_back();

	if (!CurrentSnapshot.empty())
	{
		FUndoSnapshotEntry CurrentEntry;
		CurrentEntry.Label = "Current";
		CurrentEntry.Snapshot = std::move(CurrentSnapshot);
		PushWithLimit(RedoHistory, std::move(CurrentEntry));
	}
	return true;
}

bool FEditorUndoSystem::PopRedoSnapshot(FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry)
{
	if (RedoHistory.empty())
	{
		return false;
	}

	OutEntry = std::move(RedoHistory.back());
	RedoHistory.pop_back();

	if (!CurrentSnapshot.empty())
	{
		FUndoSnapshotEntry CurrentEntry;
		CurrentEntry.Label = "Current";
		CurrentEntry.Snapshot = std::move(CurrentSnapshot);
		PushWithLimit(UndoHistory, std::move(CurrentEntry));
	}
	return true;
}

bool FEditorUndoSystem::RestoreHistorySnapshotIndex(int32 Index, FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry)
{
	if (Index < 0 || Index >= static_cast<int32>(UndoHistory.size()))
	{
		return false;
	}

	FUndoSnapshotEntry Target = UndoHistory[Index];

	RedoHistory.clear();
	if (!CurrentSnapshot.empty())
	{
		FUndoSnapshotEntry CurrentEntry;
		CurrentEntry.Label = "Current";
		CurrentEntry.Snapshot = std::move(CurrentSnapshot);
		PushWithLimit(RedoHistory, std::move(CurrentEntry));
	}

	for (int32 HistoryIndex = static_cast<int32>(UndoHistory.size()) - 1; HistoryIndex > Index; --HistoryIndex)
	{
		PushWithLimit(RedoHistory, std::move(UndoHistory[HistoryIndex]));
	}

	UndoHistory.erase(UndoHistory.begin() + Index, UndoHistory.end());
	OutEntry = std::move(Target);
	return true;
}

bool FEditorUndoSystem::ClearStorage()
{
	const bool bHadHistory = !UndoHistory.empty() || !RedoHistory.empty();
	UndoHistory.clear();
	RedoHistory.clear();
	return bHadHistory;
}

FUndoHistoryStats FEditorUndoSystem::GetStats() const
{
	FUndoHistoryStats Stats;
	Stats.UndoCount = static_cast<int32>(UndoHistory.size());
	Stats.RedoCount = static_cast<int32>(RedoHistory.size());
	Stats.MaxEntries = MaxUndoHistory;

	for (const FUndoSnapshotEntry& Entry : UndoHistory)
	{
		Stats.LogicalBytes += Entry.Label.size();
		Stats.LogicalBytes += Entry.Snapshot.size();
		Stats.ReservedBytes += Entry.Label.capacity();
		Stats.ReservedBytes += Entry.Snapshot.capacity();
	}

	for (const FUndoSnapshotEntry& Entry : RedoHistory)
	{
		Stats.LogicalBytes += Entry.Label.size();
		Stats.LogicalBytes += Entry.Snapshot.size();
		Stats.ReservedBytes += Entry.Label.capacity();
		Stats.ReservedBytes += Entry.Snapshot.capacity();
	}

	Stats.EntryOverheadBytes = (UndoHistory.size() + RedoHistory.size()) * sizeof(FUndoSnapshotEntry);
	Stats.ApproxTotalBytes = Stats.ReservedBytes + Stats.EntryOverheadBytes;
	return Stats;
}

void FEditorUndoSystem::PushWithLimit(TArray<FUndoSnapshotEntry>& History, FUndoSnapshotEntry Entry)
{
	History.push_back(std::move(Entry));
	if (static_cast<int32>(History.size()) > MaxUndoHistory)
	{
		History.erase(History.begin());
	}
}
