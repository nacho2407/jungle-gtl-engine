#pragma once

#include "Core/Containers/String.h"
#include "Core/Paths.h"

namespace FShaderPaths
{
	// Shader 경로를 한 곳에서 관리합니다.
	// HLSL 폴더 구조가 바뀔 때 하드코딩 문자열이 흩어지지 않도록 하기 위한 용도입니다.
	inline constexpr const char* MaterialUberLit = "Shaders/Material/UberLit.hlsl";
	inline constexpr const char* MaterialDecal = "Shaders/Material/Decal.hlsl";

	inline constexpr const char* UIFont = "Shaders/UI/Font.hlsl";
	inline constexpr const char* UILine = "Shaders/UI/Line.hlsl";
	inline constexpr const char* UISubUV = "Shaders/UI/SubUV.hlsl";
	inline constexpr const char* UIScreenOverlay = "Shaders/UI/ScreenOverlay.hlsl";

	inline constexpr const char* EditorGizmo = "Shaders/EditorDebug/Gizmo.hlsl";
	inline constexpr const char* EditorPrimitive = "Shaders/EditorDebug/Primitive.hlsl";
	inline constexpr const char* EditorMain = "Shaders/EditorDebug/Editor.hlsl";
	inline constexpr const char* EditorSelectionMask = "Shaders/EditorDebug/SelectionMask.hlsl";
	inline constexpr const char* EditorIDPick = "Shaders/EditorDebug/IDPick.hlsl";
	inline constexpr const char* EditorIDPickDebug = "Shaders/EditorDebug/IDPickDebug.hlsl";

	inline constexpr const char* DepthPrepass = "Shaders/Depth/DepthPrepass.hlsl";
	inline constexpr const char* Shadow = "Shaders/Shadow/Shadow.hlsl";
	inline constexpr const char* VSMShadow = "Shaders/Shadow/VSMShadow.hlsl";
	inline constexpr const char* VSMBlurCompute = "Shaders/Shadow/VSMBlurComputeShader.hlsl";

	inline constexpr const char* PostProcessLight = "Shaders/PostProcess/LightPass.hlsl";
	inline constexpr const char* PostProcessFog = "Shaders/PostProcess/FogPass.hlsl";
	inline constexpr const char* PostProcessSandervistan = "Shaders/PostProcess/SandervistanPass.hlsl";
	inline constexpr const char* PostProcessFXAA = "Shaders/PostProcess/FXAAPass.hlsl";
	inline constexpr const char* PostProcessMain = "Shaders/PostProcess/PostProcess.hlsl";
	inline constexpr const char* PostProcessOutline = "Shaders/PostProcess/Outline.hlsl";

	inline constexpr const char* ComputeLightCulling = "Shaders/Compute/LightCullingCS.hlsl";

	// Material 파일에 PixelShader만 적혀 있을 때 기본 PS EntryPoint를 정합니다.
	// 대부분은 mainPS지만, 일부 기존 UI/Editor 셰이더는 PS 이름을 그대로 사용합니다.
	inline FString GetDefaultPixelShaderEntryPoint(const FString& ShaderPath)
	{
		const FString NormalizedPath = FPaths::Normalize(ShaderPath);
		if (NormalizedPath == UIFont
			|| NormalizedPath == UISubUV
			|| NormalizedPath == EditorGizmo
			|| NormalizedPath == EditorMain
			|| NormalizedPath == EditorPrimitive
			|| NormalizedPath == PostProcessOutline)
		{
			return "PS";
		}

		return "mainPS";
	}
}
