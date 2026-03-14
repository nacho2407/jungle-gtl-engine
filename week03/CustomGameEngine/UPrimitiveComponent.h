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

	void SetRelativeLocation(FVector newLocation) override;
	void SetRelativeRotation(FVector newRotation) override;
	void SetRelativeScale3D(FVector newScale) override;

	virtual void Update(float DeltaTime);
	RenderObject* CreateRenderObject();
	virtual RenderObject* GetRenderObject();
	void SetMaterial(int Handle) { MaterialHandle = Handle; }
	void MarkRenderStateDirty();
	bool GetRenderStateDirty();
	void ResolveRenderStateDirty();

	virtual json::JSON Serialize() override;
	virtual void Deserialize(json::JSON) override;

	FGeometry* GetGeometry() { return VResource; }
private:
	void UpdateRenderObject();

protected:
	FGeometry* VResource;
	EPrimitiveType type;
	RenderObject* RenderObj = nullptr;
private:
	int MaterialHandle = -1;
};
