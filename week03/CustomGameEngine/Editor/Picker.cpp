#include "Picker.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"
#include "Editor.h"
#include "Gizmo.h"
#include "Renderer/Geometry.h"
#include "Renderer/Renderer.h"
#include "Scene.h"
#include "Math/Vector4.h"
#include "EngineCore/Logger.h"
#include <algorithm>

UPicker::UPicker()
{
}

UPicker::~UPicker()
{
}

void UPicker::Pick(int ScreenX, int ScreenY, UScene* Scene, bool bMultiSelect)
{
	Ray ray = CameraPtr->ScreenPointToRay(ScreenX, ScreenY);

	UGizmoComponent* controller = Gizmo::GetInstance().GetController();
	if (controller)
	{
		GizmoControllerAxis selectedAxis = controller->TryPick(ray);
		if (selectedAxis != GizmoControllerAxis::None)
		{
			controller->SetAxis(selectedAxis);
			controller->BeginDrag(ray);
			return;
		}
	}

	USceneComponent* selectedComp = nullptr;
	float ClosestDist = FLT_MAX;

	// 레이캐스팅
	for (USceneComponent* sc : Scene->SceneComponents)
	{
		UPrimitiveComponent* Comp = Cast<UPrimitiveComponent>(sc);
		if (Comp == nullptr || !(Comp->Pickable))
			continue;

		RenderObject* RO = Comp->GetRenderObject();
		if (RO && !EditorPtr->GetRenderer()->ShowFlagMap[RO->ShowFlag])
			continue;

		float Distance = 0.f;
		if (Comp->RayIntersects(ray, Distance))
		{
			if (Distance < ClosestDist)
			{
				ClosestDist = Distance;
				selectedComp = Comp;
			}
		}
	}

	if (selectedComp != nullptr)
	{
		if (!bMultiSelect)
		{
			if (EditorPtr->IsComponentSelected(selectedComp) &&
				EditorPtr->GetSelectedComponents().size() == 1)
			{
				EditorPtr->ClearSelection();
			}
			else
			{
				EditorPtr->SelectSingleComponent(selectedComp);
			}
		}
		else
		{
			EditorPtr->SelectMultiComponent(selectedComp);
		}
		Scene->IsCompSelected = true;
	}
	else
	{
		if (!bMultiSelect)
		{
			EditorPtr->ClearSelection();
		}
	}
}

