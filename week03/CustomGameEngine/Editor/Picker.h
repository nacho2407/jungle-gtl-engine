#pragma once

#include "EngineTypes.h"
#include "Object.h"
#include "Component/CameraComponent.h"
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

	static Ray CreateRay(int x, int y, UCameraComponent* camera);

	bool IntersectsTriangle(const FVector& V0, const FVector& V1, const FVector& V2, float& OutT, const Ray& ray);

	bool Intersects(UPrimitiveComponent* Comp, float& OutDistance);
};

class UPicker : public UObject
{
	DECLARE_RTTI(UPicker, UObject)
public:
	UPicker();
	virtual ~UPicker() override;
	void Pick(int x, int y, UScene* Scene);
	GizmoControllerAxis PickGizmo(int x, int y, UScene* Scene);
	Ray CreateRay(int x, int y);

	void SetEditor(Editor* pEditor) { EditorPtr = pEditor; }
	void SetCamera(UCameraComponent* pCamera) { CameraPtr = pCamera; }

private:
	UCameraComponent* Camera;
	UScene* Scene;
	Editor* EditorPtr;
	UCameraComponent* CameraPtr;
};