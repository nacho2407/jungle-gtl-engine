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

	const FName WorldHandle = GetActiveWorldHandle();
	if (WorldHandle == FName::None)
	{
		return false;
	}

	FString Snapshot = Owner->CaptureSceneSnapshot();
	if (Snapshot.empty())
	{
		return false;
	}

	bool bClearedRedo = false;
	const bool bCaptured = PushSnapshot(WorldHandle, std::move(Snapshot), Reason, bClearedRedo);
	if (bClearedRedo)
	{
		Owner->GetNotificationService().Info("Redo history cleared");
	}
	return bCaptured;
}

bool FEditorUndoSystem::Undo()
{
	if (Owner == nullptr || IsRestoring() || Owner->GetEditorState() != EViewportPlayState::Editing)
	{
		return false;
	}

	const FName WorldHandle = GetActiveWorldHandle();
	if (WorldHandle == FName::None)
	{
		return false;
	}

	FString Current = Owner->CaptureSceneSnapshot();
	FUndoSnapshotEntry Previous;
	if (!PopUndoSnapshot(WorldHandle, std::move(Current), Previous))
	{
		return false;
	}

	const bool bRestored = Owner->RestoreSceneSnapshot(Previous.Snapshot, Previous.WorldHandle);
	if (bRestored)
	{
		Owner->GetNotificationService().Info("Undo: " + Previous.Label);
	}
	return bRestored;
}

bool FEditorUndoSystem::Redo()
{
	if (Owner == nullptr || IsRestoring() || Owner->GetEditorState() != EViewportPlayState::Editing)
	{
		return false;
	}

	const FName WorldHandle = GetActiveWorldHandle();
	if (WorldHandle == FName::None)
	{
		return false;
	}

	FString Current = Owner->CaptureSceneSnapshot();
	FUndoSnapshotEntry Next;
	if (!PopRedoSnapshot(WorldHandle, std::move(Current), Next))
	{
		return false;
	}

	const bool bRestored = Owner->RestoreSceneSnapshot(Next.Snapshot, Next.WorldHandle);
	if (bRestored)
	{
		Owner->GetNotificationService().Info("Redo: " + Next.Label);
	}
	return bRestored;
}

bool FEditorUndoSystem::RestoreHistoryIndex(int32 Index)
{
	if (Owner == nullptr || IsRestoring() || Owner->GetEditorState() != EViewportPlayState::Editing)
	{
		return false;
	}

	const FName WorldHandle = GetActiveWorldHandle();
	if (WorldHandle == FName::None)
	{
		return false;
	}

	FString Current = Owner->CaptureSceneSnapshot();
	FUndoSnapshotEntry Target;
	if (!RestoreHistorySnapshotIndex(WorldHandle, Index, std::move(Current), Target))
	{
		return false;
	}

	const bool bRestored = Owner->RestoreSceneSnapshot(Target.Snapshot, Target.WorldHandle);
	if (bRestored)
	{
		Owner->GetNotificationService().Info("History restored: " + Target.Label);
	}
	return bRestored;
}

void FEditorUndoSystem::ClearHistory()
{
	ClearHistory(GetActiveWorldHandle());
}

void FEditorUndoSystem::ClearHistory(const FName& WorldHandle)
{
	if (Owner == nullptr)
	{
		ClearStorage(WorldHandle);
		return;
	}

	if (ClearStorage(WorldHandle))
	{
		Owner->GetNotificationService().Info("Undo history cleared");
	}
}

void FEditorUndoSystem::ClearAllHistory()
{
	if (Owner == nullptr)
	{
		ClearStorage();
		return;
	}

	if (ClearStorage())
	{
		Owner->GetNotificationService().Info("Undo history cleared");
	}
}

FName FEditorUndoSystem::GetActiveWorldHandle() const
{
	return Owner ? Owner->GetActiveWorldHandle() : FName::None;
}

FWorldUndoHistory* FEditorUndoSystem::FindHistory(const FName& WorldHandle)
{
	auto It = HistoriesByWorld.find(WorldHandle);
	return It != HistoriesByWorld.end() ? &It->second : nullptr;
}

const FWorldUndoHistory* FEditorUndoSystem::FindHistory(const FName& WorldHandle) const
{
	auto It = HistoriesByWorld.find(WorldHandle);
	return It != HistoriesByWorld.end() ? &It->second : nullptr;
}

FWorldUndoHistory& FEditorUndoSystem::GetOrCreateHistory(const FName& WorldHandle)
{
	return HistoriesByWorld[WorldHandle];
}

bool FEditorUndoSystem::PushSnapshot(const FName& WorldHandle, FString Snapshot, const char* Reason, bool& bOutClearedRedo)
{
	bOutClearedRedo = false;
	if (WorldHandle == FName::None || Snapshot.empty())
	{
		return false;
	}

	FWorldUndoHistory& History = GetOrCreateHistory(WorldHandle);
	if (!History.UndoHistory.empty() && History.UndoHistory.back().Snapshot == Snapshot)
	{
		return false;
	}

	FUndoSnapshotEntry Entry;
	Entry.WorldHandle = WorldHandle;
	Entry.Label = (Reason && Reason[0] != '\0') ? Reason : "Scene Edit";
	Entry.Snapshot = std::move(Snapshot);
	PushWithLimit(History.UndoHistory, std::move(Entry));

	if (!History.RedoHistory.empty())
	{
		History.RedoHistory.clear();
		bOutClearedRedo = true;
	}
	return true;
}

