#pragma once

/*
	Shader, Constant Buffer 등 렌더링에 필요한 리소스들을 관리하는 Class 입니다.
	Renderer에서 필요한 리소스들을 FRenderResources에 추가하여 관리할 수 있습니다.
*/

#include "Render/Resource/Shader.h"
#include "Render/Resource/Buffer.h"

struct FRenderResources
{
	FConstantBuffer FrameBuffer;					// b0
    FConstantBuffer PerObjectConstantBuffer;        // b1
    FConstantBuffer GizmoPerObjectConstantBuffer;   // b2
    FConstantBuffer EditorConstantBuffer;           // b4
	FConstantBuffer OutlineConstantBuffer;          // b5

    FConstantBuffer StaticMeshConstantBuffer;       // b6
    FConstantBuffer LightPassConstantBuffer;		// b7
	FConstantBuffer DecalConstantBuffer;			// b8
    FConstantBuffer FogPassConstantBuffer;		// b9
    FConstantBuffer FXAAConstantBuffer;                     // b10

    UShader* PrimitiveShader;
    UShader* GizmoShader;
    UShader* EditorShader;
	UShader* SelectionMaskShader;
	UShader* OutlineShader;
	UShader* StaticMeshShader;
	UShader* LightPassShader;
	UShader* FogPassShader;
	UShader* DecalShader;
	UShader* FXAAShader;
	FStructuredBuffer LightStructuredBuffer;	// t3

	TComPtr<ID3D11SamplerState> MeshSamplerState;
};
