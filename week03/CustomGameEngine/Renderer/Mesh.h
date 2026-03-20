//TODO : NOT DONE YET USE THIS TO REFACTOR RENDEROBJECT

#pragma once

#include "Object.h"
#include <d3d11.h>

class UMesh : public UObject
{
	DECLARE_RTTI(UMesh, UObject)



public:
	ID3D11Buffer* VertexBuffer;
	ID3D11InputLayout* InputLayout;
};