#include "Renderer.h"

#include "Render/Common/RenderTypes.h"

#if DEBUG

#include <iostream>
#include <cassert>

#endif

#include "World/Mesh/MeshManager.h"

void FRenderer::Create(HWND hWindow)
{
	Device.Create(hWindow);

	if (Device.GetDevice() == nullptr)
	{
		std::cout << "Failed to create D3D Device." << std::endl;
	}

	Resources.PrimitiveShader.Create(Device.GetDevice(), ShaderFilePath,
		"PrimitiveVS", "PrimitivePS",PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));
	Resources.GizmoShader.Create(Device.GetDevice(), ShaderFilePath,
		"GizmoVS", "GizmoPS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));
	Resources.OverlayShader.Create(Device.GetDevice(), ShaderFilePath,
		"OverlayVS", "OverlayPS", OverlayInputLayout, ARRAYSIZE(OverlayInputLayout));
	Resources.EditorShader.Create(Device.GetDevice(), ShaderFilePath,
		"EditorVS", "EditorPS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));
	Resources.OutlineShader.Create(Device.GetDevice(), ShaderFilePath,
		"OutlineVS", "OutlinePS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	Resources.PerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FTransformConstants));
	Resources.GizmoPerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FGizmoConstants));
	Resources.OverlayConstantBuffer.Create(Device.GetDevice(), sizeof(FOverlayConstants));
	Resources.EditorConstantBuffer.Create(Device.GetDevice(), sizeof(FEditorConstants));
	Resources.OutlineConstantBuffer.Create(Device.GetDevice(), sizeof(FOutlineConstants));

	//	MeshManager init
	FMeshManager::Initialize();
}

void FRenderer::Release()
{
	Resources.PrimitiveShader.Release();
	Resources.GizmoShader.Release();
	Resources.OverlayShader.Release();
	Resources.EditorShader.Release();
	Resources.OutlineShader.Release();

	Resources.PerObjectConstantBuffer.Release();
	Resources.GizmoPerObjectConstantBuffer.Release();
	Resources.OverlayConstantBuffer.Release();
	Resources.EditorConstantBuffer.Release();
	Resources.OutlineConstantBuffer.Release();

	Device.Release();
}

//	Prepare the rendering state for a new frame. 반드시 Render 이전에 호출되어야 함.
void FRenderer::BeginFrame()
{
	Device.BeginFrame();
}

//	Render Update Main function. RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행
void FRenderer::Render(const FRenderBus& InRenderBus)
{
	ID3D11DeviceContext* context = Device.GetDeviceContext();

	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurrentPass = static_cast<ERenderPass>(i);
		const auto& Commands = InRenderBus.GetCommands(CurrentPass);

		if (Commands.empty()) continue;


		SetupRenderState(CurrentPass, context);

		for (const auto& Cmd : Commands)
		{
			DrawCommand(context, Cmd);
		}
	}


	//RenderComponentPass(context, InRenderBus);

	//RenderEditorPass(context, InRenderBus);

	////	Grid (AlphaBlend)
	////	Grid should not overwrite depth, otherwise gizmo behind z=0 gets culled.
	//
	//RenderGridEditorPass(context, InRenderBus);

	////	Selection Outline (Stencil)

	//RenderOutlinePass(context, InRenderBus);

	//// Gizmo (Depth-less Variable)
	//Device.SetBlendState(EBlendState::Opaque);
	//RenderDepthLessPass(context, InRenderBus);

	////	Reset to default
	//Device.SetRasterizerState(ERasterizerState::SolidBackCull);

	//	NOTE : Overlay는 반드시 따로 호출해야 함. (Engine Loop에서 돌고 있음)
}

void FRenderer::RenderOverlay(const FRenderBus& InRenderBus)
{
	ID3D11DeviceContext* context = Device.GetDeviceContext();



	//RenderOverlayPass(context, InRenderBus);
}

void FRenderer::SetupRenderState(ERenderPass Pass, ID3D11DeviceContext* DeviceContext)
{
	switch (Pass)
	{
	case ERenderPass::Component:
		//	Primitive
		Device.SetDepthStencilState(EDepthStencilState::StencilWrite);
		Device.SetBlendState(EBlendState::Opaque);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Resources.PrimitiveShader.Bind(DeviceContext);
		break;
	case ERenderPass::Outline:
		Device.SetDepthStencilState(EDepthStencilState::StencilOutline);
		Device.SetRasterizerState(ERasterizerState::SolidFrontCull);
		Device.SetBlendState(EBlendState::Opaque);
		Resources.OutlineShader.Bind(DeviceContext);
		break;
	case ERenderPass::DepthLess:
		Device.SetDepthStencilState(EDepthStencilState::None);
		Device.SetBlendState(EBlendState::AlphaBlend);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Resources.GizmoShader.Bind(DeviceContext);
		break;

	case ERenderPass::Editor:
		//	Axis (LINELIST)
		Device.SetDepthStencilState(EDepthStencilState::Default);
		Device.SetRasterizerState(ERasterizerState::SolidBackCull);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		Resources.EditorShader.Bind(DeviceContext);
		break;

	case ERenderPass::Grid:
		Device.SetDepthStencilState(EDepthStencilState::DepthReadOnly);
		Device.SetBlendState(EBlendState::AlphaBlend);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Resources.EditorShader.Bind(DeviceContext);
		break;
	case ERenderPass::Overlay:
		Device.SetDepthStencilState(EDepthStencilState::None);
		Device.SetBlendState(EBlendState::Opaque);
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Resources.OverlayShader.Bind(DeviceContext);
		break;
	}
}

