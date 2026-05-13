#pragma once

namespace FShaderPaths
{
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
}
