#include "Renderer.h"

#include <iostream>
#include <algorithm>
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Mesh/MeshManager.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Core/ResourceManager.h"

void FRenderer::Create(HWND hWindow)
{
	Device.Create(hWindow);

	if (Device.GetDevice() == nullptr)
	{
		std::cout << "Failed to create D3D Device." << std::endl;
	}

	FResourceManager::Get().SetCachedDevice(Device.GetDevice());
	FResourceManager::Get().LoadShader("Shaders/Primitive.hlsl", "VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));
	FResourceManager::Get().LoadShader("Shaders/ShaderSubUV.hlsl", "VS", "PS", FontBatcherInputLayout, ARRAYSIZE(FontBatcherInputLayout));
	FResourceManager::Get().LoadShader("Shaders/Gizmo.hlsl", "VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));
	FResourceManager::Get().LoadShader("Shaders/Editor.hlsl", "VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));
	FResourceManager::Get().LoadShader("Shaders/SelectionMask.hlsl", "VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));
	FResourceManager::Get().LoadShader("Shaders/OutlinePostProcess.hlsl", "VS", "PS", nullptr, 0);
	FResourceManager::Get().LoadShader("Shaders/ShaderStaticMesh.hlsl", "mainVS", "mainPS", NormalVertexInputLayout, ARRAYSIZE(NormalVertexInputLayout));
	FResourceManager::Get().LoadShader("Shaders/Multipass/LightPass.hlsl", "mainVS", "mainPS", nullptr, 0);
	FResourceManager::Get().LoadShader("Shaders/ShaderDecal.hlsl", "mainVS", "mainPS", NormalVertexInputLayout, ARRAYSIZE(NormalVertexInputLayout));
	FResourceManager::Get().LoadShader("Shaders/Multipass/FogPass.hlsl", "mainVS", "mainPS", nullptr, 0);
	FResourceManager::Get().LoadShader("Shaders/Multipass/FXAAPass.hlsl", "mainVS", "mainPS", nullptr, 0);
	FResourceManager::Get().LoadShader("Shaders/ShaderFont.hlsl", "VS", "PS", FontBatcherInputLayout, ARRAYSIZE(FontBatcherInputLayout));
	FResourceManager::Get().LoadShader("Shaders/ShaderLine.hlsl", "mainVS", "mainPS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));
}

void FRenderer::CreateResources()
{
	Resources.PerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FPerObjectConstants));
	Resources.FrameBuffer.Create(Device.GetDevice(), sizeof(FFrameConstants));
	Resources.FogPassConstantBuffer.Create(Device.GetDevice(), sizeof(FFogPassConstants));
	Resources.FXAAConstantBuffer.Create(Device.GetDevice(), sizeof(FFXAAConstants));
	Resources.LightPassConstantBuffer.Create(Device.GetDevice(), sizeof(FLightPassConstants));
	Resources.LightStructuredBuffer.Create(Device.GetDevice(), sizeof(FLightData), 256);

	//	MeshManager init
	FMeshManager::Initialize();

	EditorLineBatcher.Create(Device.GetDevice());
	GridLineBatcher.Create(Device.GetDevice());

	// 텍스처는 ResourceManager가 소유 — Batcher 는 셰이더/버퍼만 초기화
	FontBatcher.Create(Device.GetDevice());
	SubUVBatcher.Create(Device.GetDevice());

	InitializePassRenderStates();
	InitializePassBatchers();
	UseBackBufferRenderTargets();

	// GPU Profiler 초기화
	FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());

	RenderPipeline.Initialize();
    RenderPassContext = std::make_shared<FRenderPassContext>();
}

void FRenderer::Release()
{
	InvalidateSceneFinalTargets();

	RenderPipeline.Release();
    RenderPassContext.reset();

	Resources.PerObjectConstantBuffer.Release();
	Resources.FrameBuffer.Release();
    Resources.FogPassConstantBuffer.Release();
    Resources.FXAAConstantBuffer.Release();
    Resources.LightPassConstantBuffer.Release();

	FGPUProfiler::Get().Shutdown();

	EditorLineBatcher.Release();
	GridLineBatcher.Release();
	FontBatcher.Release();
	SubUVBatcher.Release();

    // Device::ReportLiveObjects 이전에 ResourceManager가 잡고 있던 D3D 객체를 먼저 해제한다.
    FResourceManager::Get().ReleaseGPUResources();

	Device.Release();
}

//	Bus → Batcher 데이터 수집 (CPU). BeginFrame 이전에 호출.
void FRenderer::PrepareBatchers(const FRenderBus& InRenderBus)
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		if (!PassBatchers[i]) continue;

		const auto& Commands = InRenderBus.GetCommands(static_cast<ERenderPass>(i));
		const auto& AlignedCommands = GetAlignedCommands(static_cast<ERenderPass>(i), Commands);

		PassBatchers[i].Clear();
		for (const auto& Cmd : AlignedCommands)
			PassBatchers[i].Collect(Cmd, InRenderBus);
	}
}

