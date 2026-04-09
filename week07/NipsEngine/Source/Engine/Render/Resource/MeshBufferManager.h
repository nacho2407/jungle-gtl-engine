#pragma once

#include "Core/CoreTypes.h"
#include "Render/Common/RenderTypes.h"

#include "Render/Resource/Buffer.h"

#include "Render/Mesh/MeshManager.h"

class UStaticMesh;

/*
	Mesh Manager에서 넘겨 받은 MeshData를 바탕으로 MeshBuffer를 생성하고 소유합니다.
*/

class FMeshBufferManager
{
private:
	// 최대 LOD 레벨을 양쪽에서 저장하고 있습니다... 
	// MAX_LOD를 수정하실 필요가 있다면 MeshBufferManager를 찾아 함께 수정해주세요.
	static constexpr int32 MAX_LOD = 5;

	ID3D11Device* Device = nullptr;
	TMap<EPrimitiveType, FMeshBuffer> MeshBufferMap;
	TMap<const UStaticMesh*, FMeshBuffer> StaticMeshBufferMap[MAX_LOD];

public:

private:

public:
	void Create(ID3D11Device* InDevice);
	void Release();

	FMeshBuffer& GetMeshBuffer(EPrimitiveType InPrimitiveType);
	FMeshBuffer* GetStaticMeshBuffer(const UStaticMesh* StaticMeshAsset, int32 LODLevel = 0);
};