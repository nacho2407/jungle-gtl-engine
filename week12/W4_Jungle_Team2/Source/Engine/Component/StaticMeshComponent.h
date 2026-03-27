#pragma once
#include "MeshComponent.h"

class FStaticMesh;

class UStaticMeshComponent : public UMeshComponent
{
private:
	FStaticMesh* StaticMeshAsset;

public:
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent);

};
