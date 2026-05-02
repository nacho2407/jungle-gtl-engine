#pragma once

#include "PrimitiveComponent.h"

class FMeshBuffer;

class UQuestArrowComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UQuestArrowComponent, UPrimitiveComponent)

	UQuestArrowComponent();

	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetWorldDirection(const FVector& Direction);
};
