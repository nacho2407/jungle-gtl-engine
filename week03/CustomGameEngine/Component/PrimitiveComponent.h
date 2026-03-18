#pragma once

#include "SceneComponent.h"
#include "Renderer/RenderObject.h"

class URenderer;
struct FGeometry;

enum EPrimitiveType
{
	Sphere,
	Cube,
	Triangle,
	Plane,
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

	void SetRelativeLocation(const FVector& newLocation) override;
	void SetRelativeRotation(const FRotator& newRotation) override;
	void SetRelativeScale3D(const FVector& newScale) override;

	virtual void OnComponentAdded() override;
	virtual void Update(float DeltaTime);
	virtual RenderObject* CreateRenderObject();
	virtual RenderObject* GetRenderObject();
	void SetMaterial(int Handle) { MaterialHandle = Handle; }
	void MarkRenderStateDirty();
	bool GetRenderStateDirty();
	void ResolveRenderStateDirty();

	virtual json::JSON Serialize() override;
	virtual void Deserialize(json::JSON) override;

	FGeometry* GetGeometry() { return VResource; }

	bool Pickable = true;
private:
	virtual void UpdateRenderObject();

protected:
	FGeometry* VResource;
	EPrimitiveType type;
	RenderObject* RenderObj = nullptr;
	
private:
	int MaterialHandle = -1;
};
