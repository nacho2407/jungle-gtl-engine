#pragma once

#include "Core/CoreTypes.h"
#include "Object/ObjectIterator.h"
#include "Render/Common/RenderTypes.h"
#include <map>
#include <string>

struct FStaticMesh;
class UStaticMesh;

// 에셋 로드/캐싱 관리자
class FObjManager
{
	static std::map<std::string, FStaticMesh> ObjStaticMeshMap;
	static ID3D11Device* GDevice;

public:
	static std::string GetBinaryFilePath(const std::string& OriginalPath);
	static void SetDevice(ID3D11Device* InDevice) { GDevice = InDevice; }
	static FStaticMesh* LoadObjStaticMeshAsset(const std::string& PathFileName);
	static UStaticMesh* LoadObjStaticMesh(const std::string& PathFileName);
};