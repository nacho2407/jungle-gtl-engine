#pragma once

#include "Materials/Graph/MaterialGraphTypes.h"

struct FMaterialCompileOptions
{
	FString MaterialPath;
	FString MaterialGuid;
	EMaterialDomain Domain = EMaterialDomain::Surface;
	ERenderPass RenderPass = ERenderPass::Opaque;
	EBlendState BlendState = EBlendState::Opaque;
	EDepthStencilState DepthStencilState = EDepthStencilState::Default;
	ERasterizerState RasterizerState = ERasterizerState::SolidBackCull;
	bool bReceiveLighting = false; // ParticleMesh 전용 — Ambient + Directional 조명 적용
};

struct FMaterialCompileResult
{
	FString GeneratedShaderPath;
	FString GeneratedHlsl;

	TMap<FString, FMaterialCompiledParameter> Parameters;
	TMap<FString, FMaterialCompiledTexture>   Textures;
	TArray<FString> Errors;
};

class FMaterialHlslGenerator
{
public:
	static bool Generate(const FMaterialGraph& Graph, const FMaterialCompileOptions& Options, FMaterialCompileResult& OutResult);
};
