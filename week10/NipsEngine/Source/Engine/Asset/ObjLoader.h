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
	bool ParseObj(const FString& Path, FObjRawData& InRawData);
	//	Raw Data -> Cooked Data
	bool BuildStaticMesh(const FString& Path, FStaticMesh* InStaticMesh, FObjRawData& RawData);

	/* Helpers */
	bool ParsePositionLine(const FString& Line, FObjRawData& InRawData);
	bool ParseTexCoordLine(const FString& Line, FObjRawData& InRawData);
	bool ParseNormalLine(const FString& Line, FObjRawData& InRawData);
	void ParseMtllibLine(const FString& Line, FObjRawData& InRawData);
	void ParseUseMtlLine(const FString &Line, FString& CurrentMaterialName, FObjRawData& InRawData);
	bool ParseFaceLine(const FString& Line, const FString& CurrentMaterialName, FObjRawData& InRawData);
	bool ParseFaceVertexToken(const FString& Token, FObjRawIndex& OutIndex, FObjRawData& InRawData);
	
	FNormalVertex MakeVertex(const FObjRawIndex& RawIndex, FObjRawData& RawData) const;
	uint32 GetOrCreateVertexIndex(const FObjRawIndex& RawIndex, TMap<FObjVertexKey, uint32>& VertexMap, FStaticMesh* StaticMesh, FObjRawData& RawData);
	
	void NormalizeRawPositionsToUnitCube(FObjRawData& RawData);
	void NormalizeRawSizeToUnitCube(FObjRawData& RawData);
	
	int32 GetOrAddMaterialSlot(const FString& MaterialName);
	FAABB BuildLocalBounds(FStaticMesh* InStaticMesh) const;

private:
	TArray<FString> BuiltMaterialSlotName;
};
