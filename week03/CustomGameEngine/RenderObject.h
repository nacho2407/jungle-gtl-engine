#pragma once
#include "UObject.h"
#include "d3d11.h"
#include "FMatrix.h"
class FMaterial;
class FGeometry;

struct RenderObject
{
public:
	bool bIsDirty;
	virtual void Render(ID3D11DeviceContext* Context);

public:
	FGeometry* Geometry;//Mesh
	FMaterial* Material;//Material
	FMatrix World;
	//Texture

	bool bIsSelected = false;
	bool bDepthEnabled = true;
};