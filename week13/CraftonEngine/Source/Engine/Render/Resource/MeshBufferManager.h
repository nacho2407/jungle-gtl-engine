#pragma once

#include "Core/CoreTypes.h"
#include "Render/Common/RenderTypes.h"

#include "Render/Resource/Buffer.h"
#include <map>
#include "Render/Mesh/MeshManager.h"

class FStaticMesh;
class UStaticMesh;

/*
	Mesh Manager에서 넘겨 받은 MeshData를 바탕으로 MeshBuffer를 생성하고 소유합니다.
*/

class FMeshBufferManager
{
private:
	TMap<EPrimitiveType, FMeshBuffer> MeshBufferMap;

public:

private:

public:
	void Create(ID3D11Device* InDevice);
	void Release();

	FMeshBuffer& GetMeshBuffer(EPrimitiveType InPrimitiveType);

};