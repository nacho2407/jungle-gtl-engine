#pragma once

#include "UObject.h"
#include "CoreTypes.h"
#include "Component/USceneComponent.h"
#include "EGizmoAxis.h"

class UGizmoComponent : public USceneComponent
{
	DECLARE_UClass(UGizmoComponent, USceneComponent)

public:
	UGizmoComponent();

	void Update(float DeltaTime, const FVector& CameraLocation);
	
};
