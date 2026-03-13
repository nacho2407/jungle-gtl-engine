#pragma once
#include "USceneComponent.h"

class URenderer;

class UPrimitiveComponent : public USceneComponent
{
	DECLARE_RTTI(UPrimitiveComponent, USceneComponent)
public:
	UPrimitiveComponent();
	virtual ~UPrimitiveComponent() override;
	virtual void Update(float DeltaTime);
	virtual void Render(URenderer* Renderer);
	void SetMaterial(int Handle) { MaterialHandle = Handle; }

private:
	int MaterialHandle = -1;
};
