#pragma once

#include "Component/PrimitiveComponent.h"

#include "Source/Game/Component/CompletionOutlineComponent.generated.h"
class FMeshBuffer;

UCLASS()
class UCompletionOutlineComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()
	UCompletionOutlineComponent() = default;
	~UCompletionOutlineComponent() override = default;

	void CreateRenderState() override;
	void DestroyRenderState() override;
	FMeshBuffer* GetMeshBuffer() const override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	float RemainingTime = 0.5f;
};
