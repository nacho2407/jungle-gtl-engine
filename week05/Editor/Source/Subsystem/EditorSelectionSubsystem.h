#pragma once

#include "Types/ObjectPtr.h"

class AActor;

class FEditorSelectionSubsystem
{
public:
	void Shutdown();
	void SetSelectedActor(AActor* InActor);
	AActor* GetSelectedActor() const;

private:
	TObjectPtr<AActor> SelectedActor;
};
