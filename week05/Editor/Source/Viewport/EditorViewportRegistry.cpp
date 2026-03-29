#include "EditorViewportRegistry.h"

FEditorViewportRegistry::FEditorViewportRegistry()
{
	ResetToDefault();
}

void FEditorViewportRegistry::ResetToDefault()
{
	Viewports.resize(MAX_VIEWPORTS);

	Entries.clear();
	Entries.reserve(MAX_VIEWPORTS);

	AddEntry(0, EViewportType::Perspective, 0);
	AddEntry(1, EViewportType::OrthoTop, 1);
	AddEntry(2, EViewportType::OrthoRight, 2);
	AddEntry(3, EViewportType::OrthoFront, 3);
}

FViewport* FEditorViewportRegistry::GetViewportById(FViewportId Id)
{
	if (Id == INVALID_VIEWPORT_ID)
	{
		return nullptr;
	}

	for (FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == Id && Entry.bActive && Entry.Viewport)
		{
			return Entry.Viewport;
		}
	}

	return nullptr;
}

const FViewport* FEditorViewportRegistry::GetViewportById(FViewportId Id) const
{
	if (Id == INVALID_VIEWPORT_ID)
	{
		return nullptr;
	}

	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == Id && Entry.bActive && Entry.Viewport)
		{
			return Entry.Viewport;
		}
	}

	return nullptr;
}

FViewportEntry* FEditorViewportRegistry::FindEntryByType(EViewportType Type)
{
	for (FViewportEntry& Entry : Entries)
	{
		if (Entry.LocalState.ProjectionType == Type)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FViewportEntry* FEditorViewportRegistry::FindEntryByType(EViewportType Type) const
{
	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.LocalState.ProjectionType == Type)
		{
			return &Entry;
		}
	}

	return nullptr;
}

FViewportEntry* FEditorViewportRegistry::FindEntryByViewportID(FViewportId ViewportId)
{
	for (FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == ViewportId)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FViewportEntry* FEditorViewportRegistry::FindEntryByViewportID(FViewportId ViewportId) const
{
	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == ViewportId)
		{
			return &Entry;
		}
	}

	return nullptr;
}

void FEditorViewportRegistry::AddEntry(FViewportId Id, EViewportType Type, int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(Viewports.size()))
	{
		return;
	}

	FViewportEntry Entry;
	Entry.Id = Id;
	Entry.Viewport = &Viewports[SlotIndex];
	Entry.bActive = true;
	Entry.LocalState = FViewportLocalState::CreateDefault(Type);
	Entries.push_back(Entry);
}
