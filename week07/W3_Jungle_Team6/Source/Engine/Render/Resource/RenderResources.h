#pragma once

/*
	Shader, Constant Buffer 등 렌더링에 필요한 리소스들을 관리하는 Class 입니다.
	Renderer에서 필요한 리소스들을 FRenderResources에 추가하여 관리할 수 있습니다.
*/

#include "Render/Resource/Shader.h"
#include "Render/Resource/Buffer.h"

struct FRenderResources
{
    FConstantBuffer PerObjectConstantBuffer;        // b0
    FConstantBuffer GizmoPerObjectConstantBuffer;   // b1
    FConstantBuffer OverlayConstantBuffer;          // b2
    FConstantBuffer EditorConstantBuffer;           // b3
	FConstantBuffer OutlineConstantBuffer;          // b4

    FShader PrimitiveShader;
    FShader GizmoShader;
    FShader OverlayShader;
    FShader EditorShader;
	FShader OutlineShader;
};