bool FEditorUndoSystem::PopUndoSnapshot(const FName& WorldHandle, FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry)
{
	FWorldUndoHistory* History = FindHistory(WorldHandle);
	if (!History || History->UndoHistory.empty())
	{
		return false;
	}

	OutEntry = std::move(History->UndoHistory.back());
	History->UndoHistory.pop_back();

	if (!CurrentSnapshot.empty())
	{
		FUndoSnapshotEntry CurrentEntry;
		CurrentEntry.WorldHandle = WorldHandle;
		CurrentEntry.Label = "Current";
		CurrentEntry.Snapshot = std::move(CurrentSnapshot);
		PushWithLimit(History->RedoHistory, std::move(CurrentEntry));
	}
	return true;
}

bool FEditorUndoSystem::PopRedoSnapshot(const FName& WorldHandle, FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry)
{
	FWorldUndoHistory* History = FindHistory(WorldHandle);
	if (!History || History->RedoHistory.empty())
	{
		return false;
	}

	OutEntry = std::move(History->RedoHistory.back());
	History->RedoHistory.pop_back();

	if (!CurrentSnapshot.empty())
	{
		FUndoSnapshotEntry CurrentEntry;
		CurrentEntry.WorldHandle = WorldHandle;
		CurrentEntry.Label = "Current";
		CurrentEntry.Snapshot = std::move(CurrentSnapshot);
		PushWithLimit(History->UndoHistory, std::move(CurrentEntry));
	}
	return true;
}

bool FEditorUndoSystem::RestoreHistorySnapshotIndex(const FName& WorldHandle, int32 Index, FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry)
{
	FWorldUndoHistory* History = FindHistory(WorldHandle);
	if (!History || Index < 0 || Index >= static_cast<int32>(History->UndoHistory.size()))
	{
		return false;
	}

	FUndoSnapshotEntry Target = History->UndoHistory[Index];

	History->RedoHistory.clear();
	if (!CurrentSnapshot.empty())
	{
		FUndoSnapshotEntry CurrentEntry;
		CurrentEntry.WorldHandle = WorldHandle;
		CurrentEntry.Label = "Current";
		CurrentEntry.Snapshot = std::move(CurrentSnapshot);
		PushWithLimit(History->RedoHistory, std::move(CurrentEntry));
	}

	for (int32 HistoryIndex = static_cast<int32>(History->UndoHistory.size()) - 1; HistoryIndex > Index; --HistoryIndex)
	{
		PushWithLimit(History->RedoHistory, std::move(History->UndoHistory[HistoryIndex]));
	}

	History->UndoHistory.erase(History->UndoHistory.begin() + Index, History->UndoHistory.end());
	OutEntry = std::move(Target);
	return true;
}

bool FEditorUndoSystem::ClearStorage()
{
	const bool bHadHistory = !HistoriesByWorld.empty();
	HistoriesByWorld.clear();
	return bHadHistory;
}

bool FEditorUndoSystem::ClearStorage(const FName& WorldHandle)
{
	if (WorldHandle == FName::None)
	{
		return false;
	}

	auto It = HistoriesByWorld.find(WorldHandle);
	if (It == HistoriesByWorld.end())
	{
		return false;
	}

	HistoriesByWorld.erase(It);
	return true;
}

const TArray<FUndoSnapshotEntry>& FEditorUndoSystem::GetUndoHistory() const
{
	const FWorldUndoHistory* History = FindHistory(GetActiveWorldHandle());
	return History ? History->UndoHistory : EmptyHistory;
}

const TArray<FUndoSnapshotEntry>& FEditorUndoSystem::GetRedoHistory() const
{
	const FWorldUndoHistory* History = FindHistory(GetActiveWorldHandle());
	return History ? History->RedoHistory : EmptyHistory;
}

FUndoHistoryStats FEditorUndoSystem::GetStats() const
{
	FUndoHistoryStats Stats;
	Stats.MaxEntries = MaxUndoHistory;

	const FWorldUndoHistory* History = FindHistory(GetActiveWorldHandle());
	if (!History)
	{
		return Stats;
	}

	Stats.UndoCount = static_cast<int32>(History->UndoHistory.size());
	Stats.RedoCount = static_cast<int32>(History->RedoHistory.size());

	for (const FUndoSnapshotEntry& Entry : History->UndoHistory)
	{
		Stats.LogicalBytes += Entry.Label.size();
		Stats.LogicalBytes += Entry.Snapshot.size();
		Stats.ReservedBytes += Entry.Label.capacity();
		Stats.ReservedBytes += Entry.Snapshot.capacity();
	}

	for (const FUndoSnapshotEntry& Entry : History->RedoHistory)
	{
		Stats.LogicalBytes += Entry.Label.size();
		Stats.LogicalBytes += Entry.Snapshot.size();
		Stats.ReservedBytes += Entry.Label.capacity();
		Stats.ReservedBytes += Entry.Snapshot.capacity();
	}

	Stats.EntryOverheadBytes = (History->UndoHistory.size() + History->RedoHistory.size()) * sizeof(FUndoSnapshotEntry);
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
