#pragma once
#include "MeshComponent.h"
#include "Engine/Render/Mesh/StaticMesh.h"

class UStaticMeshComponent : public UMeshComponent
{
private:
	UStaticMesh* StaticMesh = nullptr;

public:
	DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)

	void         SetStaticMesh(UStaticMesh* InMesh) { StaticMesh = InMesh; }
	UStaticMesh* GetStaticMesh() const              { return StaticMesh; }

	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_StaticMesh; }
};
