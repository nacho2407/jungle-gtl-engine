#include "Gizmo.h"
#include "Component/SceneComponent.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Renderer/RenderObject.h"
#include "ResourceManager.h"
#include "Picker.h"
#include "Logger.h"
#include "World.h"

Gizmo::Gizmo()
{
}

Gizmo::~Gizmo()
{
}

void Gizmo::CreateGizmoController()
{
	Controller = Cast<UGizmoComponent>(GetWorld().AddPermanentSceneComponent<UGizmoComponent>());
}
