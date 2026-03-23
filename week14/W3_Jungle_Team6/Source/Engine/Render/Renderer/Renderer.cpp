#include "Renderer.h"

#include <iostream>
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Mesh/MeshManager.h"

void FRenderer::Create(HWND hWindow)
{
	Device.Create(hWindow);

	if (Device.GetDevice() == nullptr)
	{
		std::cout << "Failed to create D3D Device." << std::endl;
	}

	// 1. 일반 메쉬 (Primitive.hlsl)
	Resources.PrimitiveShader.Create(Device.GetDevice(), L"Shaders/Primitive.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 2. 기즈모 (Gizmo.hlsl)
	Resources.GizmoShader.Create(Device.GetDevice(), L"Shaders/Gizmo.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 3. 오버레이 (Overlay.hlsl)
	Resources.OverlayShader.Create(Device.GetDevice(), L"Shaders/Overlay.hlsl",
		"VS", "PS", OverlayInputLayout, ARRAYSIZE(OverlayInputLayout));

	// 4. 에디터/라인 (Editor.hlsl)
	Resources.EditorShader.Create(Device.GetDevice(), L"Shaders/Editor.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 5. 아웃라인 (Outline.hlsl)
	Resources.OutlineShader.Create(Device.GetDevice(), L"Shaders/Outline.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	Resources.PerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FPerObjectConstants));
	Resources.FrameBuffer.Create(Device.GetDevice(), sizeof(FFrameConstants));
	Resources.GizmoPerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FGizmoConstants));
	Resources.OverlayConstantBuffer.Create(Device.GetDevice(), sizeof(FOverlayConstants));
	Resources.EditorConstantBuffer.Create(Device.GetDevice(), sizeof(FEditorConstants));
	Resources.OutlineConstantBuffer.Create(Device.GetDevice(), sizeof(FOutlineConstants));

	//	MeshManager init
	FMeshManager::Initialize();

	LineBatcher.Create(Device.GetDevice());

	// 텍스처는 ResourceManager가 소유 — Batcher는 셰이더/버퍼만 초기화
	FontBatcher.Create(Device.GetDevice());
	SubUVBatcher.Create(Device.GetDevice());
}

void FRenderer::Release()
{
	Resources.PrimitiveShader.Release();
	Resources.GizmoShader.Release();
	Resources.OverlayShader.Release();
	Resources.EditorShader.Release();
	Resources.OutlineShader.Release();

	Resources.PerObjectConstantBuffer.Release();
	Resources.FrameBuffer.Release();
	Resources.GizmoPerObjectConstantBuffer.Release();
	Resources.OverlayConstantBuffer.Release();
	Resources.EditorConstantBuffer.Release();
	Resources.OutlineConstantBuffer.Release();

	LineBatcher.Release();
	FontBatcher.Release();
	SubUVBatcher.Release();

	Device.Release();
}
//	Prepare the rendering state for a new frame. 반드시 Render 이전에 호출되어야 함.
void FRenderer::BeginFrame()
{
	Device.BeginFrame();

	LineBatcher.Clear();
	FontBatcher.Clear();
	SubUVBatcher.Clear();
}
//	Render Update Main function. RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행
void FRenderer::Render(const FRenderBus& InRenderBus, ERasterizerState InViewModeRasterizer)
{
	ID3D11DeviceContext* context = Device.GetDeviceContext();
	UpdateFrameBuffer(context, InRenderBus.GetView(), InRenderBus.GetProj());

	RenderPasses(InRenderBus, context);
	RenderEditorHelpers(InRenderBus, context);
}

void FRenderer::RenderOverlay(const FRenderBus& InRenderBus)
{
	ID3D11DeviceContext* context = Device.GetDeviceContext();
	//RenderOverlayPass(context, InRenderBus);
}

void FRenderer::SetupRenderState(ERenderPass Pass, ID3D11DeviceContext* DeviceContext, EViewMode CurViewMode)
{
	switch (Pass)
	{
	case ERenderPass::Opaque:
		if (CurViewMode == EViewMode::Wireframe)
		{
			Device.SetRasterizerState(ERasterizerState::WireFrame);
		}
		else
		{
			Device.SetRasterizerState(ERasterizerState::SolidBackCull);
		}
		Device.SetDepthStencilState(EDepthStencilState::Default);
		Device.SetBlendState(EBlendState::Opaque);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Resources.PrimitiveShader.Bind(DeviceContext);
		break;

	case ERenderPass::Outline:
		if (CurViewMode == EViewMode::Wireframe)
		{
			Device.SetDepthStencilState(EDepthStencilState::GizmoOutside);
			Device.SetRasterizerState(ERasterizerState::WireFrame);
		}
		else
		{
			Device.SetDepthStencilState(EDepthStencilState::StencilOutline);
			Device.SetRasterizerState(ERasterizerState::SolidFrontCull);
		}
		Device.SetBlendState(EBlendState::Opaque);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Resources.OutlineShader.Bind(DeviceContext);
		break;

	case ERenderPass::DepthLess:
		Device.SetDepthStencilState(EDepthStencilState::DepthReadOnly);
		Device.SetBlendState(EBlendState::AlphaBlend);
		Device.SetRasterizerState(ERasterizerState::SolidBackCull);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Resources.GizmoShader.Bind(DeviceContext);
		break;

	case ERenderPass::Editor:
		Device.SetDepthStencilState(EDepthStencilState::Default);
		Device.SetRasterizerState(ERasterizerState::SolidBackCull);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		Resources.EditorShader.Bind(DeviceContext);
		break;

	case ERenderPass::Overlay:
		Device.SetDepthStencilState(EDepthStencilState::DepthGreater);
		Device.SetBlendState(EBlendState::Opaque);
		Device.SetRasterizerState(ERasterizerState::SolidBackCull);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Resources.OverlayShader.Bind(DeviceContext);
		break;
	}
}

void FRenderer::BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context)
{
	if (InCmd.Type != ERenderCommandType::Overlay)
	{
		Resources.PerObjectConstantBuffer.Update(Context, &InCmd.PerObjectConstants, sizeof(FPerObjectConstants));
		ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
		Context->VSSetConstantBuffers(1, 1, &cb);
		//InDeviceContext->PSSetConstantBuffers(0, 1, &cb);
	}

	switch (InCmd.Type)
	{
	case ERenderCommandType::Gizmo:
		Resources.GizmoShader.Bind(Context);
		Resources.GizmoPerObjectConstantBuffer.Update(Context, &InCmd.Constants.Gizmo, sizeof(FGizmoConstants));
		{
			ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(1, 1, &cb);
			cb = Resources.GizmoPerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(2, 1, &cb);
			Context->PSSetConstantBuffers(2, 1, &cb);
		}
		break;

	case ERenderCommandType::Overlay:
		Resources.OverlayConstantBuffer.Update(Context, &InCmd.Constants.Overlay, sizeof(FOverlayConstants));
		{
			ID3D11Buffer* cb = Resources.OverlayConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(3, 1, &cb);
			Context->PSSetConstantBuffers(3, 1, &cb);
		}
		break;

	case ERenderCommandType::DebugBox:
		Resources.EditorConstantBuffer.Update(Context, &InCmd.Constants.Editor, sizeof(FEditorConstants));
		{
			ID3D11Buffer* cb = Resources.EditorConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(4, 1, &cb);
			Context->PSSetConstantBuffers(4, 1, &cb);
			cb = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(1, 1, &cb);
			Context->PSSetConstantBuffers(1, 1, &cb);
		}
		break;

	case ERenderCommandType::SelectionOutline:
		Resources.OutlineConstantBuffer.Update(Context, &InCmd.Constants.Editor, sizeof(FOutlineConstants));
		{
			ID3D11Buffer* cb = Resources.OutlineConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(5, 1, &cb);
			Context->PSSetConstantBuffers(5, 1, &cb);
			cb = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(1, 1, &cb);
		}
		break;
	}
}

EDepthStencilState FRenderer::GetDefaultDepthForPass(ERenderPass Pass) const
{
	switch (Pass)
	{
	case ERenderPass::Opaque:    return EDepthStencilState::Default;
	case ERenderPass::Outline:   return EDepthStencilState::StencilOutline;
	case ERenderPass::DepthLess: return EDepthStencilState::Default;
	case ERenderPass::Editor:    return EDepthStencilState::Default;
	case ERenderPass::Overlay:   return EDepthStencilState::DepthGreater;
	default:                     return EDepthStencilState::Default;
	}
}

EBlendState FRenderer::GetDefaultBlendForPass(ERenderPass Pass) const
{
	switch (Pass)
	{
	case ERenderPass::DepthLess: return EBlendState::AlphaBlend;
	default:                     return EBlendState::Opaque;
	}
}

void FRenderer::DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr || !InCommand.MeshBuffer->IsValid())
	{
		return;
	}

	uint32 offset = 0;
	ID3D11Buffer* vertexBuffer = InCommand.MeshBuffer->GetVertexBuffer().GetBuffer();
	if (vertexBuffer == nullptr)
	{
		return;
	}

	uint32 vertexCount = InCommand.MeshBuffer->GetVertexBuffer().GetVertexCount();
	uint32 stride = InCommand.MeshBuffer->GetVertexBuffer().GetStride();
	if (vertexCount == 0 || stride == 0)
	{
		return;
	}

	InDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetIndexBuffer().GetBuffer();
	if (indexBuffer != nullptr)
	{
		uint32 indexCount = InCommand.MeshBuffer->GetIndexBuffer().GetIndexCount();
		InDeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		InDeviceContext->DrawIndexed(indexCount, 0, 0);
	}
	else
	{
		InDeviceContext->Draw(vertexCount, 0);
	}
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
	Device.EndFrame();
}

