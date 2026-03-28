#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Resource/Buffer.h"

class FMeshBufferManager : public TSingleton<FMeshBufferManager>
{
	friend class TSingleton<FMeshBufferManager>;

public:
	void Initialize(ID3D11Device* InDevice);
	void Release();

	FMeshBuffer& GetMeshBuffer(EPrimitiveType InPrimitiveType);
	const FMeshData& GetMeshData(EPrimitiveType InPrimitiveType) const;

private:
	FMeshBufferManager() = default;

	// CPU 메시 데이터 생성
	void CreatePrimitiveMeshData();
	void CreateCube();
	void CreatePlane();
	void CreateSphere(int Slices = 20, int Stacks = 20);
	void CreateTranslationGizmo();
	void CreateRotationGizmo();
	void CreateScaleGizmo();
	void CreateQuad();

	// CPU 메시 데이터
	TMap<EPrimitiveType, FMeshData> MeshDataMap;

	// GPU 버퍼
	TMap<EPrimitiveType, FMeshBuffer> MeshBufferMap;

	bool bIsInitialized = false;
};
