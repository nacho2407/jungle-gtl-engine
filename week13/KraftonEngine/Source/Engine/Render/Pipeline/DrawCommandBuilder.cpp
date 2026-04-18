#include "DrawCommandBuilder.h"

#include "Resource/ResourceManager.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/FogParams.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Proxy/TextRenderSceneProxy.h"
#include "Render/Proxy/FScene.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/PassRenderStateTable.h"
#include "Materials/Material.h"
#include "Texture/Texture2D.h"

// ============================================================
// Create / Release
// ============================================================

void FDrawCommandBuilder::Create(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, const FPassRenderStateTable* InPassRenderStateTable)
{
	CachedDevice  = InDevice;
	CachedContext = InContext;
	PassRenderStateTable = InPassRenderStateTable;

	EditorLines.Create(InDevice);
	GridLines.Create(InDevice);
	FontGeometry.Create(InDevice);
}

void FDrawCommandBuilder::Release()
{
	EditorLines.Release();
	GridLines.Release();
	FontGeometry.Release();

	for (FConstantBuffer& CB : PerObjectCBPool)
	{
		CB.Release();
	}
	PerObjectCBPool.clear();
}

// ============================================================
// BeginCollect — DrawCommandList + 동적 지오메트리 초기화
// ============================================================
void FDrawCommandBuilder::BeginCollect(const FFrameContext& Frame, uint32 MaxProxyCount)
{
	DrawCommandList.Reset();
	CollectViewMode = Frame.RenderOptions.ViewMode;
	bHasSelectionMaskCommands = false;

	// PerObjectCBPool 미리 할당 — Collect 도중 resize로 FDrawCommand.PerObjectCB
	// 포인터가 무효화되는 것을 방지
	if (MaxProxyCount > 0)
		EnsurePerObjectCBPoolCapacity(MaxProxyCount);

	// 동적 지오메트리 초기화
	EditorLines.Clear();
	GridLines.Clear();
	FontGeometry.Clear();
	FontGeometry.ClearScreen();

	if (const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default")))
		FontGeometry.EnsureCharInfoMap(FontRes);
}

// ============================================================
// SelectEffectiveShader — ViewMode에 따른 UberLit 셰이더 변형 선택
// ============================================================
FShader* FDrawCommandBuilder::SelectEffectiveShader(FShader* ProxyShader, EViewMode ViewMode)
{
	if (ProxyShader != FShaderManager::Get().GetShader(EShaderType::StaticMesh))
		return ProxyShader;

	switch (ViewMode)
	{
	case EViewMode::Lit_Gouraud:  return FShaderManager::Get().GetShader(EShaderType::UberLit_Gouraud);
	case EViewMode::Lit_Lambert:  return FShaderManager::Get().GetShader(EShaderType::UberLit_Lambert);
	case EViewMode::Lit_Phong:    return FShaderManager::Get().GetShader(EShaderType::UberLit_Phong);
	default:                      return ProxyShader;
	}
}

// ============================================================
// ApplyMaterialRenderState — Material 렌더 상태 오버라이드 (Wireframe 우선)
// ============================================================
void FDrawCommandBuilder::ApplyMaterialRenderState(FDrawCommandRenderState& OutState, const UMaterial* Mat, const FDrawCommandRenderState& BaseState)
{
	OutState.Blend        = Mat->GetBlendState();
	OutState.DepthStencil = Mat->GetDepthStencilState();
	if (BaseState.Rasterizer != ERasterizerState::WireFrame)
		OutState.Rasterizer = Mat->GetRasterizerState();
}

// ============================================================
// BuildCommandForProxy — Proxy → FDrawCommand 변환
// ============================================================
void FDrawCommandBuilder::BuildCommandForProxy(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass)
{
	if (!Proxy.MeshBuffer || !Proxy.MeshBuffer->IsValid()) return;

	ID3D11DeviceContext* Ctx = CachedContext;

	// PassState → RenderState 변환 (Wireframe 오버라이드 포함)
	const FDrawCommandRenderState BaseRenderState = PassRenderStateTable->ToDrawCommandState(Pass, CollectViewMode);

	// PerObjectCB 업데이트
	FConstantBuffer* PerObjCB = GetPerObjectCBForProxy(Proxy);
	if (PerObjCB && Proxy.NeedsPerObjectCBUpload())
	{
		PerObjCB->Update(Ctx, &Proxy.PerObjectConstants, sizeof(FPerObjectConstants));
		Proxy.ClearPerObjectCBDirty();
	}

	// PerShaderCB 업데이트 (Gizmo, SubUV, Decal 등) — lazy creation if buffer not yet allocated
	if (Proxy.ExtraCB.Buffer)
	{
		if (!Proxy.ExtraCB.Buffer->GetBuffer())
			Proxy.ExtraCB.Buffer->Create(CachedDevice, Proxy.ExtraCB.Size);
		Proxy.ExtraCB.Buffer->Update(Ctx, Proxy.ExtraCB.Data, Proxy.ExtraCB.Size);
	}

	// SelectionMask 커맨드 존재 추적
	if (Pass == ERenderPass::SelectionMask)
		bHasSelectionMaskCommands = true;

	FShader* EffectiveShader = SelectEffectiveShader(Proxy.Shader, CollectViewMode);

	// Proxy.ExtraCB → PerShaderCB 인덱스 변환 헬퍼
	auto SetProxyExtraCB = [&](FDrawCommand& Cmd)
		{
			if (Proxy.ExtraCB.Buffer)
			{
				const uint32 Idx = Proxy.ExtraCB.Slot - ECBSlot::PerShader0;
				check(Idx < 2);
				Cmd.Bindings.PerShaderCB[Idx] = Proxy.ExtraCB.Buffer;
			}
		};

	const bool bDepthOnly = (Pass == ERenderPass::PreDepth);
	const bool bApplyMaterialState = !bDepthOnly && (Pass == Proxy.Pass);

	// MeshBuffer → FDrawCommandBuffer 변환
	FDrawCommandBuffer ProxyBuffer;
	ProxyBuffer.VB       = Proxy.MeshBuffer->GetVertexBuffer().GetBuffer();
	ProxyBuffer.VBStride = Proxy.MeshBuffer->GetVertexBuffer().GetStride();
	ProxyBuffer.IB       = Proxy.MeshBuffer->GetIndexBuffer().GetBuffer();

	// 커맨드 공통 초기화 람다
	auto InitCommand = [&](FDrawCommand& Cmd)
		{
			Cmd.Pass        = Pass;
			Cmd.Shader      = EffectiveShader;
			Cmd.RenderState = BaseRenderState;
			Cmd.Buffer      = ProxyBuffer;
			Cmd.PerObjectCB = PerObjCB;
		};

	// SectionDraws가 있으면 섹션당 1개 커맨드, 없으면 1개 커맨드
	if (!Proxy.SectionDraws.empty())
	{
		for (const FMeshSectionDraw& Section : Proxy.SectionDraws)
		{
			if (Section.IndexCount == 0) continue;
			if (!ProxyBuffer.IB) continue;

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			InitCommand(Cmd);
			Cmd.Buffer.FirstIndex = Section.FirstIndex;
			Cmd.Buffer.IndexCount = Section.IndexCount;

			if (!bDepthOnly && Section.Material)
			{
				UMaterial* Mat = Section.Material;

				// dirty CB 업로드
				Mat->FlushDirtyBuffers(Ctx);

				Cmd.Bindings.PerShaderCB[0] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader0);
				Cmd.Bindings.PerShaderCB[1] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader1);
				SetProxyExtraCB(Cmd);

				// CachedSRVs에서 직접 복사 (map lookup 회피)
				const ID3D11ShaderResourceView* const* MatSRVs = Mat->GetCachedSRVs();
				for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
					Cmd.Bindings.SRVs[s] = const_cast<ID3D11ShaderResourceView*>(MatSRVs[s]);

				if (bApplyMaterialState)
					ApplyMaterialRenderState(Cmd.RenderState, Mat, BaseRenderState);
			}
			else if (!bDepthOnly)
			{
				SetProxyExtraCB(Cmd);
			}

			Cmd.BuildSortKey();
		}
	}
	else
	{
		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		InitCommand(Cmd);

		// MeshBuffer 전체 드로우 — IndexCount/VertexCount 명시
		if (ProxyBuffer.IB)
			Cmd.Buffer.IndexCount = Proxy.MeshBuffer->GetIndexBuffer().GetIndexCount();
		else
			Cmd.Buffer.VertexCount = Proxy.MeshBuffer->GetVertexBuffer().GetVertexCount();

		if (!bDepthOnly)
		{
			SetProxyExtraCB(Cmd);
			Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = Proxy.DiffuseSRV;
		}

		if (bApplyMaterialState && Proxy.Material)
			ApplyMaterialRenderState(Cmd.RenderState, Proxy.Material, BaseRenderState);

		Cmd.BuildSortKey();
	}
}

