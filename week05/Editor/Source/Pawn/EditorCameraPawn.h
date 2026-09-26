
#pragma once
#include "Actor/Actor.h"

class UCameraComponent;

class AEditorCameraPawn : public AActor
{
public:
	DECLARE_RTTI(AEditorCameraPawn, AActor)
	void PostConstruct() override;

	UCameraComponent* GetCameraComponent() const { return CameraCompenent; }

private:
	UCameraComponent* CameraCompenent = nullptr;
};