const TArray<FRenderCommand>& FRenderer::GetAlignedCommands(ERenderPass Pass, const TArray<FRenderCommand>& Commands)
{
	// SubUV 패스: Particle(SRV) 포인터 기준 정렬 → 같은 텍스쳐끼리 연속 배치
	if (Pass == ERenderPass::SubUV && Commands.size() > 1)
	{
		SortedCommandBuffer.assign(Commands.begin(), Commands.end());

		std::sort(SortedCommandBuffer.begin(), SortedCommandBuffer.end(),
			[](const FRenderCommand& A, const FRenderCommand& B) {
				return A.Constants.SubUV.Particle < B.Constants.SubUV.Particle;
			});

		return SortedCommandBuffer;
	}

	return Commands;
}

//	GPU 프레임 시작. 반드시 Render 이전에 호출되어야 함.
void FRenderer::BeginFrame()
{
	Device.BeginFrame();
	UseBackBufferRenderTargets();

#if STATS
	FGPUProfiler::Get().BeginFrame();
#endif
}

void FRenderer::UseBackBufferRenderTargets()
{
	CurrentRenderTargets = Device.GetBackBufferRenderTargets();

	if (CurrentRenderTargets.IsValid())
	{
        ID3D11RenderTargetView* RTV =
                CurrentRenderTargets.SceneColorRTV; // Back Buffer 의 경우 SceneColorRTV 가 FinalRTV 역할
        SceneFinalRTV = RTV;
        
		Device.GetDeviceContext()->OMSetRenderTargets(1, &RTV, CurrentRenderTargets.DepthStencilView);
		Device.SetSubViewport(0, 0,
			static_cast<int32>(CurrentRenderTargets.Width),
			static_cast<int32>(CurrentRenderTargets.Height));
	}
}

void FRenderer::UseViewportRenderTargets()
{
	CurrentRenderTargets = Device.GetViewportRenderTargets();
	if (!CurrentRenderTargets.IsValid())
	{
		InvalidateSceneFinalTargets();
		UseBackBufferRenderTargets();
		return;
	}

	Device.SetSubViewport(0, 0,
		static_cast<int32>(CurrentRenderTargets.Width),
		static_cast<int32>(CurrentRenderTargets.Height));
}

void FRenderer::InvalidateSceneFinalTargets()
{
	SceneFinalRTV.Reset();
	SceneFinalSRV.Reset();
	CurrentRenderTargets = {};
}

//	RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행 (GPU)
void FRenderer::Render(const FRenderBus& InRenderBus)
{
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	UpdateFrameBuffer(Context, InRenderBus);

	/** Opaque 만 테스트 */
    
	RenderPassContext->Device = Device.GetDevice();
    RenderPassContext->DeviceContext = Device.GetDeviceContext();
    RenderPassContext->RenderState = &PassRenderStates[(uint32)ERenderPass::Opaque];
    RenderPassContext->RenderBus = &InRenderBus;
    RenderPassContext->RenderTargets = &CurrentRenderTargets;
    RenderPassContext->RenderResources = &Resources;
    RenderPassContext->FontBatcher = &FontBatcher;
    RenderPassContext->SubUVBatcher = &SubUVBatcher;
    RenderPassContext->GridLineBatcher = &GridLineBatcher;
    RenderPassContext->EditorLineBatcher = &EditorLineBatcher;
	RenderPipeline.Render(RenderPassContext.get());
	
	SceneFinalSRV = RenderPipeline.GetOutSRV();
}

