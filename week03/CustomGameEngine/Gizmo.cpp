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

Gizmo::~Gizmo()
{
	delete Controller;
}

void Gizmo::Render(URenderer* renderer)
{
	if (Controller)
	{
		Controller->Update();
		Controller->Draw(renderer);
	}
}

void Gizmo::SetControllerTarget(USceneComponent* target)
{
	AttachedComponent = target;
	if (Controller)
	{
		Controller->AttachTo(target);
	}
}
