#include "Gizmo.h"
#include "USceneComponent.h"
#include "RenderObject.h"
#include "ResourceManager.h"
#include "UPicker.h"
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

void Gizmo::OnBeforeRender()
{
	if (Controller)
	{
		Controller->Update();
		Controller->UpdateRenderObject();
	}
}

