#pragma once

#include "Viewport.h"
#include "ViewportTypes.h"

class FEditorViewportRegistry
{
public:
	FEditorViewportRegistry();

	TArray<FViewport>& GetViewports() { return Viewports; }
	const TArray<FViewport>& GetViewports() const { return Viewports; }

	TArray<FViewportEntry>& GetEntries() { return Entries; }
	const TArray<FViewportEntry>& GetEntries() const { return Entries; }

	FViewport* GetViewportById(FViewportId Id);
	const FViewport* GetViewportById(FViewportId Id) const;

	FViewportEntry* FindEntryByType(EViewportType Type);
	const FViewportEntry* FindEntryByType(EViewportType Type) const;

	FViewportEntry* FindEntryByViewportID(FViewportId ViewportId);
	const FViewportEntry* FindEntryByViewportID(FViewportId ViewportId) const;

	void ResetToDefault();

private:
	void AddEntry(FViewportId Id, EViewportType Type, int32 SlotIndex);

private:
	TArray<FViewport> Viewports;
	TArray<FViewportEntry> Entries;
};
