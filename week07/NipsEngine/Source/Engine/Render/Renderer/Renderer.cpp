#include "Renderer.h"
#include "Renderer.h"

#include <iostream>
#include <algorithm>
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Mesh/MeshManager.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"

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

	// 3. 에디터/라인 (Editor.hlsl)
	Resources.EditorShader.Create(Device.GetDevice(), L"Shaders/Editor.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 4. 선택 마스크 (SelectionMask.hlsl)
	Resources.SelectionMaskShader.Create(Device.GetDevice(), L"Shaders/SelectionMask.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 5. 포스트 프로세스 아웃라인 (OutlinePostProcess.hlsl)
	Resources.OutlineShader.Create(Device.GetDevice(), L"Shaders/OutlinePostProcess.hlsl",
		"VS", "PS", nullptr, 0);

	// 6. 스태틱 메시 (ShaderStaticMesh.hlsl)
	Resources.StaticMeshShader.Create(Device.GetDevice(), L"Shaders/ShaderStaticMesh.hlsl",
		"mainVS", "mainPS", NormalVertexInputLayout, ARRAYSIZE(NormalVertexInputLayout));

	// 7. Light Pass (LightPass.hlsl)
    Resources.LightPassShader.Create(Device.GetDevice(), L"Shaders/Multipass/LightPass.hlsl", "mainVS", "mainPS",
                                        nullptr, 0);

	// 8. Decal
	Resources.DecalShader.Create(Device.GetDevice(), L"Shaders/ShaderDecal.hlsl", "mainVS", "mainPS", NormalVertexInputLayout, ARRAYSIZE(NormalVertexInputLayout));

	// 9. Fog Pass
    Resources.FogPassShader.Create(Device.GetDevice(), L"Shaders/Multipass/FogPass.hlsl", "mainVS", "mainPS",
                                     nullptr, 0);

	// 10. FXAA Pass
	Resources.FXAAShader.Create(Device.GetDevice(), L"Shaders/Multipass/FXAAPass.hlsl", "mainVS", "mainPS", nullptr, 0);

	Resources.PerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FPerObjectConstants));
	Resources.FrameBuffer.Create(Device.GetDevice(), sizeof(FFrameConstants));
	Resources.GizmoPerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FGizmoConstants));
	Resources.EditorConstantBuffer.Create(Device.GetDevice(), sizeof(FEditorConstants));
	Resources.OutlineConstantBuffer.Create(Device.GetDevice(), sizeof(FOutlineConstants));
	Resources.StaticMeshConstantBuffer.Create(Device.GetDevice(), sizeof(FStaticMeshConstants));
	Resources.DecalConstantBuffer.Create(Device.GetDevice(), sizeof(FDecalConstants));
    Resources.FogPassConstantBuffer.Create(Device.GetDevice(), sizeof(FFogConstants));
    Resources.FXAAConstantBuffer.Create(Device.GetDevice(), sizeof(FFXAAConstants));

	// TODO : SamplerState 관리
	D3D11_SAMPLER_DESC SampDesc = {};
	SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	Device.GetDevice()->CreateSamplerState(&SampDesc, Resources.MeshSamplerState.ReleaseAndGetAddressOf());

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
}

void FRenderer::Release()
{
	Resources.PrimitiveShader.Release();
	Resources.GizmoShader.Release();
	Resources.EditorShader.Release();
	Resources.SelectionMaskShader.Release();
	Resources.OutlineShader.Release();
    Resources.StaticMeshShader.Release();
    Resources.FogPassShader.Release();
    Resources.DecalShader.Release();
    Resources.FXAAShader.Release();
    Resources.LightPassShader.Release();

	Resources.PerObjectConstantBuffer.Release();
	Resources.FrameBuffer.Release();
	Resources.GizmoPerObjectConstantBuffer.Release();
	Resources.EditorConstantBuffer.Release();
	Resources.OutlineConstantBuffer.Release();
	Resources.StaticMeshConstantBuffer.Release();
    Resources.FogPassConstantBuffer.Release();
	Resources.DecalConstantBuffer.Release();
    Resources.FXAAConstantBuffer.Release();
    Resources.LightPassConstantBuffer.Release();

	Resources.MeshSamplerState.Reset();

	FGPUProfiler::Get().Shutdown();

	EditorLineBatcher.Release();
	GridLineBatcher.Release();
	FontBatcher.Release();
	SubUVBatcher.Release();

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
		UseBackBufferRenderTargets();
		return;
	}

	Device.SetSubViewport(0, 0,
		static_cast<int32>(CurrentRenderTargets.Width),
		static_cast<int32>(CurrentRenderTargets.Height));
}

