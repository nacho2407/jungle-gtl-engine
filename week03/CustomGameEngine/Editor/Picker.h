#pragma once

#include "EngineTypes.h"
#include "Object.h"
#include "Math/Vector.h"

class UCameraComponent;
class UScene;
class UPrimitiveComponent;
class Editor;

enum class GizmoControllerAxis : int32;

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
	void Pick(int x, int y, UCameraComponent* Camera, UScene* Scene);
	GizmoControllerAxis PickGizmo(int x, int y, UCameraComponent* Camera, UScene* Scene);
	Ray CreateRay(int x, int y, UCameraComponent* Camera);

	void SetEditor(Editor* pEditor) { EditorPtr = pEditor; }

private:
	UCameraComponent* Camera;
	UScene* Scene;
	Editor* EditorPtr;
};