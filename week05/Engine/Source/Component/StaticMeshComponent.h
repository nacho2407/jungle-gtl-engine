#pragma once
#include "CoreMinimal.h"
#include "PrimitiveComponent.h"
#include "MeshComponent.h"
#include "Serializer/Archive.h"

class UStaticMesh;
class FPrimitiveSceneProxy;

class ENGINE_API UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_RTTI(UStaticMeshComponent, UMeshComponent)

	void SetStaticMesh(UStaticMesh* InStaticMesh);
	FRenderMesh* GetRenderMesh() const override;
	UStaticMesh* GetStaticMesh() const { return StaticMesh; }
	std::shared_ptr<FPrimitiveSceneProxy> CreateSceneProxy() const override;


	// 현재는 일단 .obj파싱 용도로 사용 - 추후 직렬화?
	// virtual void Serialize(FArchive& Ar) override;
	void Serialize(FArchive& Ar) override;
	FBoxSphereBounds CalcBounds(const FMatrix& LocalToWorld) const override;
	FBoxSphereBounds GetLocalBounds() const override;

private:
	UStaticMesh* StaticMesh = nullptr;
};
