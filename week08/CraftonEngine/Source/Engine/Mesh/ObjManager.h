#pragma once

#include "Core/CoreTypes.h"
#include <map>
#include <string>

struct FStaticMesh;
class UStaticMesh;

// 에셋 로드/캐싱 관리자
class FObjManager
{
	static std::map<std::string, FStaticMesh*> ObjStaticMeshMap;

public:
	static FStaticMesh* LoadObjStaticMeshAsset(const std::string& PathFileName);
	static UStaticMesh* LoadObjStaticMesh(const std::string& PathFileName);
};