// ============================================================
// BuildDecalCommandForReceiver
// ============================================================
void FDrawCommandBuilder::BuildDecalCommandForReceiver(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy)
{
	if (!ReceiverProxy.MeshBuffer || !ReceiverProxy.MeshBuffer->IsValid()) return;
	if (!DecalProxy.Shader || !DecalProxy.DiffuseSRV) return;

	ID3D11DeviceContext* Ctx = CachedContext;
	const ERenderPass DecalPass = DecalProxy.Pass;
	const FDrawCommandRenderState BaseRenderState = PassRenderStateTable->ToDrawCommandState(DecalPass, CollectViewMode);

	FConstantBuffer* ReceiverPerObjCB = GetPerObjectCBForProxy(ReceiverProxy);
	if (ReceiverPerObjCB && ReceiverProxy.NeedsPerObjectCBUpload())
	{
		ReceiverPerObjCB->Update(Ctx, &ReceiverProxy.PerObjectConstants, sizeof(FPerObjectConstants));
		ReceiverProxy.ClearPerObjectCBDirty();
	}

	if (DecalProxy.ExtraCB.Buffer)
	{
		if (!DecalProxy.ExtraCB.Buffer->GetBuffer())
		{
			DecalProxy.ExtraCB.Buffer->Create(CachedDevice, DecalProxy.ExtraCB.Size);
		}
		DecalProxy.ExtraCB.Buffer->Update(Ctx, DecalProxy.ExtraCB.Data, DecalProxy.ExtraCB.Size);
	}

	FDrawCommandBuffer ReceiverBuffer;
	ReceiverBuffer.VB       = ReceiverProxy.MeshBuffer->GetVertexBuffer().GetBuffer();
	ReceiverBuffer.VBStride = ReceiverProxy.MeshBuffer->GetVertexBuffer().GetStride();
	ReceiverBuffer.IB       = ReceiverProxy.MeshBuffer->GetIndexBuffer().GetBuffer();

	auto AddDraw = [&](uint32 FirstIndex, uint32 IndexCount)
		{
			if (IndexCount == 0) return;

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.Pass        = DecalPass;
			Cmd.Shader      = DecalProxy.Shader;
			Cmd.RenderState = BaseRenderState;

			// 머티리얼 기반 렌더 상태 오버라이드
			if (DecalProxy.Material)
				ApplyMaterialRenderState(Cmd.RenderState, DecalProxy.Material, BaseRenderState);

			Cmd.Buffer            = ReceiverBuffer;
			Cmd.Buffer.FirstIndex = FirstIndex;
			Cmd.Buffer.IndexCount = IndexCount;
			Cmd.PerObjectCB       = ReceiverPerObjCB;
			Cmd.Bindings.PerShaderCB[0] = DecalProxy.ExtraCB.Buffer;
			Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = DecalProxy.DiffuseSRV;
			Cmd.BuildSortKey();
		};

	if (!ReceiverProxy.SectionDraws.empty())
	{
		for (const FMeshSectionDraw& Section : ReceiverProxy.SectionDraws)
		{
			AddDraw(Section.FirstIndex, Section.IndexCount);
		}
	}
	else if (ReceiverBuffer.IB)
	{
		AddDraw(0, ReceiverProxy.MeshBuffer->GetIndexBuffer().GetIndexCount());
	}
}

