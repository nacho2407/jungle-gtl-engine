#pragma once

#include "ObjRawTypes.h"
#include "StaticMeshCookedTypes.h"
#include "Asset/IAssetLoader.h"

class UStaticMesh;

class FObjLoader : public IAssetLoader
{
public:
	FObjLoader() = default;
	~FObjLoader() override = default;

	UStaticMesh* Load(const FString& Path);

	bool SupportsExtension(const FString& Extension) const override;
	FString GetLoaderName() const override;

private:
	//	OBJ -> Raw Data
	bool ParseObj(const FString& Path);
	//	Raw Data -> Cooked Data
	bool BuildStaticMesh();
	//	Cooked Data Slot -> Material Binding
	bool BindMaterials();

	//	CookedData -> UStaticMesh
	UStaticMesh* CreateAsset();
	void Reset();

	/* Helpers */
	bool ParsePositionLine(const FString& Line);
	bool ParseTexCoordLine(const FString& Line);
	bool ParseNormalLine(const FString& Line);
	void ParseMtllibLine(const FString& Line);
	void ParseUseMtlLine(const FString &Line, FString & CurrentMaterialName);
	bool ParseFaceLine(const FString& Line, const FString &CurrentMaterialName);
	bool ParseFaceVertexToken(const FString& Token, FObjRawIndex & OutIndex);
	
	
private:
	FString SourcePath;
	FObjRawData RawData;
	FStaticMesh StaticMeshAsset;
};
