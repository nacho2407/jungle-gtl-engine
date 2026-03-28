#pragma once

#include "Object/Object.h"

struct FStaticMesh;
struct FStaticMaterial;

// UStaticMesh — FStaticMesh를 참조하는 UObject 래퍼
class UStaticMesh : public UObject
{
public:
	DECLARE_CLASS(UStaticMesh, UObject)

	UStaticMesh() = default;
	~UStaticMesh() override = default;

	const FString& GetAssetPathFileName() const;
	void SetStaticMeshAsset(FStaticMesh* InMesh);
	FStaticMesh* GetStaticMeshAsset() const;
	const TArray<FStaticMaterial>& GetStaticMaterials() const;

private:
	FStaticMesh* StaticMeshAsset = nullptr;
	TArray<FStaticMaterial> StaticMaterials; // 슬롯 이름과 머티리얼 인터페이스를 묶어서 저장하는 배열
};
