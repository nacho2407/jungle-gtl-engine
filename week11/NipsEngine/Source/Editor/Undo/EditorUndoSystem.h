#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/CoreTypes.h"

#include <cstddef>

struct FUndoSnapshotEntry
{
	FString Label;
	FString Snapshot;
};

struct FUndoHistoryStats
{
	int32 UndoCount = 0;
	int32 RedoCount = 0;
	int32 MaxEntries = 0;
	size_t LogicalBytes = 0;
	size_t ReservedBytes = 0;
	size_t EntryOverheadBytes = 0;
	size_t ApproxTotalBytes = 0;
};

class FEditorUndoSystem
{
public:
	bool IsRestoring() const { return bRestoring; }
	void BeginRestore() { bRestoring = true; }
	void EndRestore() { bRestoring = false; }

	bool CaptureSnapshot(FString Snapshot, const char* Reason, bool& bOutClearedRedo);
	bool PopUndo(FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry);
	bool PopRedo(FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry);
	bool RestoreHistoryIndex(int32 Index, FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry);
	bool Clear();

	const TArray<FUndoSnapshotEntry>& GetUndoHistory() const { return UndoHistory; }
	const TArray<FUndoSnapshotEntry>& GetRedoHistory() const { return RedoHistory; }
	FUndoHistoryStats GetStats() const;

private:
	void PushWithLimit(TArray<FUndoSnapshotEntry>& History, FUndoSnapshotEntry Entry);

private:
	TArray<FUndoSnapshotEntry> UndoHistory;
	TArray<FUndoSnapshotEntry> RedoHistory;
	bool bRestoring = false;

	static constexpr int32 MaxUndoHistory = 50;
};
