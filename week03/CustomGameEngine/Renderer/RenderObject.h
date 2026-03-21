#pragma once

#include "Math/Matrix.h"
#include "EngineCore/Color.h"
#include <d3d11.h>

struct FMaterial;
struct FGeometry;

struct BatchKey {
	FMaterial* Material;
	D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology;

	bool operator<(const BatchKey& other) const {
		return std::tie(Material, PrimitiveTopology) <
			std::tie(other.Material, other.PrimitiveTopology);
	};
};

struct RenderObject
{
public:
	bool bIsDirty;
	virtual void Render(ID3D11DeviceContext* Context);

public:
	FGeometry* Geometry;//Mesh
	FMaterial* Material;//Material
	FMatrix World = FMatrix::IdentityMatrix();
	FColor Color = FColor(1.0f, 1.0f, 1.0f, .5f);

	BatchKey GetBatchKey() const
	{
		return { Material, PrimitiveTopology };
	}
	//Texture

	bool bIsSelected = false;
	bool bDepthEnabled = true;
	bool bBatch = false;

	D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};