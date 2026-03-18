#include "Picker.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"
#include "Editor.h"
#include "Gizmo.h"
#include "Renderer/Geometry.h"
#include "Scene.h"
#include "Math/Vector4.h"
#include "Logger.h"
#include <algorithm>

UPicker::UPicker()
{
}

UPicker::~UPicker()
{
}

void UPicker::Pick(int ScreenX, int ScreenY, UScene* Scene)
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

	TArray<TPair<float, USceneComponent*>> RayHits;

	for (USceneComponent* sc : Scene->SceneComponents)
	{
		UPrimitiveComponent* Comp = Cast<UPrimitiveComponent>(sc);
		if (Comp == nullptr || !(Comp->Pickable))
			continue;

		float Distance = 0.f;
		if (ray.Intersects(Comp, Distance))
		{
			RayHits.push_back({ Distance, Comp });
		}
	}

	if (!RayHits.empty())
	{
		// RayHits 안에 이미 선택된 오브젝트가 있다면 거리순으로 정렬해 그 다음 것을 선택
		std::sort(RayHits.begin(), RayHits.end());
		int nextIndex = 0;
		for (int i = 0; i < RayHits.size(); ++i) {
			if (RayHits[i].second == EditorPtr->GetSelectedComponent()) {
				nextIndex = (i + 1) % RayHits.size();
				break;
			}
		}
		EditorPtr->SelectComponent(RayHits[nextIndex].second);
		Scene->IsCompSelected = true;
	}
	else
	{
		EditorPtr->SelectComponent(nullptr);
		Scene->IsCompSelected = false;
	}
}

REGISTER_CLASS(UPicker)