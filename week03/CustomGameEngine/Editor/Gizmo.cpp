#include "Gizmo.h"
#include "Component/SceneComponent.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Renderer/RenderObject.h"
#include "EngineCore/ResourceManager.h"
#include "Picker.h"
#include "EngineCore/Logger.h"
#include "World.h"

Gizmo::Gizmo()
{
}

Gizmo::~Gizmo()
{
}

void Gizmo::CreateGizmoController()
{
	Controller = Cast<UGizmoComponent>(GetWorld().GetSceneManager()->AddPermanentSceneComponent<UGizmoComponent>());
}
