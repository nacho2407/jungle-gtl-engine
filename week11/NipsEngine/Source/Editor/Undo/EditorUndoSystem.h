#pragma once

#include "Core/CoreMinimal.h"
#include "Object/FName.h"

#include <cstddef>

class UEditorEngine;

struct FUndoSnapshotEntry
{
	FName WorldHandle;
	FString Label;
	FString Snapshot;
};

struct FWorldUndoHistory
{
	TArray<FUndoSnapshotEntry> UndoHistory;
	TArray<FUndoSnapshotEntry> RedoHistory;
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
	void ClearHistory(const FName& WorldHandle);
	void ClearAllHistory();

	bool IsRestoring() const { return bRestoring; }
	void BeginRestore() { bRestoring = true; }
	void EndRestore() { bRestoring = false; }

	const TArray<FUndoSnapshotEntry>& GetUndoHistory() const;
	const TArray<FUndoSnapshotEntry>& GetRedoHistory() const;
	FUndoHistoryStats GetStats() const;

private:
	friend class UEditorEngine;

	void SetOwner(UEditorEngine* InOwner) { Owner = InOwner; }
	FName GetActiveWorldHandle() const;
	FWorldUndoHistory* FindHistory(const FName& WorldHandle);
	const FWorldUndoHistory* FindHistory(const FName& WorldHandle) const;
	FWorldUndoHistory& GetOrCreateHistory(const FName& WorldHandle);
	bool PushSnapshot(const FName& WorldHandle, FString Snapshot, const char* Reason, bool& bOutClearedRedo);
	bool PopUndoSnapshot(const FName& WorldHandle, FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry);
	bool PopRedoSnapshot(const FName& WorldHandle, FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry);
	bool RestoreHistorySnapshotIndex(const FName& WorldHandle, int32 Index, FString CurrentSnapshot, FUndoSnapshotEntry& OutEntry);
	bool ClearStorage();
	bool ClearStorage(const FName& WorldHandle);
	void PushWithLimit(TArray<FUndoSnapshotEntry>& History, FUndoSnapshotEntry Entry);

private:
	UEditorEngine* Owner = nullptr;
	TMap<FName, FWorldUndoHistory, FName::Hash> HistoriesByWorld;
	mutable TArray<FUndoSnapshotEntry> EmptyHistory;
	bool bRestoring = false;

	static constexpr int32 MaxUndoHistory = 50;
};
