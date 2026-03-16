#include "Gizmo.h"
#include "USceneComponent.h"
#include "RenderObject.h"
#include "ResourceManager.h"
#include "UPicker.h"
#include "Logger.h"
#include "GizmoController.h"
#include "UCamera.h"

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