void FRenderer::RenderPasses(const FRenderBus& RenderBus, ID3D11DeviceContext* Context)
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);

		// Font / SubUV 패스는 Batcher가 처리 — 일반 DrawCommand 루프에서 제외
		if (CurPass == ERenderPass::Font || CurPass == ERenderPass::SubUV) continue;

		const auto& Commands = RenderBus.GetCommands(CurPass);
		if (Commands.empty()) continue;

		SetupRenderState(CurPass, Context, RenderBus.GetViewMode());

		for (const auto& Cmd : Commands)
		{
			EDepthStencilState TargetDepth = (Cmd.DepthStencilState != EDepthStencilState::Default)
				? Cmd.DepthStencilState
				: GetDefaultDepthForPass(CurPass);

			EBlendState TargetBlend = (Cmd.BlendState != EBlendState::Opaque)
				? Cmd.BlendState
				: GetDefaultBlendForPass(CurPass);

			Device.SetDepthStencilState(TargetDepth);
			Device.SetBlendState(TargetBlend);

			BindShaderByType(Cmd, Context);
			DrawCommand(Context, Cmd);
		}
	}
}

void FRenderer::RenderEditorHelpers(const FRenderBus& RenderBus, ID3D11DeviceContext* Context)
{
	// --- AABB 디버그 박스 (LineBatcher) ---
	const auto& EditorCmds = RenderBus.GetCommands(ERenderPass::Editor);
	for (const auto& Cmd : EditorCmds)
	{
		if (Cmd.Type == ERenderCommandType::DebugBox)
		{
			LineBatcher.AddAABB(FBoundingBox{ Cmd.Constants.AABB.Min, Cmd.Constants.AABB.Max }, Cmd.Constants.AABB.Color);
		}
	}

	// --- SubUV 패스 → SubUVBatcher ---
	const auto& SubUVCmds = RenderBus.GetCommands(ERenderPass::SubUV);
	for (const auto& Cmd : SubUVCmds)
	{
		if (Cmd.Type == ERenderCommandType::SubUV && Cmd.AtlasResource)
		{
			SubUVBatcher.AddSprite(
				Cmd.PerObjectConstants.Model.GetLocation(),
				RenderBus.GetCameraRight(),
				RenderBus.GetCameraUp(),
				Cmd.FrameIndex,
				Cmd.AtlasResource->Columns,
				Cmd.AtlasResource->Rows,
				Cmd.SpriteSize.X,
				Cmd.SpriteSize.Y
			);
		}
	}

	// --- Font 패스 → FontBatcher ---
	const auto& FontCmds = RenderBus.GetCommands(ERenderPass::Font);
	for (const auto& Cmd : FontCmds)
	{
		if (Cmd.Type == ERenderCommandType::Font && !Cmd.TextData.empty())
		{
			FontBatcher.AddText(
				Cmd.TextData,
				Cmd.PerObjectConstants.Model.GetLocation(),
				RenderBus.GetCameraRight(),
				RenderBus.GetCameraUp(),
				Cmd.SpriteSize.X // FontSize
			);
		}
	}

	// --- Editor 렌더 상태 설정 ---
	Device.SetDepthStencilState(EDepthStencilState::Default);
	Device.SetBlendState(EBlendState::Opaque);

	Resources.EditorShader.Bind(Context);

	ID3D11Buffer* cb = Resources.EditorConstantBuffer.GetBuffer();
	Context->VSSetConstantBuffers(4, 1, &cb);
	Context->PSSetConstantBuffers(4, 1, &cb);

	cb = Resources.PerObjectConstantBuffer.GetBuffer();
	Context->VSSetConstantBuffers(1, 1, &cb);
	Context->PSSetConstantBuffers(1, 1, &cb);

	if (RenderBus.GetShowFlags().bGrid)
	{
		LineBatcher.AddWorldGrid(100.0f, 20);
	}

	LineBatcher.Flush(Context);

	// --- FontBatcher Flush ---
	// SRV는 ResourceManager 소유 리소스에서 가져옴
	Device.SetDepthStencilState(EDepthStencilState::Default);
	Device.SetBlendState(EBlendState::AlphaBlend);

	// --- SubUVBatcher Flush ---
	// 단일 아틀라스 기준: 커맨드에서 첫 번째 유효한 SRV를 사용
	ID3D11ShaderResourceView* SubUVSRV = nullptr;
	for (const auto& Cmd : SubUVCmds)
	{
		if (Cmd.AtlasResource && Cmd.AtlasResource->IsLoaded())
		{
			SubUVSRV = Cmd.AtlasResource->SRV;
			break;
		}
	}
	SubUVBatcher.Flush(Context, SubUVSRV);

	const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
	FontBatcher.Flush(Context, FontRes);
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
{
	FFrameConstants frameConstantData;
	frameConstantData.View = ViewMatrix;
	frameConstantData.Projection = ProjMatrix;

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(0, 1, &b0);
	Context->PSSetConstantBuffers(0, 1, &b0);
}
