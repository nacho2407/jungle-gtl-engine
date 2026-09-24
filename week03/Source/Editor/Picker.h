#pragma once

#include "Math/Ray.h"
#include "EngineCore/EngineTypes.h"
#include "Math/Vector.h"

class UCameraComponent;
class UScene;
class UPrimitiveComponent;
class Editor;

enum class GizmoControllerAxis : int32;

class UPicker
{
public:
	UPicker();
	~UPicker();
	void Pick(int x, int y, UScene* Scene, bool bMultiSelect);

	void SetEditor(Editor* pEditor) { EditorPtr = pEditor; }
	void SetCamera(UCameraComponent* pCamera) { CameraPtr = pCamera; }

private:
	//UCameraComponent* Camera;
	UScene* Scene;
	Editor* EditorPtr;
	UCameraComponent* CameraPtr;
};
