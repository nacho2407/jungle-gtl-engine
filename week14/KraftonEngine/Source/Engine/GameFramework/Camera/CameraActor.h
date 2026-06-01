#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Camera/CameraActor.generated.h"
class UBillboardComponent;
class UCameraComponent;

UCLASS()
class ACameraActor : public AActor
{
public:
	GENERATED_BODY()
	ACameraActor() = default;

	void InitDefaultComponents();
	void PostDuplicate() override;

	UCameraComponent* GetCameraComponent() const;

protected:
	UBillboardComponent* EnsureCameraBillboard();

	UCameraComponent* CameraComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
