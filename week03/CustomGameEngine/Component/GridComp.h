#pragma once

#include "Renderer/Geometry.h"
#include "Renderer/Material.h"
#include "Component/PrimitiveComponent.h"
class UGridComp : public UPrimitiveComponent
{
	DECLARE_RTTI(UGridComp, UPrimitiveComponent)
public:
	UGridComp();
	~UGridComp() {}
	void Update(float deltaTime) override;
	RenderObject* CreateRenderObject() override;
	float HalfExtent = 10.f;
	float CellSize = 1.f;
private:

};