//void FRenderer::RenderComponentPass(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus)
//{
//	for (const FRenderCommand& command : InRenderBus.GetCommands(ERenderPass::Component))
//	{
//		DrawCommand(InDeviceContext, command);
//	}
//}

//void FRenderer::RenderDepthLessPass(ID3D11DeviceContext* InDeviceContext, const FRenderBus& InRenderBus)
//{
//	if (InRenderBus.GetDepthLessCommands().size() == 0) return;
//	//	DepthLess (Gizmo)
//
//	DrawCommand(InDeviceContext, InRenderBus.GetDepthLessCommands()[0]);
	//RenderDepthLessPass(InDeviceContext, InRenderBus);


	//	���õ� ��� �׸��� ����
	//if (InRenderBus.GetDepthLessCommands().size() < 2) return;

	////	Non-DepthLess (Gizmo)
	//Device.SetDepthStencilState(EDepthStencilState::Default);
	//Device.SetBlendState(EBlendState::Opaque);
	//Resources.GizmoShader.Bind(InDeviceContext);

	//InDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//DrawCommand(InDeviceContext, InRenderBus.GetDepthLessCommands()[1]);

	//RenderDepthLessPass(InDeviceContext, InRenderBus);
	//for (const FRenderCommand& command : InRenderBus.GetDepthLessCommands())
	//{
	//	DrawCommand(InDeviceContext, command);
	//}
//}

void FRenderer::DrawCommand(ID3D11DeviceContext * InDeviceContext, const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr || !InCommand.MeshBuffer->IsValid())
	{
		return;
	}

	if (InCommand.Type != ERenderCommandType::Overlay)
	{
		Resources.PerObjectConstantBuffer.Update(InDeviceContext, &InCommand.TransformConstants, sizeof(FTransformConstants));
		
		ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
		InDeviceContext->VSSetConstantBuffers(0, 1, &cb);
		//InDeviceContext->PSSetConstantBuffers(0, 1, &cb);
	}

	if (InCommand.Type == ERenderCommandType::Gizmo)
	{
		Resources.GizmoPerObjectConstantBuffer.Update(InDeviceContext, &InCommand.GizmoConstants, sizeof(FGizmoConstants));

		ID3D11Buffer* cb = Resources.PerObjectConstantBuffer.GetBuffer();
		InDeviceContext->VSSetConstantBuffers(0, 1, &cb);

		cb = Resources.GizmoPerObjectConstantBuffer.GetBuffer();
		InDeviceContext->VSSetConstantBuffers(1, 1, &cb);
		InDeviceContext->PSSetConstantBuffers(1, 1, &cb);
	}
	else if (InCommand.Type == ERenderCommandType::Overlay)
	{
		Resources.OverlayConstantBuffer.Update(InDeviceContext, &InCommand.OverlayConstants, sizeof(FOverlayConstants));

		ID3D11Buffer* cb = Resources.OverlayConstantBuffer.GetBuffer();
		InDeviceContext->VSSetConstantBuffers(2, 1, &cb);
		InDeviceContext->PSSetConstantBuffers(2, 1, &cb);
	}
	else if (InCommand.Type == ERenderCommandType::Axis || InCommand.Type == ERenderCommandType::Grid)
	{
		Resources.EditorConstantBuffer.Update(InDeviceContext, &InCommand.EditorConstants, sizeof(FEditorConstants));

		ID3D11Buffer* cb = Resources.EditorConstantBuffer.GetBuffer();
		InDeviceContext->VSSetConstantBuffers(3, 1, &cb);
		InDeviceContext->PSSetConstantBuffers(3, 1, &cb);

		cb = Resources.PerObjectConstantBuffer.GetBuffer();
		InDeviceContext->VSSetConstantBuffers(0, 1, &cb);
		InDeviceContext->PSSetConstantBuffers(0, 1, &cb);
	}
	else if(InCommand.Type == ERenderCommandType::SelectionOutline)
	{
		Resources.OutlineConstantBuffer.Update(InDeviceContext, &InCommand.OutlineConstants, sizeof(FOutlineConstants));

		ID3D11Buffer* cb = Resources.OutlineConstantBuffer.GetBuffer();
		InDeviceContext->VSSetConstantBuffers(4, 1, &cb);
		InDeviceContext->PSSetConstantBuffers(4, 1, &cb);

		cb = Resources.PerObjectConstantBuffer.GetBuffer();
		InDeviceContext->VSSetConstantBuffers(0, 1, &cb);
		//InDeviceContext->PSSetConstantBuffers(0, 1, &cb);
	}

	uint32 offset = 0;
	ID3D11Buffer* vertexBuffer = InCommand.MeshBuffer->GetFVertexBuffer().GetBuffer();
	if (vertexBuffer == nullptr)
	{
		return;
	}

	uint32 vertexCount = InCommand.MeshBuffer->GetFVertexBuffer().GetVertexCount();
	uint32 stride = InCommand.MeshBuffer->GetFVertexBuffer().GetStride();
	if (vertexCount == 0 || stride == 0)
	{
		return;
	}

	InDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetFIndexBuffer().GetBuffer();
	if (indexBuffer != nullptr)
	{
		uint32 indexCount = InCommand.MeshBuffer->GetFIndexBuffer().GetIndexCount();
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