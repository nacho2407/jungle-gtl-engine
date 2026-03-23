#pragma once

#include "Math/Matrix.h"
#include "EngineCore/Color.h"
#include "Math/Vector4.h"
#include <d3d11.h>
//#include "tup"
struct FMaterial;
struct FGeometry;

enum class EEngineShowFlags
{
	SF_Primitives,
	SF_BillboardText,
	SF_EditorPrimitives,
	SF_DebugLines,
	SF_None,
};

struct BatchKey {
	FGeometry* Geometry;
	FMaterial* Material;
	D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology;

	bool operator<(const BatchKey& other) const {
		return std::tie(Material, PrimitiveTopology) <
			std::tie(other.Material, other.PrimitiveTopology);
	}
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
	FColor Color = FColor(1.0f, 1.0f, 1.0f, 1.0f);
	FVector4 UVTransform = FVector4(1.0f, 1.0f, 0.0f, 0.0f);

	BatchKey GetBatchKey() const
	{
		return { Geometry, Material, PrimitiveTopology };
	}
	//Texture

	bool bIsSelected = false;
	bool bDepthEnabled = true;
	bool bDisableCulling = false;
	bool bBatch = false;
	EEngineShowFlags ShowFlag = EEngineShowFlags::SF_None;

	D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};