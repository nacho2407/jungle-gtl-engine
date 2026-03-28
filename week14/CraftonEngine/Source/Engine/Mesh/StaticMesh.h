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

private:
	std::unique_ptr<FStaticMesh> StaticMeshAsset;
};