// ============================================================
// AddWorldText — Font 프록시 배칭
// ============================================================
void FDrawCommandBuilder::AddWorldText(const FTextRenderSceneProxy* TextProxy, const FFrameContext& Frame)
{
	FontGeometry.AddWorldText(
		TextProxy->CachedText,
		TextProxy->CachedBillboardMatrix.GetLocation(),
		Frame.CameraRight,
		Frame.CameraUp,
		TextProxy->CachedBillboardMatrix.GetScale(),
		TextProxy->CachedFontScale
	);
}

// ============================================================
// BuildDynamicCommands — Scene 경량 데이터 → 동적 지오메트리 → FDrawCommand
// ============================================================
void FDrawCommandBuilder::BuildDynamicCommands(const FFrameContext& Frame, const FScene* Scene)
{
	PrepareDynamicGeometry(Frame, Scene);
	BuildDynamicDrawCommands(Frame, Scene);
}

// ============================================================
// PrepareDynamicGeometry — FScene의 경량 데이터 → 라인/폰트 지오메트리
// ============================================================
void FDrawCommandBuilder::PrepareDynamicGeometry(const FFrameContext& Frame, const FScene* Scene)
{
	if (!Scene) return;

	// --- Editor 패스: AABB 디버그 박스 + DebugDraw 라인 ---
	for (const auto& AABB : Scene->GetDebugAABBs())
	{
		EditorLines.AddAABB(FBoundingBox{ AABB.Min, AABB.Max }, AABB.Color);
	}
	for (const auto& Line : Scene->GetDebugLines())
	{
		EditorLines.AddLine(Line.Start, Line.End, Line.Color.ToVector4());
	}

	// --- Grid 패스: 월드 그리드 + 축 ---
	if (Scene->HasGrid())
	{
		const FVector CameraPos = Frame.View.GetInverseFast().GetLocation();
		FVector CameraFwd = Frame.CameraRight.Cross(Frame.CameraUp);
		CameraFwd.Normalize();

		GridLines.AddWorldHelpers(
			Frame.RenderOptions.ShowFlags,
			Scene->GetGridSpacing(),
			Scene->GetGridHalfLineCount(),
			CameraPos, CameraFwd, Frame.IsFixedOrtho());
	}

	// --- OverlayFont 패스: 스크린 공간 텍스트 ---
	for (const auto& Text : Scene->GetOverlayTexts())
	{
		if (!Text.Text.empty())
		{
			FontGeometry.AddScreenText(
				Text.Text,
				Text.Position.X,
				Text.Position.Y,
				Frame.ViewportWidth,
				Frame.ViewportHeight,
				Text.Scale
			);
		}
	}
}

