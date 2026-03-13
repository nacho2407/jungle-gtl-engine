#include "ResourceManager.h"

TMap<FString, VertexResource> ResourceManager::VertexResourceMap;
TMap<WFString, ID3D11VertexShader*> ResourceManager::VertexShaderMap;
TMap<WFString, ID3D11PixelShader*> ResourceManager::PixelShaderMap;
TMap<WFString, ID3D11InputLayout*> ResourceManager::InputLayoutMap;