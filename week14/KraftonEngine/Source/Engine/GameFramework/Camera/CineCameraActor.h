#pragma once

#include "GameFramework/Camera/CameraActor.h"

#include "Source/Engine/GameFramework/Camera/CineCameraActor.generated.h"
class UCineCameraComponent;

UCLASS()
class ACineCameraActor : public ACameraActor
{
public:
	GENERATED_BODY()
	ACineCameraActor() = default;

	void InitDefaultComponents();
	void PostDuplicate() override;

	UCineCameraComponent* GetCineCameraComponent() const;
};