//	RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행 (GPU)
void FRenderer::Render(const FRenderBus& InRenderBus)
{
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	UpdateFrameBuffer(Context, InRenderBus);

	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);

		/** TODO: if 문 처리는 아쉬움. 나중에 확장성을 위해 수정 필요 */
		if (CurPass == ERenderPass::Light)
		{
			// Command 로 따로 넣어주지 않아도 무조건 실행되어야하는 Pass
			ExecuteLightPass(InRenderBus, Context);
		}
        else if (CurPass == ERenderPass::Fog)
		{
            const auto& Commands = InRenderBus.GetCommands(CurPass);
            if (Commands.empty())
                continue;
			else
            {
                ExecuteFogPass(Commands, InRenderBus, Context);
			}
        }
		else if (CurPass == ERenderPass::FXAA)
		{
            // Command 로 따로 넣어주지 않아도 무조건 실행되어야하는 Pass
            ExecuteFXAAPass(InRenderBus, Context);
		}
		else
		{
            const auto& Commands = InRenderBus.GetCommands(CurPass);
            if (Commands.empty())
                continue;

            if (PassBatchers[i])
            {
                ApplyPassRenderState(CurPass, Context, InRenderBus.GetViewMode());
                PassBatchers[i].Flush(CurPass, InRenderBus, Context);
            }
            else
            {
                ExecuteDefaultPass(CurPass, Commands, InRenderBus, Context);
            }
		}
		
	}
}

