#include "Subsystem/EditorSelectionSubsystem.h"

#include "Actor/Actor.h"

void FEditorSelectionSubsystem::Shutdown()
{
	SelectedActor = nullptr;
}

void FEditorSelectionSubsystem::SetSelectedActor(AActor* InActor)
{
	SelectedActor = InActor;
}

AActor* FEditorSelectionSubsystem::GetSelectedActor() const
{
	if (SelectedActor && SelectedActor->IsPendingDestroy())
	{
		return nullptr;
	}

	return SelectedActor.Get();
}
