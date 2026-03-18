#pragma once

#include "Math/Matrix.h"
#include "Color.h"
#include <d3d11.h>

struct FMaterial;
struct FGeometry;

struct RenderObject
{
public:
	bool bIsDirty;
	virtual void Render(ID3D11DeviceContext* Context);

public:
	FGeometry* Geometry;//Mesh
	FMaterial* Material;//Material
	FMatrix World;
	FColor Color = FColor(1.0f, 1.0f, 1.0f, 1.0f);

	//Texture

	bool bIsSelected = false;
	bool bDepthEnabled = true;

	D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};