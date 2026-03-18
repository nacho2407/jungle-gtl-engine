#pragma once

#include "Ray.h"
#include "EngineTypes.h"
#include "Object.h"
#include "Component/CameraComponent.h"
#include "Math/Vector.h"

class UCameraComponent;
class UScene;
class UPrimitiveComponent;
class Editor;

enum class GizmoControllerAxis : int32;

class UPicker : public UObject
{
	DECLARE_RTTI(UPicker, UObject)
public:
	UPicker();
	virtual ~UPicker() override;
	void Pick(int x, int y, UScene* Scene);

	void SetEditor(Editor* pEditor) { EditorPtr = pEditor; }
	void SetCamera(UCameraComponent* pCamera) { CameraPtr = pCamera; }

private:
	UCameraComponent* Camera;
	UScene* Scene;
	Editor* EditorPtr;
	UCameraComponent* CameraPtr;
};