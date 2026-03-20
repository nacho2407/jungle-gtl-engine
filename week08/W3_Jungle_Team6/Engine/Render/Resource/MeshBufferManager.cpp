#include "MeshBufferManager.h"

void FMeshBufferManager::Create(ID3D11Device* InDevice)
{
	MeshBufferMap[EPrimitiveType::EPT_Cube].Create(InDevice, FMeshManager::GetCube());
	MeshBufferMap[EPrimitiveType::EPT_Sphere].Create(InDevice, FMeshManager::GetSphere());
	MeshBufferMap[EPrimitiveType::EPT_Plane].Create(InDevice, FMeshManager::GetPlane());
	MeshBufferMap[EPrimitiveType::EPT_TransGizmo].Create(InDevice, FMeshManager::GetTranslationGizmo());
	MeshBufferMap[EPrimitiveType::EPT_RotGizmo].Create(InDevice, FMeshManager::GetRotationGizmo()); 
	MeshBufferMap[EPrimitiveType::EPT_ScaleGizmo].Create(InDevice, FMeshManager::GetScaleGizmo());
	MeshBufferMap[EPrimitiveType::EPT_Axis].Create(InDevice, FMeshManager::GetAxis());
	MeshBufferMap[EPrimitiveType::EPT_Grid].Create(InDevice, FMeshManager::GetGrid());
	MeshBufferMap[EPrimitiveType::EPT_MouseOverlay].Create(InDevice, FMeshManager::GetMouseOverlay());
}

//	TODO : 내일 하기
void FMeshBufferManager::Release()
{
	for (auto& pair : MeshBufferMap)
	{
		pair.second.Release();
	}
	MeshBufferMap.clear();
}

//	MeshBuffer는 VB, IB를 모두 포함하고 있습니다.
FMeshBuffer& FMeshBufferManager::GetMeshBuffer(EPrimitiveType InPrimitiveType)
{
	auto it = MeshBufferMap.find(InPrimitiveType);
	if (it != MeshBufferMap.end())
	{
		return it->second;
	}
	
	//	존재하지 않는 PrimitiveType이 요청된 경우, 기본적으로 CubeMeshBuffer를 반환하도록 합니다.
	return MeshBufferMap.at(EPrimitiveType::EPT_Cube);
}