#include "LightComponentBase.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Component/BillboardComponent.h"
#include "Materials/MaterialManager.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(ULightComponentBase, USceneComponent)
HIDE_FROM_COMPONENT_LIST(ULightComponentBase)

BEGIN_PROPERTY_REGISTRATION(ULightComponentBase)
	EDIT_PROPERTY_RANGE(ULightComponentBase, Intensity, "Intensity", EPropertyType::Float, "Lighting", 0.0f, 50.0f, 0.05f)
	EDIT_PROPERTY(ULightComponentBase, LightColor, "Color", EPropertyType::Color4, "Lighting")
	EDIT_PROPERTY(ULightComponentBase, bVisible, "Visible", EPropertyType::Bool, "Lighting")
	EDIT_PROPERTY(ULightComponentBase, bCastShadows, "Cast Shadows", EPropertyType::Bool, "Lighting")
END_PROPERTY_REGISTRATION()

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << Intensity;
	Ar << LightColor;
	Ar << bVisible;
	Ar << bCastShadows;
}

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
		IconMaterialPath = "Asset/Materials/Editor/AmbientLight.mat";
		break;
	case ELightComponentType::Directional:
		IconMaterialPath = "Asset/Materials/Editor/DirectionalLight.mat";
		break;
	case ELightComponentType::Point:
		IconMaterialPath = "Asset/Materials/Editor/PointLight.mat";
		break;
	case ELightComponentType::Spot:
		IconMaterialPath = "Asset/Materials/Editor/SpotLight.mat";
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
