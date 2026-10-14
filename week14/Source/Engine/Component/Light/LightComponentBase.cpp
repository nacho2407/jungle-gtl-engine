#include "LightComponentBase.h"
#include "Serialization/Archive.h"
#include "Object/Reflection/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Materials/MaterialManager.h"

HIDE_FROM_COMPONENT_LIST(ULightComponentBase)

UBillboardComponent* ULightComponentBase::EnsureEditorBillboard()
{
	if (!Owner)
	{
		return nullptr;
	}

	const char* IconMaterialPath = nullptr;
	switch (GetLightType())
	{
	case ELightComponentType::Ambient:
		IconMaterialPath = "Content/Material/Editor/AmbientLight.mat";
		break;
	case ELightComponentType::Directional:
		IconMaterialPath = "Content/Material/Editor/DirectionalLight.mat";
		break;
	case ELightComponentType::Point:
		IconMaterialPath = "Content/Material/Editor/PointLight.mat";
		break;
	case ELightComponentType::Spot:
		IconMaterialPath = "Content/Material/Editor/SpotLight.mat";
		break;
	}

	if (!IconMaterialPath)
	{
		return nullptr;
	}

	for (USceneComponent* Child : GetChildren())
	{
		UBillboardComponent* Billboard = Cast<UBillboardComponent>(Child);
		if (Billboard && Billboard->IsEditorOnlyComponent())
		{
			// 에디터 아이콘 빌보드는 부모 스케일과 컴포넌트 트리 기본 표시에서 분리한다.
			Billboard->SetAbsoluteScale(true);
			Billboard->SetHiddenInComponentTree(true);
			return Billboard;
		}
	}

	UBillboardComponent* Billboard = Owner->AddComponent<UBillboardComponent>();
	if (Billboard)
	{
		Billboard->AttachToComponent(this);
		// 에디터 아이콘 빌보드는 부모 스케일과 컴포넌트 트리 기본 표시에서 분리한다.
		Billboard->SetAbsoluteScale(true);
		Billboard->SetEditorOnlyComponent(true);
		Billboard->SetHiddenInComponentTree(true);
		auto Material = FMaterialManager::Get().GetOrCreateMaterial(IconMaterialPath);
		Billboard->SetMaterial(Material);
	}

	return Billboard;
}
