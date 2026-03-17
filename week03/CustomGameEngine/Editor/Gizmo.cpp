#include "Gizmo.h"
#include "Component/SceneComponent.h"
#include "Component/CameraComponent.h"
#include "Renderer/RenderObject.h"
#include "ResourceManager.h"
#include "Picker.h"
#include "Logger.h"
#include "GizmoController.h"

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

void Gizmo::OnBeforeRender(UCameraComponent* Camera)
{
	if (Controller)
	{
		Controller->Update(Camera);
		Controller->UpdateRenderObject();
	}
}

