#pragma once

#include "UObject.h"
#include "FVector.h"
#include "Gizmo.h"

class UCamera;
class UScene;
class UPrimitiveComponent;
class Editor;

struct Ray
{
	FVector Origin;
	FVector Direction;

	bool IntersectsTriangle(const FVector& V0, const FVector& V1, const FVector& V2, float& OutT, const Ray& ray);

	bool Intersects(UPrimitiveComponent* Comp, float& OutDistance);
};

class UPicker : public UObject
{
	DECLARE_RTTI(UPicker, UObject)
public:
	UPicker();
	virtual ~UPicker() override;
	void Pick(int x, int y, UCamera* Camera, UScene* Scene);
	GizmoAxis PickGizmo(int x, int y, UCamera* Camera, UScene* Scene);
	Ray CreateRay(int x, int y, UCamera* Camera);

	void SetEditor(Editor* pEditor) { EditorPtr = pEditor; }

private:
	UCamera* Camera;
	UScene* Scene;
	Editor* EditorPtr;
};