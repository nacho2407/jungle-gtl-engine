#pragma once

#include "Core/CoreTypes.h"
#include "Object/Object.h"

class UCameraComponent;
class AActor;

class UCameraManager : public UObject
{
public:
	DECLARE_CLASS(UCameraManager, UObject)
	UCameraManager() = default;
	~UCameraManager() override = default;

	void RegisterCamera(UCameraComponent* Camera);
	void UnregisterCamera(UCameraComponent* Camera);

	void AutoPossessDefaultCamera();
	bool ToggleActiveCameraForActor(const FString& ActorName);
	bool ToggleActiveCameraForActor(const AActor* Actor);

	UCameraComponent* GetActiveCamera() const { return ActiveCamera; }
	void SetActiveCamera(UCameraComponent* NewCamera) { ActiveCamera = NewCamera; }

	UCameraComponent* GetPossessedCamera() const { return PossessedCamera; }
	void Possess(UCameraComponent* NewCamera) { PossessedCamera = NewCamera; }

private:
	TSet<UCameraComponent*> RegisteredCameras;
	TArray<UCameraComponent*> RegisteredCameraOrder;

	UCameraComponent* ActiveCamera = nullptr;		// Rendering Camera
	UCameraComponent* PossessedCamera = nullptr;	// Input/Control Camera
};
