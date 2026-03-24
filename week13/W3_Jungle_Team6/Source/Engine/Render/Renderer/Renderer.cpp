#include "Renderer.h"

#include <iostream>
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Mesh/MeshManager.h"
#include "Core/Stats.h"
#include "Core/GPUProfiler.h"


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

	// GPU Profiler 초기화
	FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
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

	FGPUProfiler::Get().Shutdown();

	LineBatcher.Release();
	FontBatcher.Release();
	SubUVBatcher.Release();

	Device.Release();
}
//	Prepare the rendering state for a new frame. 반드시 Render 이전에 호출되어야 함.
void FRenderer::BeginFrame()
{
	Device.BeginFrame();

#if STATS
	FGPUProfiler::Get().BeginFrame();
#endif

	LineBatcher.Clear();
	FontBatcher.Clear();
	SubUVBatcher.Clear();
}
//	Render Update Main function. RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행
void FRenderer::Render(const FRenderBus& InRenderBus, ERasterizerState InViewModeRasterizer)
{
	bool bIsWireframe = InRenderBus.GetViewMode() == EViewMode::Wireframe;
	ID3D11DeviceContext* context = Device.GetDeviceContext();
	UpdateFrameBuffer(context, InRenderBus.GetView(), InRenderBus.GetProj(), bIsWireframe);

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

	case ERenderPass::StencilMask:
		Device.SetRasterizerState(ERasterizerState::SolidBackCull);
		Device.SetDepthStencilState(EDepthStencilState::Default);
		Device.SetBlendState(EBlendState::Opaque);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Resources.PrimitiveShader.Bind(DeviceContext);
		break;

	case ERenderPass::Outline:
		if (CurViewMode == EViewMode::Wireframe)
		{
			Device.SetDepthStencilState(EDepthStencilState::StencilOutline);
			Device.SetRasterizerState(ERasterizerState::SolidFrontCull);
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

	case ERenderPass::Translucent:
		Device.SetDepthStencilState(EDepthStencilState::Default);
		Device.SetBlendState(EBlendState::AlphaBlend);
		Device.SetRasterizerState(ERasterizerState::SolidBackCull);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Resources.PrimitiveShader.Bind(DeviceContext);
		break;

	case ERenderPass::Editor:
		Device.SetDepthStencilState(EDepthStencilState::Default);
		Device.SetBlendState(EBlendState::Opaque);
		if (CurViewMode == EViewMode::Wireframe)
		{
			Device.SetRasterizerState(ERasterizerState::WireFrame);
		}
		else
		{
			Device.SetRasterizerState(ERasterizerState::SolidBackCull);
		}
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
	case ERenderPass::Translucent:return EDepthStencilState::Default;
	case ERenderPass::StencilMask: return EDepthStencilState::StencilWrite;
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
	case ERenderPass::Translucent: return EBlendState::AlphaBlend;
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
#if STATS
	FGPUProfiler::Get().EndFrame();
#endif
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
			EDepthStencilState TargetDepth = (Cmd.DepthStencilState != static_cast<EDepthStencilState>(-1))
				? Cmd.DepthStencilState
				: GetDefaultDepthForPass(CurPass);

			EBlendState TargetBlend = (Cmd.BlendState != static_cast<EBlendState>(-1))
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
	const FVector CameraPosition = RenderBus.GetView().GetInverseFast().GetLocation();
	FVector CameraForward = RenderBus.GetCameraRight().Cross(RenderBus.GetCameraUp());
	CameraForward.Normalize();

	FEditorConstants EditorConstants = {};
	EditorConstants.CameraPosition = CameraPosition;
	Resources.EditorConstantBuffer.Update(Context, &EditorConstants, sizeof(FEditorConstants));

	CollectBatchingData(RenderBus);

	SetupRenderState(ERenderPass::Editor, Context, RenderBus.GetViewMode());

	ID3D11Buffer* cb = Resources.EditorConstantBuffer.GetBuffer();
	Context->VSSetConstantBuffers(4, 1, &cb);
	Context->PSSetConstantBuffers(4, 1, &cb);

	LineBatcher.AddWorldHelpers(FEditorSettings::Get(), CameraPosition, CameraForward);
	LineBatcher.Flush(Context);

	// --- SubUVBatcher Flush ---
	// 단일 아틀라스 기준: 커맨드에서 첫 번째 유효한 SRV를 사용
	const auto& SubUVCmds = RenderBus.GetCommands(ERenderPass::SubUV);
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

	// Editor helper에서 사용한 알파 블렌드가 다음 렌더 경로에 남지 않도록 기본 상태로 복원한다.
	Device.SetDepthStencilState(EDepthStencilState::Default);
	Device.SetBlendState(EBlendState::Opaque);
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, bool bIsWireframe)
{
	FFrameConstants frameConstantData;
	frameConstantData.View = ViewMatrix;
	frameConstantData.Projection = ProjMatrix;
	frameConstantData.bIsWireframe = bIsWireframe;
	frameConstantData.ColorRGB = FVector(0.0f, 0.0f, 0.7f);

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(0, 1, &b0);
	Context->PSSetConstantBuffers(0, 1, &b0);
}

void FRenderer::CollectBatchingData(const FRenderBus& RenderBus)
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
}
