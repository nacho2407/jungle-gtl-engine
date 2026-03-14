#pragma once
#include "UObject.h"
#include "d3d11.h"
#include "FMatrix.h"
class FMaterial;
class FGeometry;

class URenderObject : public UObject
{
	DECLARE_RTTI(URenderObject, UObject)
public:
	URenderObject();
	virtual ~URenderObject() override;

	virtual void Render(ID3D11DeviceContext* Context);

public:
	FGeometry* Geometry;//Mesh
	FMaterial* Material;//Material
	FMatrix World;
	//Texture

	bool bIsSelected = false;
	bool bDepthEnabled = true;
};