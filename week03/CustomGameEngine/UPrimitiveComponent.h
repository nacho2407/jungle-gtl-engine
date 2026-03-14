#pragma once
#include "USceneComponent.h"
#include "RenderObject.h"
class URenderer;
class FGeometry;

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
	virtual RenderObject* GetRenderObject();
	void SetMaterial(int Handle) { MaterialHandle = Handle; }

	virtual json::JSON Serialize() override;
	virtual void Deserialize(json::JSON) override;

	FGeometry* GetGeometry() { return VResource; }
protected:
	FGeometry* VResource;
	EPrimitiveType type;
private:
	int MaterialHandle = -1;
};
