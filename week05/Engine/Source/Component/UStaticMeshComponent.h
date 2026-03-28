#pragma once
#include "CoreMinimal.h"
#include "UMeshComponent.h"
#include "Serializer/Archive.h"

class UStaticMesh;

class ENGINE_API UStaticMeshComponent : public UMeshComponent
{
public:
	DECLARE_RTTI(UStaticMeshComponent, UMeshComponent)

	void SetStaticMesh(UStaticMesh* InStaticMesh);
	UStaticMesh* GetStaticMesh() const { return StaticMesh; }

	// 현재는 일단 .obj파싱 용도로 사용 - 추후 직렬화?
	virtual void Serialize(FArchive& Ar) override;
	virtual FBoxSphereBounds CalcBounds(const FMatrix& LocalToWorld) const override;
	virtual FBoxSphereBounds GetLocalBounds() const override;

private:
	UStaticMesh* StaticMesh = nullptr;
};