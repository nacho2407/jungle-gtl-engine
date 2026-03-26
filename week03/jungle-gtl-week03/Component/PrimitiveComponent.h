#pragma once

#include "SceneComponent.h"
#include "Renderer/RenderObject.h"
#include "AABB.h"

#include "ThirdParty/json.h"

class URenderer;
struct FGeometry;

enum EPrimitiveType
{
	Sphere,
	Cube,
	Triangle,
	Plane,
	SubUV,
	TextMesh,
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
	void SetRelativeQuaternion(FQuat newQuat) override;

	virtual void OnComponentAdded() override;
	virtual void Update(float DeltaTime) override;
	virtual RenderObject* CreateRenderObject();
	virtual RenderObject* GetRenderObject();
	virtual void DrawDebugLines() override;
	void SetMaterial(int Handle) { MaterialHandle = Handle; }
	void MarkRenderStateDirty();
	bool GetRenderStateDirty();
	void ResolveRenderStateDirty();
	void UpdateAABB();
	bool RayIntersects(struct Ray& ray, float& OutDistance);

	virtual json::JSON Serialize() override;
	virtual void Deserialize(json::JSON) override;

	FGeometry* GetGeometry() { return VResource; }

	bool Pickable = true;
	bool bVisible = true;
	bool bDebugLine = true;
	void SetVisible(bool bNewVisible) { bVisible = bNewVisible; MarkRenderStateDirty(); }
private:
	virtual void UpdateRenderObject();

protected:
	FGeometry* VResource;
	EPrimitiveType type;
	RenderObject* RenderObj = nullptr;
	FAABB BoundingBox;

private:
	int MaterialHandle = -1;
};