// ============================================================
// BuildDynamicDrawCommands — 오케스트레이터
// ============================================================
void FDrawCommandBuilder::BuildDynamicDrawCommands(const FFrameContext& Frame, const FScene* Scene)
{
	EViewMode ViewMode = Frame.RenderOptions.ViewMode;
	BuildEditorLineCommands(ViewMode);
	BuildPostProcessCommands(Frame, Scene);
	BuildFontCommands(ViewMode);
}

// ============================================================
// EmitLineCommand — 라인 지오메트리 → FDrawCommand 공통 헬퍼
// ============================================================
void FDrawCommandBuilder::EmitLineCommand(FLineGeometry& Lines, FShader* Shader, const FDrawCommandRenderState& RS)
{
	if (Lines.GetLineCount() > 0 && Lines.UploadBuffers(CachedContext))
	{
		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		Cmd.Pass        = ERenderPass::EditorLines;
		Cmd.Shader      = Shader;
		Cmd.RenderState = RS;
		Cmd.Buffer      = { Lines.GetVBBuffer(), Lines.GetVBStride(), Lines.GetIBBuffer() };
		Cmd.Buffer.IndexCount = Lines.GetIndexCount();
		Cmd.BuildSortKey();
	}
}

// ============================================================
// BuildEditorLineCommands — EditorLines + GridLines
// ============================================================
void FDrawCommandBuilder::BuildEditorLineCommands(EViewMode ViewMode)
{
	FShader* EditorShader = FShaderManager::Get().GetShader(EShaderType::Editor);
	const FDrawCommandRenderState EditorLinesRS = PassRenderStateTable->ToDrawCommandState(ERenderPass::EditorLines, ViewMode);

	EmitLineCommand(EditorLines, EditorShader, EditorLinesRS);
	EmitLineCommand(GridLines,   EditorShader, EditorLinesRS);
}

