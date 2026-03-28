#pragma once

#include "Object/Object.h"
#include "Mesh/StaticMeshAsset.h"

#include <memory>

// UStaticMesh — FStaticMesh를 소유하는 UObject 에셋
class UStaticMesh : public UObject
{
public:
	DECLARE_CLASS(UStaticMesh, UObject)

	UStaticMesh() = default;
	~UStaticMesh() override = default;

	const FString& GetAssetPathFileName() const;
	void SetStaticMeshAsset(std::unique_ptr<FStaticMesh> InMesh);
	FStaticMesh* GetStaticMeshAsset() const;
	const TArray<FStaticMaterial>& GetStaticMaterials() const;

private:
	std::unique_ptr<FStaticMesh> StaticMeshAsset;
	TArray<FStaticMaterial> StaticMaterials; // 슬롯 이름과 머티리얼 인터페이스를 묶어서 저장하는 배열
};