// ============================================================
// 패스별 기본 렌더 상태 테이블 초기화
// ============================================================
void FRenderer::InitializePassRenderStates()
{
	using E = ERenderPass;
	auto& S = PassRenderStates;

	//                              DepthStencil                   Blend                Rasterizer                  Topology                                Shader                   WireframeAware
	S[(uint32)E::Opaque] = { EDepthStencilState::Default,      EBlendState::Opaque,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.PrimitiveShader, true };
	S[(uint32)E::Decal] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.DecalShader, false };
	S[(uint32)E::Light] = {EDepthStencilState::Default,   EBlendState::AlphaBlend,
                            ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                            &Resources.LightPassShader,    false};	
	S[(uint32)E::Translucent] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.PrimitiveShader, false };
    
	S[(uint32)E::Fog] = {EDepthStencilState::Default,   EBlendState::AlphaBlend,
                           ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                           &Resources.FogPassShader,    false};

    S[(uint32)E::FXAA] = {EDepthStencilState::Default,   EBlendState::AlphaBlend,
                            ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                            &Resources.FXAAShader,      false};
	
	S[(uint32)E::SelectionMask] = { EDepthStencilState::StencilWrite, EBlendState::Opaque,     ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.SelectionMaskShader, false };
	S[(uint32)E::Editor] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     &Resources.EditorShader,    true };
	S[(uint32)E::Grid] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     &Resources.EditorShader,    false };
	S[(uint32)E::DepthLess] = { EDepthStencilState::DepthReadOnly,EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.GizmoShader,     false };
	S[(uint32)E::Font] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidNoCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, nullptr,                    true };
	S[(uint32)E::SubUV] = { EDepthStencilState::Default,      EBlendState::AlphaBlend, ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, nullptr,                    true };
    S[(uint32)E::PostProcessOutline] = {EDepthStencilState::Default,   EBlendState::AlphaBlend,
                                        ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                                        &Resources.OutlineShader,      false};

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
			SubUVCachedSRV = nullptr;
		},
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::SubUV && Cmd.Constants.SubUV.Particle)
			{
				const auto& SubUV = Cmd.Constants.SubUV;
				if (!SubUVCachedSRV && SubUV.Particle->IsLoaded())
				{
					SubUVCachedSRV = SubUV.Particle->SRV.Get();
				}

				SubUVBatcher.AddSprite(
					SubUV.Particle->SRV.Get(),
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
			else if (Cmd.Type == ERenderCommandType::Billboard && Cmd.Constants.Billboard.SRV)
			{
				SubUVBatcher.AddSprite(
					Cmd.Constants.Billboard.SRV,
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

	const FVector CameraPosition = Bus.GetView().GetInverse().GetOrigin();
	FEditorConstants EditorConstants = {};
	EditorConstants.CameraPosition = CameraPosition;
	Resources.EditorConstantBuffer.Update(Context, &EditorConstants, sizeof(FEditorConstants));

	ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

	ID3D11Buffer* cb = Resources.EditorConstantBuffer.GetBuffer();
	Context->VSSetConstantBuffers(4, 1, &cb);
	Context->PSSetConstantBuffers(4, 1, &cb);

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
		EDepthStencilState TargetDepth = (Cmd.DepthStencilState != static_cast<EDepthStencilState>(-1))
			? Cmd.DepthStencilState
			: State.DepthStencil;

		EBlendState TargetBlend = (Cmd.BlendState != static_cast<EBlendState>(-1))
			? Cmd.BlendState
			: State.Blend;

		Device.SetDepthStencilState(TargetDepth);
		Device.SetBlendState(TargetBlend);

		BindShaderByType(Cmd, Context, LastCommandType);
		if (Cmd.Type == ERenderCommandType::PostProcessOutline)
		{
			DrawPostProcessOutline(Context);
			continue;
		}
		DrawCommand(Context, Cmd);
	}
}

void FRenderer::ExecuteLightPass(const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
    ApplyPassRenderState(ERenderPass::Light, Context, Bus.GetViewMode());

    const FPassRenderState& State = PassRenderStates[(uint32)ERenderPass::Light];

    Device.SetDepthStencilState(State.DepthStencil);
    Device.SetBlendState(State.Blend);

    ID3D11ShaderResourceView* srvs[] = {
        CurrentRenderTargets.SceneColorSRV,
        CurrentRenderTargets.SceneNormalSRV,
        CurrentRenderTargets.SceneDepthSRV
    };

    Context->PSSetShaderResources(0, 3, srvs);

	Resources.LightPassShader.Bind(Context);

	/**
     * LightPass 는 풀스크린 쿼드에 그려지는데, mainVS 에서	정점 데이터를 생성하기 때문에 IA 단계에서 별도의
     * 버퍼 바인딩이 필요 없다.
	 */
    Context->IASetInputLayout(nullptr);
    Context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Context->Draw(3, 0);

    // SRV 해제 (중요!!)
    ID3D11ShaderResourceView* nullSRVs[] = {nullptr, nullptr, nullptr};
    Context->PSSetShaderResources(0, 3, nullSRVs);
}

/**
 * TODO: 
 * 현재 Light -> Fog -> FXAA 라는 가정을 바탕으로 작성되어 있는데 패스 간 의존성을 없애야함
 * Pass 전후 SRV, RTV 갱신에 대한 구조를 새로 짜야함
 */
void FRenderer::ExecuteFogPass(const TArray<FRenderCommand>& Commands, const FRenderBus& Bus,
                               ID3D11DeviceContext*                                    Context)
{
    if (!CurrentRenderTargets.SceneLightRTV || !CurrentRenderTargets.SceneLightSRV ||
        !CurrentRenderTargets.SceneFogRTV || !CurrentRenderTargets.SceneFogSRV ||
        !CurrentRenderTargets.SceneFXAARTV || !CurrentRenderTargets.SceneFXAASRV)
    {
        return;
    }

    ApplyPassRenderState(ERenderPass::Fog, Context, Bus.GetViewMode());
    const FPassRenderState& State = PassRenderStates[(uint32)ERenderPass::Fog];

    Resources.FogPassShader.Bind(Context);

    // Fog 누적: Light는 read-only로 유지하고 Fog/FXAA를 임시 ping-pong 버퍼로 사용한다.
    // 마지막 결과는 항상 SceneFog에 남도록 시작 타깃을 커맨드 개수 parity로 결정한다.
    const bool bOddCount = (Commands.size() & 1u) != 0;
    ID3D11ShaderResourceView* InputSceneSRV = CurrentRenderTargets.SceneLightSRV;
    ID3D11RenderTargetView* OutputSceneRTV = bOddCount ? CurrentRenderTargets.SceneFogRTV : CurrentRenderTargets.SceneFXAARTV;

    const FVector CameraPosition = Bus.GetView().GetInverse().GetOrigin();
    FEditorConstants EditorConstants = {};
    EditorConstants.CameraPosition = CameraPosition;
    Resources.EditorConstantBuffer.Update(Context, &EditorConstants, sizeof(FEditorConstants));
    ID3D11Buffer* cb4 = Resources.EditorConstantBuffer.GetBuffer();
    Context->VSSetConstantBuffers(4, 1, &cb4);
    Context->PSSetConstantBuffers(4, 1, &cb4);

    for (const auto& Cmd : Commands)
    {
        EDepthStencilState TargetDepth =
            (Cmd.DepthStencilState != static_cast<EDepthStencilState>(-1)) ? Cmd.DepthStencilState : State.DepthStencil;
        Device.SetDepthStencilState(TargetDepth);

        // Fog는 소스 장면을 포함해 최종색을 셰이더에서 계산하므로 블렌딩은 Opaque로 고정
        Device.SetBlendState(EBlendState::Opaque);

        ID3D11RenderTargetView* RTVs[MaxRTVCount] = { OutputSceneRTV, nullptr, nullptr };
        Context->OMSetRenderTargets(MaxRTVCount, RTVs, nullptr);

        ID3D11ShaderResourceView* srvs[] = {
            CurrentRenderTargets.SceneColorSRV,
            CurrentRenderTargets.SceneNormalSRV,
            CurrentRenderTargets.SceneDepthSRV,
            InputSceneSRV,
            CurrentRenderTargets.SceneWorldPosSRV
        };
        Context->PSSetShaderResources(0, 5, srvs);

        FFogConstants FogConstants = Cmd.Constants.Fog;
        Resources.FogPassConstantBuffer.Update(Context, &FogConstants, sizeof(FFogConstants));
        ID3D11Buffer* cb9 = Resources.FogPassConstantBuffer.GetBuffer();
        Context->VSSetConstantBuffers(9, 1, &cb9);
        Context->PSSetConstantBuffers(9, 1, &cb9);

        // 풀스크린 triangle
        Context->IASetInputLayout(nullptr);
        Context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->Draw(3, 0);

        ID3D11ShaderResourceView* nullSRVs[] = {nullptr, nullptr, nullptr, nullptr, nullptr};
        Context->PSSetShaderResources(0, 5, nullSRVs);

        // 다음 Fog 커맨드 입력/출력 swap
        if (OutputSceneRTV == CurrentRenderTargets.SceneFogRTV)
        {
            InputSceneSRV = CurrentRenderTargets.SceneFogSRV;
            OutputSceneRTV = CurrentRenderTargets.SceneFXAARTV;
        }
        else
        {
            InputSceneSRV = CurrentRenderTargets.SceneFXAASRV;
            OutputSceneRTV = CurrentRenderTargets.SceneFogRTV;
        }
    }

    SceneFinalSRV = CurrentRenderTargets.SceneFogSRV;
    SceneFinalRTV = CurrentRenderTargets.SceneFogRTV;
}

void FRenderer::ExecuteFXAAPass(const FRenderBus& Bus, ID3D11DeviceContext* Context) 
{
    ID3D11ShaderResourceView* srvs[] = {SceneFinalSRV.Get()}; // FXAA 패스에서는 최종 조명 결과만 필요
    ApplyPassRenderState(ERenderPass::FXAA, Context, Bus.GetViewMode());

    const FPassRenderState& State = PassRenderStates[(uint32)ERenderPass::FXAA];

    Device.SetDepthStencilState(State.DepthStencil);
    Device.SetBlendState(State.Blend);

    Context->PSSetShaderResources(0, 1, srvs);

	FFXAAConstants FXAAConstants = {};
    FXAAConstants.InvResolution[0] = (CurrentRenderTargets.Width > 0.0f) ? (1.0f / CurrentRenderTargets.Width) : 0.0f;
    FXAAConstants.InvResolution[1] = (CurrentRenderTargets.Height > 0.0f) ? (1.0f / CurrentRenderTargets.Height) : 0.0f;
    FXAAConstants.Threshold = std::clamp(Bus.GetFXAAThreshold(), 0.0f, 1.0f);
    Resources.FXAAConstantBuffer.Update(Context, &FXAAConstants, sizeof(FFXAAConstants));
    ID3D11Buffer* cb10 = Resources.FXAAConstantBuffer.GetBuffer();

	Context->PSSetConstantBuffers(10, 1, &cb10);

    Resources.FXAAShader.Bind(Context);

    /**
     * 풀스크린 쿼드에 그리는데, mainVS 에서	정점 데이터를 생성하기 때문에 IA 단계에서 별도의
     * 버퍼 바인딩이 필요 없다.
     */
    Context->IASetInputLayout(nullptr);
    Context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Context->Draw(3, 0);

    // SRV 해제 (중요!!)
    ID3D11ShaderResourceView* nullSRVs[] = {nullptr};
    Context->PSSetShaderResources(0, 1, nullSRVs);
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

	ERasterizerState Rasterizer = State.Rasterizer;
	if (State.bWireframeAware && CurViewMode == EViewMode::Wireframe)
	{
		Rasterizer = ERasterizerState::WireFrame;
	}

	Device.SetDepthStencilState(State.DepthStencil);
	Device.SetBlendState(State.Blend);
	Device.SetRasterizerState(Rasterizer);
	Context->IASetPrimitiveTopology(State.Topology);

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

	// 데이터 Update는 항상 수행하지만, 셰이더/상수 버퍼 바인딩은 타입이 변경된 경우에만 수행
    switch (InCmd.Type)
    {
    case ERenderCommandType::Gizmo:
        Resources.GizmoPerObjectConstantBuffer.Update(Context, &InCmd.Constants.Gizmo, sizeof(FGizmoConstants));
        
        if (bTypeChanged)
        {
            Resources.GizmoShader.Bind(Context);
            ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(1, 1, &cb1);
            ID3D11Buffer* cb2 = Resources.GizmoPerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(2, 1, &cb2);
            Context->PSSetConstantBuffers(2, 1, &cb2);
        }
        break;

	case ERenderCommandType::SelectionMask:
		break;

	case ERenderCommandType::PostProcessOutline:
	{
		FOutlineConstants outlineConstants = InCmd.Constants.Outline;
		outlineConstants.ViewportSize = FVector2(CurrentRenderTargets.Width, CurrentRenderTargets.Height);

		Resources.OutlineShader.Bind(Context);
		Resources.OutlineConstantBuffer.Update(Context, &outlineConstants, sizeof(FOutlineConstants));
		ID3D11Buffer* cb = Resources.OutlineConstantBuffer.GetBuffer();
		Context->VSSetConstantBuffers(5, 1, &cb);
		Context->PSSetConstantBuffers(5, 1, &cb);
		break;
	}

    case ERenderCommandType::StaticMesh:
	{
        Resources.StaticMeshConstantBuffer.Update(Context, &InCmd.Constants.StaticMesh, sizeof(FStaticMeshConstants));
        
        if (bTypeChanged)
        {
            Resources.StaticMeshShader.Bind(Context);
            
            ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(1, 1, &cb1);
            Context->PSSetConstantBuffers(1, 1, &cb1);

            ID3D11Buffer* cb6 = Resources.StaticMeshConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(6, 1, &cb6);
            Context->PSSetConstantBuffers(6, 1, &cb6);

            // 샘플러 상태도 주로 렌더 타입에 종속적이므로 스킵 가능
            ID3D11SamplerState* Samplers[] = { Resources.MeshSamplerState.Get() };
            Context->PSSetSamplers(0, 1, Samplers);
        }

        // [주의] 텍스처(SRV)는 타입이 같아도 메시의 머티리얼마다 변경될 수 있으므로 분기문 밖에서 매번 바인딩합니다.
        {
            ID3D11ShaderResourceView* SRVs[4] = {
                InCmd.Constants.StaticMesh.DiffuseSRV,
                InCmd.Constants.StaticMesh.AmbientSRV,
                InCmd.Constants.StaticMesh.SpecularSRV,
                InCmd.Constants.StaticMesh.BumpSRV
            };
            Context->PSSetShaderResources(0, 4, SRVs);
        }
        break;
    }

	case ERenderCommandType::Decal:
	{
		Resources.DecalConstantBuffer.Update(Context, &InCmd.Constants.Decal, sizeof(FDecalConstants));

		if (bTypeChanged)
		{
			ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(1, 1, &cb1);
			Context->PSSetConstantBuffers(1, 1, &cb1);

			ID3D11Buffer* cb8 = Resources.DecalConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(8, 1, &cb8);
			Context->PSSetConstantBuffers(8, 1, &cb8);

			// 샘플러 상태도 주로 렌더 타입에 종속적이므로 스킵 가능
			ID3D11SamplerState* Samplers[] = { Resources.MeshSamplerState.Get() };
			Context->PSSetSamplers(0, 1, Samplers);
		}

		ID3D11ShaderResourceView* SRVs[1] = { InCmd.Constants.Decal.DiffuseSRV };
		Context->PSSetShaderResources(0, 1, SRVs);
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
	frameConstantData.bIsWireframe = (InRenderBus.GetViewMode() == EViewMode::Wireframe);
	frameConstantData.WireframeColor = InRenderBus.GetWireframeColor();

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(0, 1, &b0);
	Context->PSSetConstantBuffers(0, 1, &b0);
}
