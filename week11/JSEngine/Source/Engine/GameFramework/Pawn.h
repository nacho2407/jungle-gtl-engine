#pragma once

#include "Pawn.generated.h"

#include "GameFramework/AActor.h"

class APlayerController;
struct FInputActionState;

UCLASS()
class APawn : public AActor
{
    GENERATED_BODY_APawn()
public:
	DECLARE_CLASS(APawn, AActor)

	APlayerController* GetController() const { return Controller; }
	void PossessedBy(APlayerController* NewController);
	void UnPossessed();

	virtual void OnInputAction(const FInputActionState& Action);

private:
	APlayerController* Controller = nullptr;
};
