#pragma once
#include "USceneComponent.h"
#include "RenderObject.h"
class URenderer;

enum EPrimitiveType
{
	Sphere,
	Cube,
	Triangle
};

class UPrimitiveComponent : public USceneComponent
{
	DECLARE_RTTI(UPrimitiveComponent, USceneComponent)
public:
	UPrimitiveComponent();
	virtual ~UPrimitiveComponent() override;

	EPrimitiveType GetType();
	void SetType(EPrimitiveType);

	virtual void Update(float DeltaTime);
	virtual URenderObject* Render();
	void SetMaterial(int Handle) { MaterialHandle = Handle; }

	virtual json::JSON Serialize() override;
	virtual void Deserialize(json::JSON) override;

private:
	int MaterialHandle = -1;
	EPrimitiveType type;
};
