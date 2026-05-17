#pragma once
#include "Object/Object.h"
#include <d3d11.h>

struct FTexture
{
	~FTexture() { Release(); }

	void Release()
	{
		if (SRV)
		{
			SRV->Release();
			SRV = nullptr;
		}
	}

	ID3D11ShaderResourceView* SRV = nullptr;
};

UCLASS()
class UTexture : public UObject
{
public:
	GENERATED_BODY(UTexture, UObject)
	~UTexture() override
	{
		TextureData.Release();
	}

	bool LoadFromFile(const FString& InFilePath, ID3D11Device* InDevice);

	const FString& GetFilePath() const { return FilePath; }
	FString& GetFilePathRef() { return FilePath; }

	ID3D11ShaderResourceView** GetAddressOfSRV() { return &TextureData.SRV; }
	ID3D11ShaderResourceView* GetSRV() const { return TextureData.SRV; }

private:
	FString FilePath;
	FTexture TextureData;
};
