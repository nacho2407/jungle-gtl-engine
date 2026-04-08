#pragma once

#include "Actor.h"

class UCameraComponent;
class UStaticMeshComponent;

class ENGINE_API APlayerCameraActor : public AActor
{
public:
	DECLARE_RTTI(APlayerCameraActor, AActor)

	void PostSpawnInitialize() override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

	UCameraComponent* GetCameraComponent() const { return CameraComponent; }
	void SyncCameraComponentState() const;

private:
	UCameraComponent* CameraComponent = nullptr;
	UStaticMeshComponent* VisualizerComponent = nullptr;
};
