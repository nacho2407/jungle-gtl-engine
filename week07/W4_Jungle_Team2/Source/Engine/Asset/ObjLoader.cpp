#include "ObjLoader.h"

#include "FileUtils.h"

//	v, vt, vn, mtllib, usemtl, f
UStaticMesh* FObjLoader::Load(const FString& Path)
{
	Reset();
	
	if (!ParseObj(Path))
	{
		return nullptr;
	}
	
	if (!BuildStaticMesh())
	{
		return nullptr;
	}
	
	if (!BindMaterials())
	{
		return nullptr;
	}
	
	return CreateAsset();
}

bool FObjLoader::SupportsExtension(const FString& Extension) const
{
	return Extension == FString("obj");
}

FString FObjLoader::GetLoaderName() const
{
	return FString{"FObjLoader"};
}

bool FObjLoader::ParseObj(const FString& Path)
{
	TArray<FString> Lines;
	
	if (!FFileUtils::LoadFileToLines(Path, Lines))
	{
		return false;
	}
	
	FString CurrentMaterialName;
	
	for (const auto & RawLine : Lines)
	{
		FString Line = RawLine;
		Line.
	}
	
}

bool FObjLoader::BuildStaticMesh()
{
}

bool FObjLoader::BindMaterials()
{
}

UStaticMesh* FObjLoader::CreateAsset()
{
	
}

void FObjLoader::Reset()
{
	SourcePath.clear();
	
	RawData = {};
	StaticMeshAsset = {};
}

#pragma region __HELPER__

//	v
bool FObjLoader::ParsePositionLine(const FString& Line)
{
}

//	vt
bool FObjLoader::ParseTexCoordLine(const FString& Line)
{
}

//	vn
bool FObjLoader::ParseNormalLine(const FString& Line)
{
}

//	mtllib
void FObjLoader::ParseMtllibLine(const FString& Line)
{
}

//	usemtl
void FObjLoader::ParseUseMtlLine(const FString& Line, FString& CurrentMaterialName)
{
}

//	f
bool FObjLoader::ParseFaceLine(const FString& Line, const FString& CurrentMaterialName)
{
}

//	Obj index는 1-based이기에 0-based로 변경
bool FObjLoader::ParseFaceVertexToken(const FString& Token, FObjRawIndex& OutIndex)
{
}

#pragma endregion 