#pragma once

#include "ActorComponent.h"
#include "Renderer/Material.h"
#include <memory>

class FDynamicMaterial;
class UMeshComponent;

class ENGINE_API URandomColorComponent : public UActorComponent
{
public:
	DECLARE_RTTI(URandomColorComponent, UActorComponent)
	void PostConstruct() override;
	~URandomColorComponent() override;

	void SetUpdateInterval(float InInterval) { UpdateInterval = InInterval; }
	float GetUpdateInterval() const { return UpdateInterval; }

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

private:
	TObjectPtr<UMeshComponent> CachedMesh;
	std::shared_ptr<FDynamicMaterial> DynamicMaterial;
	float UpdateInterval = 1.0f;
	float ElapsedTime = 0.0f;

	void ApplyRandomColor();
};
