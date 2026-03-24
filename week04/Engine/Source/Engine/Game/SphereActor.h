#pragma once
#include "Actor.h"

namespace Engine::Component
{
	class USphereComponent;
	class UPrimitiveComponent;
}

class ENGINE_API ASphereActor : public AActor
{
	DECLARE_RTTI(ASphereActor, AActor)

public:
	ASphereActor();
	~ASphereActor() override = default;

	Engine::Component::USphereComponent* GetSphereComponent() const;

	bool IsRenderable() const override;
	bool IsSelected() const override;
	FColor GetColor() const override;
	EBasicMeshType GetMeshType() const override;
	uint32 GetObjectId() const override;

private:
	Engine::Component::UPrimitiveComponent* GetPrimitiveComponent() const;
};

