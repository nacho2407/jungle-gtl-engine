#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>
#include <filesystem>

class ENGINE_API FTextureLoader
{
public:
	static bool CreateTextureFromSTB(ID3D11Device* Device, const char* FilePath, ID3D11ShaderResourceView** OutSRV);
	static bool CreateTextureFromSTB(ID3D11Device* Device, const std::filesystem::path& FilePath, ID3D11ShaderResourceView** OutSRV);
};
