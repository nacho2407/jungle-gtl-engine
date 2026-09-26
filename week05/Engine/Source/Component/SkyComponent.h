#pragma once
#include "StaticMeshComponent.h"

class ENGINE_API USkyComponent : public UStaticMeshComponent
{
public:
	DECLARE_RTTI(USkyComponent, UStaticMeshComponent)

	void PostConstruct() override;
	virtual void Tick(float DeltaTime) override;
	FBoxSphereBounds GetWorldBounds() const override;
	virtual bool IsPickable() const override { return false; }
};