// ============================================================
// 패스별 기본 렌더 상태 테이블 초기화
// ============================================================
void FRenderer::InitializePassRenderStates()
{
	using E = ERenderPass;
	auto& S = PassRenderStates;

	UShader* PrimitiveShader = FResourceManager::Get().GetShader("Shaders/Primitive.hlsl");
	UShader* DecalShader = FResourceManager::Get().GetShader("Shaders/ShaderDecal.hlsl");
	UShader* LightPassShader = FResourceManager::Get().GetShader("Shaders/Multipass/LightPass.hlsl");
	UShader* FogPassShader = FResourceManager::Get().GetShader("Shaders/Multipass/FogPass.hlsl");
	UShader* FXAAShader = FResourceManager::Get().GetShader("Shaders/Multipass/FXAAPass.hlsl");
	UShader* SelectionMaskShader = FResourceManager::Get().GetShader("Shaders/SelectionMask.hlsl");
	UShader* EditorShader = FResourceManager::Get().GetShader("Shaders/Editor.hlsl");
	UShader* GizmoShader = FResourceManager::Get().GetShader("Shaders/Gizmo.hlsl");
	UShader* OutlineShader = FResourceManager::Get().GetShader("Shaders/OutlinePostProcess.hlsl");

	S[(uint32)E::Opaque] = { PrimitiveShader, false };
	S[(uint32)E::Light] = { LightPassShader, false};	
	S[(uint32)E::Translucent] = { PrimitiveShader, false };
	S[(uint32)E::Fog] = { FogPassShader, false};
    S[(uint32)E::FXAA] = { FXAAShader, false};
	S[(uint32)E::SelectionMask] = { SelectionMaskShader, false };
	S[(uint32)E::Editor] = { EditorShader, true };
	S[(uint32)E::Grid] = { EditorShader, false };
	S[(uint32)E::DepthLess] = { GizmoShader, false };
	S[(uint32)E::Font] = { nullptr, true };
	S[(uint32)E::SubUV] = { nullptr, true };
    S[(uint32)E::PostProcessOutline] = { OutlineShader, false};

}

// ============================================================
// Pass Batcher 바인딩 초기화
// ============================================================
void FRenderer::InitializePassBatchers()
{
	// --- Editor 패스: AABB 디버그 박스 → EditorLineBatcher ---
	PassBatchers[(uint32)ERenderPass::Editor] = {
		/*.Clear   =*/ [this]() { EditorLineBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus&) {
			if (Cmd.Type == ERenderCommandType::DebugBox)
			{
				EditorLineBatcher.AddAABB(FBoundingBox{ Cmd.Constants.AABB.Min, Cmd.Constants.AABB.Max }, Cmd.Constants.AABB.Color);
			}
			else if (Cmd.Type == ERenderCommandType::DebugOBB)
			{
				EditorLineBatcher.AddOBB(FOBB{ Cmd.Constants.OBB.Center, Cmd.Constants.OBB.Extents, Cmd.Constants.OBB.Rotation }, Cmd.Constants.OBB.Color);
			}
			else if (Cmd.Type == ERenderCommandType::DebugSpotlight)
			{
				const auto& S = Cmd.Constants.SpotLight;
				EditorLineBatcher.AddSpotLight(S.Position, S.Direction, S.Range, S.InnerAngle, S.OuterAngle, S.Color);
			}
		},
		/*.Flush   =*/ [this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			FlushLineBatcher(EditorLineBatcher, Pass, Bus, Ctx);
		}
	};

	// --- Grid 패스: 월드 그리드 + 축 → GridLineBatcher ---
	PassBatchers[(uint32)ERenderPass::Grid] = {
		/*.Clear   =*/ [this]() { GridLineBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::Grid)
			{
				const FVector CameraPos = Bus.GetView().GetInverse().GetOrigin();
				const FVector CameraFwd = Bus.GetCameraForward();

				GridLineBatcher.AddWorldHelpers(
					Bus.GetShowFlags(),
					Cmd.Constants.Grid.GridSpacing,
					Cmd.Constants.Grid.GridHalfLineCount,
					CameraPos, CameraFwd,
					Cmd.Constants.Grid.bOrthographic);
			}
		},
		/*.Flush   =*/ [this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			FlushLineBatcher(GridLineBatcher, Pass, Bus, Ctx);
		}
	};

	// --- Font 패스: 텍스트 → FontBatcher ---
	PassBatchers[(uint32)ERenderPass::Font] = {
		/*.Clear   =*/ [this]() { FontBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::Font && Cmd.Constants.Font.Text && !Cmd.Constants.Font.Text->empty())
			{
				FontBatcher.AddText(
					*Cmd.Constants.Font.Text,
					Cmd.PerObjectConstants.Model,
					Cmd.Constants.Font.Scale
				);
			}
		},
		/*.Flush   =*/ [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.Flush(Ctx, FontRes);
		}
	};

	// --- SubUV 패스: 스프라이트 → SubUVBatcher ---
	// Collect 시 첫 번째 유효한 SRV를 캡처하여 Flush에서 재순회 방지
	PassBatchers[(uint32)ERenderPass::SubUV] = {
		/*.Clear   =*/ [this]() {
			SubUVBatcher.Clear();
			SubUVCachedTexture = nullptr;
		},
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::SubUV && Cmd.Constants.SubUV.Particle)
			{
				const auto& SubUV = Cmd.Constants.SubUV;
				if (!SubUVCachedTexture && SubUV.Particle->IsLoaded())
				{
					SubUVCachedTexture = SubUV.Particle->Texture;
				}

				SubUVBatcher.AddSprite(
					SubUV.Particle->Texture,
					Cmd.PerObjectConstants.Model.GetOrigin(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Cmd.PerObjectConstants.Model.GetScaleVector(),
					SubUV.FrameIndex,
					SubUV.Particle->Columns,
					SubUV.Particle->Rows,
					SubUV.Width,
					SubUV.Height
				);
			}
			// 기존 SubUV 분기 아래에
			else if (Cmd.Type == ERenderCommandType::Billboard && Cmd.Constants.Billboard.Texture)
			{
				SubUVBatcher.AddSprite(
					Cmd.Constants.Billboard.Texture,
					Cmd.PerObjectConstants.Model.GetOrigin(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Cmd.PerObjectConstants.Model.GetScaleVector(),
					0,   // FrameIndex 고정
					1,   // Columns 고정
					1,   // Rows 고정
					Cmd.Constants.Billboard.Width,
					Cmd.Constants.Billboard.Height
				);
			}
		},
		/*.Flush   =*/ [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			SubUVBatcher.Flush(Ctx);
		}
	};
}

