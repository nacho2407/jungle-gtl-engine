#pragma once
#include "USceneComponent.h"
#include "RenderObject.h"
class URenderer;
class VertexResource;
enum EPrimitiveType
{
	Sphere,
	Cube,
	Triangle,
	Axis
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

	VertexResource* GetVertexResource() { return VResource; }
protected:
	VertexResource* VResource;
private:
	int MaterialHandle = -1;
	EPrimitiveType type;
};
