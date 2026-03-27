#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshAsset.h"

FObjInfo FObjImporter::ParseObj(const std::string& FilePath)
{
	return {};
}

TArray<FObjMaterialInfo> FObjImporter::ParseMtl(const std::string& MtlPath)
{
	return {};
}

FStaticMesh* FObjImporter::Convert(const FObjInfo& ObjInfo)
{
	return nullptr;
}
