#pragma once

#include "GameFramework/AActor.h"

class APlayerController;
struct FInputActionState;

class APawn : public AActor
{
public:
	DECLARE_CLASS(APawn, AActor)

	APlayerController* GetController() const { return Controller; }
	void PossessedBy(APlayerController* NewController);
	void UnPossessed();

	virtual void OnInputAction(const FInputActionState& Action);

private:
	APlayerController* Controller = nullptr;
};
