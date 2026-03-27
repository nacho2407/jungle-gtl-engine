#pragma once

#include "Object/Object.h"

struct FStaticMesh;

// UStaticMesh — FStaticMesh를 참조하는 UObject 래퍼
class UStaticMesh : public UObject
{
public:
	DECLARE_CLASS(UStaticMesh, UObject)

	UStaticMesh() = default;
	~UStaticMesh() override = default;

	const std::string& GetAssetPathFileName() const;
	void SetStaticMeshAsset(FStaticMesh* InMesh);
	FStaticMesh* GetStaticMeshAsset() const;

private:
	FStaticMesh* StaticMeshAsset = nullptr;
};
