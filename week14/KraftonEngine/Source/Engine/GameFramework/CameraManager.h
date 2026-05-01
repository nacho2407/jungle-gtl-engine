#pragma once

#include "Object/Object.h"

class UCameraComponent;

class UCameraManager : public UObject
{
public:
	DECLARE_CLASS(UCameraManager, UObject)
	UCameraManager() = default;
	~UCameraManager() override = default;

	void RegisterCamera(UCameraComponent* Camera);
	void UnregisterCamera(UCameraComponent* Camera);

	void AutoPossessDefaultCamera();

	UCameraComponent* GetActiveCamera() const { return ActiveCamera; }
	void SetActiveCamera(UCameraComponent* NewCamera) { ActiveCamera = NewCamera; }

	UCameraComponent* GetPossessedCamera() const { return PossessedCamera; }
	void Possess(UCameraComponent* NewCamera) { PossessedCamera = NewCamera; }

private:
	TSet<UCameraComponent*> RegisteredCameras;

	UCameraComponent* ActiveCamera = nullptr;		// Rendering Camera
	UCameraComponent* PossessedCamera = nullptr;	// Input/Control Camera
};
