#include "Mesh/ObjManager.h"
#include "Mesh/StaticMeshAsset.h"
#include "Mesh/StaticMesh.h"

std::map<std::string, FStaticMesh*> FObjManager::ObjStaticMeshMap;

FStaticMesh* FObjManager::LoadObjStaticMeshAsset(const std::string& PathFileName)
{
	return nullptr;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const std::string& PathFileName)
{
	return nullptr;
}
