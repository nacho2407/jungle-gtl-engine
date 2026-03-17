#include "Gizmo.h"
#include "Component/SceneComponent.h"
#include "Renderer/RenderObject.h"
#include "ResourceManager.h"
#include "Picker.h"
#include "Logger.h"
#include "GizmoController.h"
#include "Camera.h"

Gizmo::Gizmo()
{
	Controller = new GizmoController();
}

void Gizmo::UpdateRenderObject()
{
	Controller->UpdateRenderObject();
}

Gizmo::~Gizmo()
{
	delete Controller;
}

void Gizmo::OnBeforeRender(UCamera* Camera)
{
	if (Controller)
	{
		Controller->Update(Camera);
		Controller->UpdateRenderObject();
	}
}

