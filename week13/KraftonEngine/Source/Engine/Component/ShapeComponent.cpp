// Copyright Epic Games, Inc. All Rights Reserved.
#include "ShapeComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Proxy/ShapeSceneProxy.h"

#include <cstring>

IMPLEMENT_CLASS_WITH_PROPERTIES(UShapeComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UShapeComponent)

BEGIN_PROPERTY_REGISTRATION(UShapeComponent)
	EDIT_PROPERTY(UShapeComponent, ShapeColor, "Shape Color", EPropertyType::Color4, "Shape")
	EDIT_PROPERTY(UShapeComponent, bDrawOnlyIfSelected, "Draw Only If Selected", EPropertyType::Bool, "Shape")
END_PROPERTY_REGISTRATION()

UShapeComponent::UShapeComponent()
{
	bCastShadow = false;
}

FPrimitiveSceneProxy* UShapeComponent::CreateSceneProxy()
{
	return new FShapeSceneProxy(this);
}

void UShapeComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
}

void UShapeComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Shape Color") == 0 || strcmp(PropertyName, "Draw Only If Selected") == 0)
	{
		MarkRenderStateDirty();
	}
}

void UShapeComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << ShapeColor;
	Ar << bDrawOnlyIfSelected;
}
