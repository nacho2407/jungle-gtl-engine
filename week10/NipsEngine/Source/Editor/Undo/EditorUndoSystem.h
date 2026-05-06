#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/CoreTypes.h"

#include <cstddef>

class UEditorEngine;

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
	bool CaptureSnapshot(const char* Reason = nullptr);
	bool Undo();
	bool Redo();
	bool RestoreHistoryIndex(int32 Index);
	void ClearHistory();

	bool IsRestoring() const { return bRestoring; }
	void BeginRestore() { bRestoring = true; }
	void EndRestore() { bRestoring = false; }

	const TArray<FUndoSnapshotEntry>& GetUndoHistory() const { return UndoHistory; }
	const TArray<FUndoSnapshotEntry>& GetRedoHistory() const { return RedoHistory; }
	FUndoHistoryStats GetStats() const;

private:
	friend class UEditorEngine;

	void SetOwner(UEditorEngine* InOwner) { Owner = InOwner; }
	bool PushSnapshot(FString Snapshot, const char* Reason, bool& bOutClearedRedo);
	bool PopUndoSnapshot(FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry);
	bool PopRedoSnapshot(FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry);
	bool RestoreHistorySnapshotIndex(int32 Index, FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry);
	bool ClearStorage();
	void PushWithLimit(TArray<FUndoSnapshotEntry>& History, FUndoSnapshotEntry Entry);

private:
	UEditorEngine* Owner = nullptr;
	TArray<FUndoSnapshotEntry> UndoHistory;
	TArray<FUndoSnapshotEntry> RedoHistory;
	bool bRestoring = false;

	static constexpr int32 MaxUndoHistory = 50;
};
