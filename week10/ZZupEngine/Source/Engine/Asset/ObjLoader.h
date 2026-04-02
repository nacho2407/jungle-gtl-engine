#pragma once

#include "Asset/ObjRawTypes.h"
#include "Asset/StaticMeshTypes.h"
#include "Asset/IAssetLoader.h"
#include <Core/ResourceTypes.h>

class FObjLoader : public IAssetLoader
{
public:
	FObjLoader() = default;
	~FObjLoader() override = default;

	FStaticMesh* Load(const FString& Path, const FStaticMeshLoadOptions& LoadOptions);

	bool SupportsExtension(const FString& Extension) const override;
	FString GetLoaderName() const override;

private:
	//	OBJ -> Raw Data
	bool ParseObj(const FString& Path);
	//	Raw Data -> Cooked Data
	bool BuildStaticMesh();
	void Reset();

	/* Helpers */
	bool ParsePositionLine(const FString& Line);
	bool ParseTexCoordLine(const FString& Line);
	bool ParseNormalLine(const FString& Line);
	void ParseMtllibLine(const FString& Line);
	void ParseUseMtlLine(const FString &Line, FString & CurrentMaterialName);
	bool ParseFaceLine(const FString& Line, const FString &CurrentMaterialName);
	bool ParseFaceVertexToken(const FString& Token, FObjRawIndex & OutIndex);
	
	FNormalVertex MakeVertex(const FObjRawIndex & RawIndex) const;
	uint32 GetOrCreateVertexIndex(const FObjRawIndex & RawIndex, TMap<FObjVertexKey, uint32> & VertexMap);
	
	void NormalizeRawPositionsToUnitCube();
	void NormalizeRawSizeToUnitCube();
	
	int32 GetOrAddMaterialSlot(const FString& MaterialName);
	FAABB BuildLocalBounds() const;
private:
	FString SourcePath;
	FObjRawData RawData;
	FStaticMesh StaticMeshAsset;
	
	TArray<FString> BuiltMaterialSlotName;
};