// ============================================================
// LineBatcher Flush 공통
// ============================================================
void FRenderer::FlushLineBatcher(FLineBatcher& Batcher, ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
	if (Batcher.GetLineCount() == 0) return;

	ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

	Batcher.Flush(Context);
}

// ============================================================
// 기본 패스 실행기
// ============================================================
void FRenderer::ExecuteDefaultPass(ERenderPass Pass, const TArray<FRenderCommand>& Commands, const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
	ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

	const FPassRenderState& State = PassRenderStates[(uint32)Pass];
	ERenderCommandType LastCommandType = static_cast<ERenderCommandType>(-1);
	for (const auto& Cmd : Commands)
	{
		BindShaderByType(Cmd, Context, LastCommandType);
		if (Cmd.Type == ERenderCommandType::PostProcessOutline)
		{
			DrawPostProcessOutline(Context);
			continue;
		}
		DrawCommand(Context, Cmd);
	}
}

void FRenderer::ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context, EViewMode CurViewMode)
{
    ID3D11RenderTargetView* RTVs[MaxRTVCount] = {nullptr, nullptr};
    ID3D11DepthStencilView* DSV = nullptr;

	/** Pass 별 RTV 설정 */
	switch (Pass)
	{
		/**
		* TODO: Final 로 쓰이는 경로가 Light, Fog 만 있어서 현재는 해당 패스들만 Final 기록 (추후 확장 필요)
		* 
		*/
        case ERenderPass::Opaque:
		case ERenderPass::Decal:
            RTVs[0] = CurrentRenderTargets.SceneColorRTV;
            RTVs[1] = CurrentRenderTargets.SceneNormalRTV;
            RTVs[2] = CurrentRenderTargets.SceneWorldPosRTV;
            break;
        case ERenderPass::Light:
			RTVs[0] = CurrentRenderTargets.SceneLightRTV;
            SceneFinalRTV = CurrentRenderTargets.SceneLightRTV;
            SceneFinalSRV = CurrentRenderTargets.SceneLightSRV;
            break;
        case ERenderPass::Fog:
            RTVs[0] = CurrentRenderTargets.SceneFogRTV;
            SceneFinalRTV = CurrentRenderTargets.SceneFogRTV;
            SceneFinalSRV = CurrentRenderTargets.SceneFogSRV;
            break;
        case ERenderPass::SelectionMask:
            RTVs[0] = CurrentRenderTargets.SelectionMaskRTV;
            break;
        case ERenderPass::FXAA:
            RTVs[0] = CurrentRenderTargets.SceneFXAARTV; // FXAA 결과도 최종 출력이므로 SceneFinalRTV 사용
            SceneFinalRTV = CurrentRenderTargets.SceneFXAARTV;
            SceneFinalSRV = CurrentRenderTargets.SceneFXAASRV;
            break;
        default:
			// 나머지 Pass (UI, ...) 들은 하나의 RTV 에 그린다 가정
            RTVs[0] = SceneFinalRTV.Get();
            break;
	}

	/** Pass 별 DSV 설정 */
	switch (Pass)
	{
        case ERenderPass::Light:
            DSV = nullptr;
			break;
        case ERenderPass::Fog:
            DSV = nullptr;
            break;
        case ERenderPass::FXAA:
			DSV = nullptr;
            break;
        default:
            DSV = CurrentRenderTargets.DepthStencilView;
            break;
	}

	Context->OMSetRenderTargets(MaxRTVCount, RTVs, DSV);

	const FPassRenderState& State = PassRenderStates[(uint32)Pass];

	ERasterizerType Rasterizer = ERasterizerType::SolidBackCull;
	if (State.bWireframeAware && CurViewMode == EViewMode::Wireframe)
	{
		Rasterizer = ERasterizerType::WireFrame;
	}

	//Device.SetDepthStencilState(State.DepthStencil);
	//Device.SetBlendState(State.Blend);
	//Device.SetRasterizerState(Rasterizer);
	Context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (State.Shader)
	{
		State.Shader->Bind(Context);
	}
}

