// Copyright Epic Games, Inc. All Rights Reserved.
#include "ShapeComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Render/Proxy/ShapeSceneProxy.h"

#include <cstring>

IMPLEMENT_CLASS_WITH_PROPERTIES(UShapeComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UShapeComponent)

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

	if (strcmp(PropertyName, "ShapeColor") == 0 || strcmp(PropertyName, "bDrawOnlyIfSelected") == 0
		|| strcmp(PropertyName, "Shape Color") == 0 || strcmp(PropertyName, "Draw Only If Selected") == 0)
	{
		MarkRenderStateDirty();
	}
}

void UShapeComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
}