// ============================================================
// BuildPostProcessCommands — HeightFog, Outline, SceneDepth, WorldNormal, FXAA
// ============================================================
void FDrawCommandBuilder::BuildPostProcessCommands(const FFrameContext& Frame, const FScene* CollectScene)
{
	ID3D11DeviceContext* Ctx = CachedContext;
	EViewMode ViewMode = Frame.RenderOptions.ViewMode;
	const FDrawCommandRenderState PPRS = PassRenderStateTable->ToDrawCommandState(ERenderPass::PostProcess, ViewMode);

	// HeightFog (UserBits=0 → Outline보다 먼저)
	if (Frame.RenderOptions.ShowFlags.bFog && CollectScene && CollectScene->HasFog())
	{
		FShader* FogShader = FShaderManager::Get().GetShader(EShaderType::HeightFog);
		if (FogShader)
		{
			FConstantBuffer* FogCB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::Fog, sizeof(FFogConstants));
			const FFogParams& FogParams = CollectScene->GetFogParams();
			FFogConstants fogData = {};
			fogData.InscatteringColor = FogParams.InscatteringColor;
			fogData.Density = FogParams.Density;
			fogData.HeightFalloff = FogParams.HeightFalloff;
			fogData.FogBaseHeight = FogParams.FogBaseHeight;
			fogData.StartDistance = FogParams.StartDistance;
			fogData.CutoffDistance = FogParams.CutoffDistance;
			fogData.MaxOpacity = FogParams.MaxOpacity;
			FogCB->Update(Ctx, &fogData, sizeof(FFogConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(FogShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = FogCB;
			Cmd.BuildSortKey(0);
		}
	}

	// Outline (UserBits=1 → HeightFog 뒤)
	if (bHasSelectionMaskCommands)
	{
		FShader* PPShader = FShaderManager::Get().GetShader(EShaderType::OutlinePostProcess);
		if (PPShader)
		{
			FConstantBuffer* OutlineCB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::Outline, sizeof(FOutlinePostProcessConstants));
			FOutlinePostProcessConstants ppConstants;
			ppConstants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
			ppConstants.OutlineThickness = 3.0f;
			OutlineCB->Update(Ctx, &ppConstants, sizeof(ppConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(PPShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = OutlineCB;
			Cmd.BuildSortKey(1);
		}
	}

	// SceneDepth (UserBits=2 → Outline 뒤)
	if (CollectViewMode == EViewMode::SceneDepth)
	{
		FShader* DepthShader = FShaderManager::Get().GetShader(EShaderType::SceneDepth);
		if (DepthShader)
		{
			FConstantBuffer* SceneDepthCB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::SceneDepth, sizeof(FSceneDepthPConstants));
			FViewportRenderOptions Opts = Frame.RenderOptions;
			FSceneDepthPConstants depthData = {};
			depthData.Exponent = Opts.Exponent;
			depthData.NearClip = Frame.NearClip;
			depthData.FarClip = Frame.FarClip;
			depthData.Mode = Opts.SceneDepthVisMode;
			SceneDepthCB->Update(Ctx, &depthData, sizeof(FSceneDepthPConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(DepthShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = SceneDepthCB;
			Cmd.BuildSortKey(2);
		}
	}

	// WorldNormal (UserBits=3 → SceneDepth 뒤)
	if (CollectViewMode == EViewMode::WorldNormal)
	{
		FShader* NormalShader = FShaderManager::Get().GetShader(EShaderType::SceneNormal);
		if (NormalShader)
		{
			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(NormalShader, ERenderPass::PostProcess, PPRS);
			Cmd.BuildSortKey(3);
		}
	}

	// FXAA
	if (Frame.RenderOptions.ShowFlags.bFXAA)
	{
		FShader* FXAAShader = FShaderManager::Get().GetShader(EShaderType::FXAA);
		if (FXAAShader)
		{
			FConstantBuffer* FXAACB = FConstantBufferPool::Get().GetBuffer(ECBPoolKey::FXAA, sizeof(FFXAAConstants));
			FViewportRenderOptions Opts = Frame.RenderOptions;
			FFXAAConstants FXAAData = {};
			FXAAData.EdgeThreshold = Opts.EdgeThreshold;
			FXAAData.EdgeThresholdMin = Opts.EdgeThresholdMin;
			FXAACB->Update(Ctx, &FXAAData, sizeof(FFXAAConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(FXAAShader, ERenderPass::FXAA,
				PassRenderStateTable->ToDrawCommandState(ERenderPass::FXAA, ViewMode));
			Cmd.Bindings.PerShaderCB[0] = FXAACB;
			Cmd.BuildSortKey(0);
		}
	}
}

// ============================================================
// BuildFontCommands — World text (AlphaBlend) + Screen text (OverlayFont)
// ============================================================
void FDrawCommandBuilder::BuildFontCommands(EViewMode ViewMode)
{
	const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
	if (!FontRes || !FontRes->IsLoaded()) return;

	ID3D11DeviceContext* Ctx = CachedContext;

	if (FontGeometry.GetWorldQuadCount() > 0 && FontGeometry.UploadWorldBuffers(Ctx))
	{
		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		Cmd.Pass        = ERenderPass::AlphaBlend;
		Cmd.Shader      = FShaderManager::Get().GetShader(EShaderType::Font);
		Cmd.RenderState = PassRenderStateTable->ToDrawCommandState(ERenderPass::AlphaBlend, ViewMode);
		Cmd.Buffer      = { FontGeometry.GetWorldVBBuffer(), FontGeometry.GetWorldVBStride(), FontGeometry.GetWorldIBBuffer() };
		Cmd.Buffer.IndexCount = FontGeometry.GetWorldIndexCount();
		Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = FontRes->SRV;
		Cmd.BuildSortKey();
	}

	if (FontGeometry.GetScreenQuadCount() > 0 && FontGeometry.UploadScreenBuffers(Ctx))
	{
		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		Cmd.Pass        = ERenderPass::OverlayFont;
		Cmd.Shader      = FShaderManager::Get().GetShader(EShaderType::OverlayFont);
		Cmd.RenderState = PassRenderStateTable->ToDrawCommandState(ERenderPass::OverlayFont, ViewMode);
		Cmd.Buffer      = { FontGeometry.GetScreenVBBuffer(), FontGeometry.GetScreenVBStride(), FontGeometry.GetScreenIBBuffer() };
		Cmd.Buffer.IndexCount = FontGeometry.GetScreenIndexCount();
		Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = FontRes->SRV;
		Cmd.BuildSortKey();
	}
}

// ============================================================
// PerObjectCB 풀 관리
// ============================================================
void FDrawCommandBuilder::EnsurePerObjectCBPoolCapacity(uint32 RequiredCount)
{
	if (PerObjectCBPool.size() >= RequiredCount)
	{
		return;
	}

	const size_t OldCount = PerObjectCBPool.size();
	PerObjectCBPool.resize(RequiredCount);

	for (size_t Index = OldCount; Index < PerObjectCBPool.size(); ++Index)
	{
		PerObjectCBPool[Index].Create(CachedDevice, sizeof(FPerObjectConstants));
	}
}

FConstantBuffer* FDrawCommandBuilder::GetPerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy)
{
	if (Proxy.ProxyId == UINT32_MAX)
	{
		return nullptr;
	}

	EnsurePerObjectCBPoolCapacity(Proxy.ProxyId + 1);
	return &PerObjectCBPool[Proxy.ProxyId];
}