void FRenderer::BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context, ERenderCommandType& LastCommandType)
{
    bool bTypeChanged = (LastCommandType != InCmd.Type);

    // 객체별 Transform Data는 항상 업데이트해야 한다.
    Resources.PerObjectConstantBuffer.Update(Context, &InCmd.PerObjectConstants, sizeof(FPerObjectConstants));
	ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
	Context->VSSetConstantBuffers(1, 1, &cb1);
	Context->PSSetConstantBuffers(1, 1, &cb1);

	// 데이터 Update는 항상 수행하지만, 셰이더/상수 버퍼 바인딩은 타입이 변경된 경우에만 수행
    switch (InCmd.Type)
    {
	case ERenderCommandType::PostProcessOutline:
	{
		UMaterial* OutlineMaterial = Cast<UMaterial>(InCmd.Material);
		InCmd.Material->Bind(Context);
		break;
	}
	}

    LastCommandType = InCmd.Type;
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

	if (InCommand.Material)
	{
		InCommand.Material->Bind(InDeviceContext);
	}

	InDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetIndexBuffer().GetBuffer();
	if (indexBuffer != nullptr)
	{
		uint32 indexStart = InCommand.SectionIndexStart;
		uint32 indexCount = InCommand.SectionIndexCount;
		InDeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		InDeviceContext->DrawIndexed(indexCount, indexStart, 0);
	}
	else
	{
		InDeviceContext->Draw(vertexCount, 0);
	}
}

void FRenderer::DrawPostProcessOutline(ID3D11DeviceContext* InDeviceContext)
{
	ID3D11RenderTargetView* RTV = SceneFinalRTV.Get();
	InDeviceContext->OMSetRenderTargets(1, &RTV, nullptr);
	InDeviceContext->OMSetDepthStencilState(nullptr, 0);

	ID3D11ShaderResourceView* maskSRV = CurrentRenderTargets.SelectionMaskSRV;
	InDeviceContext->PSSetShaderResources(7, 1, &maskSRV);

	auto DepthStencilState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::Default);
	auto BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
	auto RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(ERasterizerType::SolidBackCull);

	InDeviceContext->OMSetDepthStencilState(DepthStencilState, 0);
	InDeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
	InDeviceContext->RSSetState(RasterizerState);

	InDeviceContext->Draw(3, 0);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	InDeviceContext->PSSetShaderResources(7, 1, &nullSRV);
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
#if STATS
	FGPUProfiler::Get().EndFrame();
#endif
	Device.EndFrame();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
	FFrameConstants frameConstantData;
	frameConstantData.View = InRenderBus.GetView();
	frameConstantData.Projection = InRenderBus.GetProj();
	frameConstantData.CameraPosition = InRenderBus.GetCameraPosition();
	frameConstantData.bIsWireframe = (InRenderBus.GetViewMode() == EViewMode::Wireframe);
	frameConstantData.WireframeColor = InRenderBus.GetWireframeColor();

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(0, 1, &b0);
	Context->PSSetConstantBuffers(0, 1, &b0);
}
