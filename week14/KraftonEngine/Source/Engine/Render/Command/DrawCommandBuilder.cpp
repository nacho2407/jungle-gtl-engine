#include "DrawCommandBuilder.h"

#include "Resource/ResourceManager.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/FogParams.h"
#include "Render/Types/LODContext.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Proxy/TextRenderSceneProxy.h"
#include "Render/Proxy/DecalSceneProxy.h"
#include "Render/Proxy/ShapeSceneProxy.h"
#include "Render/Proxy/BoneDebugSceneProxy.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"
#include "Render/Scene/FScene.h"
#include "Render/Types/RenderConstants.h"
#include "Render/RenderPass/PassRenderStateTable.h"
#include "Render/Pipeline/RenderCollector.h"
#include "Materials/Material.h"
#include "Texture/Texture2D.h"

// UpdateProxyLOD defined in RenderCollector.cpp (shared)
extern void UpdateProxyLOD(FPrimitiveSceneProxy* Proxy, const FLODUpdateContext& LODCtx);

// ============================================================
// Create / Release
// ============================================================

void FDrawCommandBuilder::Create(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, const FPassRenderStateTable* InPassRenderStateTable)
{
	CachedDevice = InDevice;
	CachedContext = InContext;
	PassRenderStateTable = InPassRenderStateTable;

	EditorLines.Create(InDevice);
	GridLines.Create(InDevice);
	DebugBoneLines.Create(InDevice);
	FontGeometry.Create(InDevice);

	FogCB.Create(InDevice, sizeof(FFogConstants), "FogCB");
	OutlineCB.Create(InDevice, sizeof(FOutlinePostProcessConstants), "OutlineCB");
	SceneDepthCB.Create(InDevice, sizeof(FSceneDepthPConstants), "SceneDepthCB");
	FXAACB.Create(InDevice, sizeof(FFXAAConstants), "FXAACB");
	GammaCorrectionCB.Create(InDevice, sizeof(FGammaCorrectionConstants), "GammaCorrectionCB");

	CameraFadeCB.Create(InDevice, sizeof(FCameraFadeConstants), "CameraFadeCB");
	CameraVignetteCB.Create(InDevice, sizeof(FCameraVignetteConstants), "CameraVignetteCB");
	CameraLetterboxCB.Create(InDevice, sizeof(FCameraLetterboxConstants), "CameraLetterboxCB");
	BoneHeatMapCB.Create(InDevice, sizeof(FBoneHeatMapConstants), "BoneHeatMapCB");
}

void FDrawCommandBuilder::Release()
{
	EditorLines.Release();
	GridLines.Release();
	DebugBoneLines.Release();
	FontGeometry.Release();

	for (auto& Pair : PerSceneObjectCBPool)
	{
		for (FConstantBuffer& CB : Pair.second)
		{
			CB.Release();
		}
		Pair.second.clear();
	}
	PerSceneObjectCBPool.clear();

	FogCB.Release();
	OutlineCB.Release();
	SceneDepthCB.Release();
	FXAACB.Release();
	GammaCorrectionCB.Release();
	
	CameraFadeCB.Release();
	CameraVignetteCB.Release();
	CameraLetterboxCB.Release();
	BoneHeatMapCB.Release();
}

// ============================================================
// BeginCollect — DrawCommandList + 동적 지오메트리 초기화
// ============================================================
void FDrawCommandBuilder::BeginCollect(const FFrameContext& Frame)
{
	DrawCommandList.Reset();
	CollectViewMode = Frame.RenderOptions.ViewMode;
	bCollectWeightBoneHeatMap = Frame.RenderOptions.bWeightBoneHeatMap;
	CollectWeightBoneHeatMapBoneIndex = Frame.RenderOptions.WeightBoneHeatMapBoneIndex;

	CollectCameraPosition = Frame.CameraPosition;
	CollectCameraForward = Frame.CameraForward;

	bHasSelectionMaskCommands = false;

	// 동적 지오메트리 초기화
	EditorLines.Clear();
	GridLines.Clear();
	DebugBoneLines.Clear();
	FontGeometry.Clear();
	FontGeometry.ClearScreen();

	if (const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default")))
		FontGeometry.EnsureCharInfoMap(FontRes);
}

// ============================================================
// SelectEffectiveShader — ViewMode에 따른 UberLit 셰이더 변형 선택
// ============================================================
FShader* FDrawCommandBuilder::SelectEffectiveShader(FShader* ProxyShader, EViewMode ViewMode,
	bool bUseSkeletalVertexFactory, bool bUseInstancedVertexFactory, bool bWeightBoneHeatMap)
{
	if (ProxyShader != FShaderManager::Get().GetOrCreate(EShaderPath::UberLit))
		return ProxyShader;

	const EUberLitDefines::EVertexFactory VertexFactory =
		bUseSkeletalVertexFactory ? EUberLitDefines::EVertexFactory::SkeletalMesh :
		bUseInstancedVertexFactory ? EUberLitDefines::EVertexFactory::InstancedStaticMesh :
		EUberLitDefines::EVertexFactory::StaticMesh;

	switch (ViewMode)
	{
	case EViewMode::Unlit:
		return FShaderManager::Get().GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel::Unlit, VertexFactory, EShaderErrorMode::Notification, bWeightBoneHeatMap);
	case EViewMode::Lit_Gouraud:
		return FShaderManager::Get().GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel::Gouraud, VertexFactory, EShaderErrorMode::Notification, bWeightBoneHeatMap);
	case EViewMode::Lit_Lambert:
		return FShaderManager::Get().GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel::Lambert, VertexFactory, EShaderErrorMode::Notification, bWeightBoneHeatMap);
	case EViewMode::Lit_Phong:
	case EViewMode::LightCulling:
		return FShaderManager::Get().GetOrCreateUberLitPermutation(EUberLitDefines::ELightingModel::Phong, VertexFactory, EShaderErrorMode::Notification, bWeightBoneHeatMap);
	default:
		return (bUseSkeletalVertexFactory || bUseInstancedVertexFactory)
			? FShaderManager::Get().GetOrCreateUberLitPermutation(
				EUberLitDefines::ELightingModel::Default,
				VertexFactory,
				EShaderErrorMode::Notification,
				bWeightBoneHeatMap)
			: ProxyShader;
	}
}

// ============================================================
// ApplyMaterialRenderState — Material 렌더 상태 오버라이드 (Wireframe 우선)
// ============================================================
void FDrawCommandBuilder::ApplyMaterialRenderState(FDrawCommandRenderState& OutState, const UMaterialInterface* Mat, const FDrawCommandRenderState& BaseState)
{
	OutState.Blend = Mat->GetBlendState();
	OutState.DepthStencil = Mat->GetDepthStencilState();
	if (BaseState.Rasterizer != ERasterizerState::WireFrame)
		OutState.Rasterizer = Mat->GetRasterizerState();
}

// ============================================================
// BuildCommandForProxy — Proxy → FDrawCommand 변환
// ============================================================
void FDrawCommandBuilder::BuildCommandForProxy(FScene& Scene, const FPrimitiveSceneProxy& Proxy, ERenderPass Pass)
{
	// if (!Proxy.GetMeshBuffer() || !Proxy.GetMeshBuffer()->IsValid()) return;
	ID3D11DeviceContext* Ctx = CachedContext;

	const bool bSkeletal = Proxy.HasProxyFlag(EPrimitiveProxyFlags::SkeletalMesh);
	const bool bWeightBoneHeatMap = bSkeletal && bCollectWeightBoneHeatMap && CollectWeightBoneHeatMapBoneIndex >= 0;
	const bool bGPUSkinning = bSkeletal && (SkinningModeRuntime::Get() == ESkinningMode::GPU || bWeightBoneHeatMap);
	const FSkeletalMeshSceneProxy* SkeletalProxy = bSkeletal
		? static_cast<const FSkeletalMeshSceneProxy*>(&Proxy)
		: nullptr;

	FDrawCommandBuffer ProxyBuffer;
	if (bGPUSkinning)
	{
		if (!SkeletalProxy || !SkeletalProxy->PrepareGpuSkinningDrawBuffer(CachedDevice, Ctx, ProxyBuffer)) return;
	}
	else if (!Proxy.PrepareDrawBuffer(CachedDevice, Ctx, ProxyBuffer))
	{
		return;
	}
	if (!ProxyBuffer.HasBuffers()) return;

	// PerObjectCB 업데이트
	FConstantBuffer* PerObjCB = GetPerObjectCBForProxy(&Scene, Proxy);
	if (PerObjCB && Proxy.NeedsPerObjectCBUpload())
	{
		PerObjCB->Update(Ctx, &Proxy.GetPerObjectConstants(), sizeof(FPerObjectConstants));
		Proxy.ClearPerObjectCBDirty();
	}

	FProxyCommandBuildContext BuildCtx;
	BuildCtx.ProxyBuffer = ProxyBuffer;
	BuildCtx.PerObjCB = PerObjCB;
	BuildCtx.SkeletalProxy = SkeletalProxy;
	BuildCtx.bSkeletal = bSkeletal;
	BuildCtx.bGPUSkinning = bGPUSkinning;
	BuildCtx.bWeightBoneHeatMap = bWeightBoneHeatMap;

	if (bWeightBoneHeatMap)
	{
		FBoneHeatMapConstants BoneHeatMapConstants = {};
		BoneHeatMapConstants.SelectedBoneIndex = CollectWeightBoneHeatMapBoneIndex;
		BoneHeatMapCB.Update(Ctx, &BoneHeatMapConstants, sizeof(FBoneHeatMapConstants));
	}

	// SelectionMask 커맨드 존재 추적
	if (Pass == ERenderPass::SelectionMask)
		bHasSelectionMaskCommands = true;

	// 섹션당 1개 커맨드 (per-section 셰이더)
 	for (const FMeshSectionDraw& Section : Proxy.GetSectionDraws())
	{
		BuildCommandForSection(Scene, Proxy, Section, Pass, BuildCtx);
	}
}

// ============================================================
// BuildDecalCommandForReceiver
// ============================================================
void FDrawCommandBuilder::BuildDecalCommandForReceiver(FScene& Scene, const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy)
{
	if (!ReceiverProxy.GetMeshBuffer() || !ReceiverProxy.GetMeshBuffer()->IsValid()) return;

	// Decal Material은 SectionDraws[0]에 저장됨
	UMaterialInterface* DecalMat = DecalProxy.GetSectionDraws().empty() ? nullptr : DecalProxy.GetSectionDraws()[0].Material;
	if (!DecalMat || !DecalMat->GetShader()) return;

	ID3D11DeviceContext* Ctx = CachedContext;
	const ERenderPass DecalPass = DecalProxy.GetRenderPass();
	const FDrawCommandRenderState BaseRenderState = PassRenderStateTable->ToDrawCommandState(DecalPass, CollectViewMode);

	FConstantBuffer* ReceiverPerObjCB = GetPerObjectCBForProxy(&Scene, ReceiverProxy);
	if (ReceiverPerObjCB && ReceiverProxy.NeedsPerObjectCBUpload())
	{
		ReceiverPerObjCB->Update(Ctx, &ReceiverProxy.GetPerObjectConstants(), sizeof(FPerObjectConstants));
		ReceiverProxy.ClearPerObjectCBDirty();
	}

	// Decal Material의 CB 업로드 (PerShaderOverride 포함)
	DecalMat->FlushDirtyBuffers(CachedDevice, Ctx);

	FDrawCommandBuffer ReceiverBuffer;
	ReceiverBuffer.VB = ReceiverProxy.GetMeshBuffer()->GetVertexBuffer().GetBuffer();
	ReceiverBuffer.VBStride = ReceiverProxy.GetMeshBuffer()->GetVertexBuffer().GetStride();
	ReceiverBuffer.IB = ReceiverProxy.GetMeshBuffer()->GetIndexBuffer().GetBuffer();

	auto AddDraw = [&](uint32 FirstIndex, uint32 IndexCount)
		{
			if (IndexCount == 0) return;

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.Pass = DecalPass;
			Cmd.Shader = DecalMat->GetShader();
			Cmd.RenderState = BaseRenderState;

			// 머티리얼 기반 렌더 상태 오버라이드
			ApplyMaterialRenderState(Cmd.RenderState, DecalMat, BaseRenderState);

			Cmd.Buffer = ReceiverBuffer;
			Cmd.Buffer.FirstIndex = FirstIndex;
			Cmd.Buffer.IndexCount = IndexCount;
			Cmd.PerObjectCB = ReceiverPerObjCB;
			Cmd.Bindings.PerShaderCB[0] = DecalMat->GetGPUBufferBySlot(ECBSlot::PerShader0);

			for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
				Cmd.Bindings.SRVs[s] = DecalMat->GetSRV(static_cast<EMaterialTextureSlot>(s));

			Cmd.BuildSortKey();
		};

	if (!ReceiverProxy.GetSectionDraws().empty())
	{
		for (const FMeshSectionDraw& Section : ReceiverProxy.GetSectionDraws())
		{
			AddDraw(Section.FirstIndex, Section.IndexCount);
		}
	}
	else if (ReceiverBuffer.IB)
	{
		AddDraw(0, ReceiverProxy.GetMeshBuffer()->GetIndexBuffer().GetIndexCount());
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
// BuildCommands — 프록시 커맨드 + 동적 커맨드 일괄 생성
// ============================================================
void FDrawCommandBuilder::BuildCommands(const FFrameContext& Frame, FScene* Scene, const FCollectOutput& Output)
{
	if (Scene)
	{
		EnsurePerObjectCBPoolCapacity(Scene, Scene->GetProxyCount());
		BuildProxyCommands(Frame, *Scene, Output);
	}

	BuildDynamicCommands(Frame, Scene);
}

// ============================================================
// BuildProxyCommands — RenderableProxies → DrawCommand
// ============================================================
void FDrawCommandBuilder::BuildProxyCommands(const FFrameContext& Frame, FScene& Scene, const FCollectOutput& Output)
{
	const bool bShowBoundingVolume = Frame.RenderOptions.ShowFlags.bBoundingVolume;
	const bool bIsEditor = (Frame.WorldType == EWorldType::Editor);
	const bool bShowCollision = bIsEditor
		? Frame.RenderOptions.ShowFlags.bCollision
		: Frame.RenderOptions.ShowFlags.bShowCollisionShape;

	for (FPrimitiveSceneProxy* Proxy : Output.RenderableProxies)
	{
		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::BoneDebug))
		{
			const FBoneDebugSceneProxy* BoneProxy = static_cast<const FBoneDebugSceneProxy*>(Proxy);
			for (const FWireLine& Line : BoneProxy->GetCachedLines())
			{
				DebugBoneLines.AddLine(Line.Start, Line.End, BoneProxy->GetBoneColor());
			}
			for (const FWireLine& Line : BoneProxy->GetCachedParentBoneLines())
			{
				DebugBoneLines.AddLine(Line.Start, Line.End, BoneProxy->GetParentBoneColor());
			}
		}
		else if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::WireShape))
		{
			if (bShowCollision)
			{
				const FShapeSceneProxy* ShapeProxy = static_cast<const FShapeSceneProxy*>(Proxy);
				const FVector4& Color = ShapeProxy->GetWireColor();
				for (const FWireLine& Line : ShapeProxy->GetCachedLines())
				{
					EditorLines.AddLine(Line.Start, Line.End, Color);
				}
			}
		}
		else if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::FontBatched))
		{
			const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
			if (!TextProxy->CachedText.empty())
				AddWorldText(TextProxy, Frame);
		}
		else if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::Decal))
		{
			BuildDecalCommands(Scene, Proxy, Frame, Output);
		}
		else if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::ParticleSystem))
		{
			BuildParticleCommands(Scene, static_cast<const FParticleSystemSceneProxy*>(Proxy));
		}
		else
		{
			BuildMeshCommands(Scene, Proxy);
		}

		if (Proxy->IsSelected())
			BuildSelectionCommands(Proxy, bShowBoundingVolume, Scene);
	}
}

// ============================================================
// BuildDecalCommands — Decal → Receiver 순회 + 커맨드 생성
// ============================================================
void FDrawCommandBuilder::BuildDecalCommands(FScene& Scene, FPrimitiveSceneProxy* Proxy, const FFrameContext& Frame, const FCollectOutput& Output)
{
	FDecalSceneProxy* DecalProxy = static_cast<FDecalSceneProxy*>(Proxy);

	for (FPrimitiveSceneProxy* ReceiverProxy : DecalProxy->GetReceiverProxies())
	{
		if (!ReceiverProxy || Output.VisibleProxySet.find(ReceiverProxy) == Output.VisibleProxySet.end())
			continue;

		UpdateProxyLOD(ReceiverProxy, Frame.LODContext);

		if (ReceiverProxy->HasProxyFlag(EPrimitiveProxyFlags::PerViewportUpdate))
			ReceiverProxy->UpdatePerViewport(Frame);

		BuildDecalCommandForReceiver(Scene, *ReceiverProxy, *DecalProxy);
	}
}

// ============================================================
// BuildMeshCommands — 일반 메시 (PreDepth + 메인 패스)
// ============================================================
void FDrawCommandBuilder::BuildMeshCommands(FScene& Scene, const FPrimitiveSceneProxy* Proxy)
{
	FProxyCommandBuildContext BuildCtx;
	if (!PrepareProxyCommandBuildContext(Scene, *Proxy, BuildCtx))
		return;

	for (const FMeshSectionDraw& Section : Proxy->GetSectionDraws())
	{
		ERenderPass SectionPass = ERenderPass::Opaque;

		if (Section.Material)
		{
			SectionPass = Section.Material->GetRenderPass();
		}

		if (SectionPass == ERenderPass::Opaque)
		{
			BuildCommandForSection(Scene, *Proxy, Section, ERenderPass::PreDepth, BuildCtx);
		}

		BuildCommandForSection(Scene, *Proxy, Section, SectionPass, BuildCtx);
	}
}

// ============================================================
// BuildParticleCommands
// ============================================================
void FDrawCommandBuilder::BuildParticleCommands(FScene& Scene, const FParticleSystemSceneProxy* Proxy)
{
	if (!Proxy) return;

	FConstantBuffer* PerObjCB = GetPerObjectCBForProxy(&Scene, *Proxy);
	if (PerObjCB && Proxy->NeedsPerObjectCBUpload())
	{
		PerObjCB->Update(CachedContext, &Proxy->GetPerObjectConstants(), sizeof(FPerObjectConstants));
		Proxy->ClearPerObjectCBDirty();
	}

	for (const FParticleDrawBatch& Batch : Proxy->GetParticleDrawBatches())
	{
		if (Batch.Sections.empty()) continue;

		FDrawCommandBuffer BatchBuffer;
		if (!Proxy->PrepareParticleDrawBuffer(Batch, CachedDevice, CachedContext, BatchBuffer)) continue;

		if (!BatchBuffer.HasBuffers()) continue;

		for (const FMeshSectionDraw& Section : Batch.Sections)
		{
			if (Section.IndexCount == 0) continue;

			ERenderPass SectionPass = ERenderPass::Opaque;
			if (Section.Material)
			{
				SectionPass = Section.Material->GetRenderPass();
			}

			if (SectionPass == ERenderPass::Opaque)
			{
				BuildParticleCommandForSection(Scene, *Proxy, BatchBuffer, Section, ERenderPass::PreDepth, PerObjCB);
			}

			BuildParticleCommandForSection(Scene, *Proxy, BatchBuffer, Section, SectionPass, PerObjCB);
		}
	}
}

// ============================================================
// BuildSelectionCommands — 아웃라인 + AABB
// ============================================================
void FDrawCommandBuilder::BuildSelectionCommands(FPrimitiveSceneProxy* Proxy, bool bShowBoundingVolume, FScene& Scene)
{
	if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::SupportsOutline))
		BuildCommandForProxy(Scene, *Proxy, ERenderPass::SelectionMask);

	if (bShowBoundingVolume && Proxy->HasProxyFlag(EPrimitiveProxyFlags::ShowAABB))
		Scene.AddDebugAABB(Proxy->GetCachedBounds().Min, Proxy->GetCachedBounds().Max, FColor::White());
}

bool FDrawCommandBuilder::PrepareProxyCommandBuildContext(FScene& Scene, const FPrimitiveSceneProxy& Proxy, FProxyCommandBuildContext& OutBuildCtx)
{
	const bool bSkeletal = Proxy.HasProxyFlag(EPrimitiveProxyFlags::SkeletalMesh);
	const bool bWeightBoneHeatMap = bSkeletal && bCollectWeightBoneHeatMap && CollectWeightBoneHeatMapBoneIndex >= 0;
	const bool bGPUSkinning = bSkeletal && (SkinningModeRuntime::Get() == ESkinningMode::GPU || bWeightBoneHeatMap);
	const FSkeletalMeshSceneProxy* SkeletalProxy = bSkeletal
		? static_cast<const FSkeletalMeshSceneProxy*>(&Proxy)
		: nullptr;
	FDrawCommandBuffer ProxyBuffer;
	if (bGPUSkinning)
	{
		if (!SkeletalProxy || !SkeletalProxy->PrepareGpuSkinningDrawBuffer(CachedDevice, CachedContext, ProxyBuffer)) return false;
	}
	else if (!Proxy.PrepareDrawBuffer(CachedDevice, CachedContext, ProxyBuffer))
	{
		return false;
	}
	if (!ProxyBuffer.HasBuffers()) return false;
	FConstantBuffer* PerObjCB = GetPerObjectCBForProxy(&Scene, Proxy);
	if (PerObjCB && Proxy.NeedsPerObjectCBUpload())
	{
		PerObjCB->Update(CachedContext, &Proxy.GetPerObjectConstants(), sizeof(FPerObjectConstants));
		Proxy.ClearPerObjectCBDirty();
	}
	OutBuildCtx.ProxyBuffer = ProxyBuffer;
	OutBuildCtx.PerObjCB = PerObjCB;
	OutBuildCtx.SkeletalProxy = SkeletalProxy;
	OutBuildCtx.bSkeletal = bSkeletal;
	OutBuildCtx.bGPUSkinning = bGPUSkinning;
	OutBuildCtx.bWeightBoneHeatMap = bWeightBoneHeatMap;
	if (bWeightBoneHeatMap)
	{
		FBoneHeatMapConstants BoneHeatMapConstants = {};
		BoneHeatMapConstants.SelectedBoneIndex = CollectWeightBoneHeatMapBoneIndex;
		BoneHeatMapCB.Update(CachedContext, &BoneHeatMapConstants, sizeof(FBoneHeatMapConstants));
	}
	return true;
}

void FDrawCommandBuilder::BuildCommandForSection(FScene& Scene, const FPrimitiveSceneProxy& Proxy, const FMeshSectionDraw& Section,
	ERenderPass Pass, const FProxyCommandBuildContext& BuildCtx)
{
	if (Section.IndexCount == 0) return;
	if (!BuildCtx.ProxyBuffer.IB) return;

	// Section Material이 셰이더를 가지면 사용, 없으면 Proxy 폴백
	FShader* SectionShader = (Section.Material && Section.Material->GetShader())
		? Section.Material->GetShader()
		: Proxy.GetShader();
	FShader* EffectiveShader = SelectEffectiveShader(SectionShader, CollectViewMode, BuildCtx.bGPUSkinning, false, BuildCtx.bWeightBoneHeatMap);

	const FDrawCommandRenderState BaseRenderState = PassRenderStateTable->ToDrawCommandState(Pass, CollectViewMode);

	FDrawCommand& Cmd = DrawCommandList.AddCommand();
	Cmd.Pass = Pass;
	Cmd.Shader = EffectiveShader;
	Cmd.RenderState = BaseRenderState;
	Cmd.Buffer = BuildCtx.ProxyBuffer;
	Cmd.PerObjectCB = BuildCtx.PerObjCB;
	Cmd.bIsSkeletal = BuildCtx.bSkeletal;
	Cmd.bIsGpuSkinned = BuildCtx.bGPUSkinning;
	Cmd.Buffer.FirstIndex = Section.FirstIndex;
	Cmd.Buffer.IndexCount = Section.IndexCount;
	Cmd.Bindings.SkinMatrixSRV = BuildCtx.bGPUSkinning && BuildCtx.SkeletalProxy
		? BuildCtx.SkeletalProxy->GetSkinMatrixSRV(CachedDevice, CachedContext)
		: nullptr;
	Cmd.Bindings.BoneHeatMapCB = BuildCtx.bWeightBoneHeatMap ? &BoneHeatMapCB : nullptr;

	if (Pass == ERenderPass::AlphaBlend)
	{
		Cmd.TranslucentSortPriority = Proxy.GetTranslucentSortPriority();

		const FVector ToObject = Proxy.GetCachedWorldPos() - CollectCameraPosition;
		Cmd.TranslucentSortDepth = ToObject.Dot(CollectCameraForward);
	}

	const bool bDepthOnly = (Pass == ERenderPass::PreDepth);

	if (!bDepthOnly && Section.Material)
	{
		UMaterialInterface* Mat = Section.Material;

		// dirty CB 업로드 (ConstantBufferMap + PerShaderOverride)
		Mat->FlushDirtyBuffers(CachedDevice, CachedContext);

		Cmd.Bindings.PerShaderCB[0] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader0);
		Cmd.Bindings.PerShaderCB[1] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader1);

		for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
			Cmd.Bindings.SRVs[s] = Mat->GetSRV(static_cast<EMaterialTextureSlot>(s));

		// 섹션별 Material의 RenderPass가 현재 Pass와 일치할 때만 렌더 상태 오버라이드
		if (Pass == Mat->GetRenderPass())
			ApplyMaterialRenderState(Cmd.RenderState, Mat, BaseRenderState);
	}

	Cmd.BuildSortKey();
}

void FDrawCommandBuilder::BuildParticleCommandForSection(FScene& Scene, const FParticleSystemSceneProxy& Proxy, const FDrawCommandBuffer& Buffer,
	const FMeshSectionDraw& Section, ERenderPass Pass, FConstantBuffer* PerObjCB)
{
	if (Section.IndexCount == 0) return;
	if (!Buffer.IB) return;

	const bool bInstanced = Buffer.IsInstanced();

	FShader* SectionShader = (Section.Material && Section.Material->GetShader())
		? Section.Material->GetShader()
		: Proxy.GetShader();

	FShader* EffectiveShader = SelectEffectiveShader(SectionShader, CollectViewMode, false, bInstanced, false);

	const FDrawCommandRenderState BaseRenderState = PassRenderStateTable->ToDrawCommandState(Pass, CollectViewMode);

	FDrawCommand& Cmd = DrawCommandList.AddCommand();
	Cmd.Pass = Pass;
	Cmd.Shader = EffectiveShader;
	Cmd.RenderState = BaseRenderState;
	Cmd.Buffer = Buffer;
	Cmd.PerObjectCB = PerObjCB;

	Cmd.Buffer.FirstIndex = Section.FirstIndex;
	Cmd.Buffer.IndexCount = Section.IndexCount;

	if (Pass == ERenderPass::AlphaBlend)
	{
		Cmd.TranslucentSortPriority = Proxy.GetTranslucentSortPriority();

		const FVector ToObject = Proxy.GetCachedWorldPos() - CollectCameraPosition;
		Cmd.TranslucentSortDepth = ToObject.Dot(CollectCameraForward);
	}

	const bool bDepthOnly = (Pass == ERenderPass::PreDepth);

	if (!bDepthOnly && Section.Material)
	{
		UMaterialInterface* Mat = Section.Material;

		// dirty CB 업로드 (ConstantBufferMap + PerShaderOverride)
		Mat->FlushDirtyBuffers(CachedDevice, CachedContext);

		Cmd.Bindings.PerShaderCB[0] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader0);
		Cmd.Bindings.PerShaderCB[1] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader1);

		for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
			Cmd.Bindings.SRVs[s] = Mat->GetSRV(static_cast<EMaterialTextureSlot>(s));

		// 섹션별 Material의 RenderPass가 현재 Pass와 일치할 때만 렌더 상태 오버라이드
		if (Pass == Mat->GetRenderPass())
			ApplyMaterialRenderState(Cmd.RenderState, Mat, BaseRenderState);
	}

	Cmd.BuildSortKey();
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
		Cmd.Pass = ERenderPass::EditorLines;
		Cmd.Shader = Shader;
		Cmd.RenderState = RS;
		Cmd.Buffer = { Lines.GetVBBuffer(), Lines.GetVBStride(), Lines.GetIBBuffer() };
		Cmd.Buffer.IndexCount = Lines.GetIndexCount();
		Cmd.BuildSortKey();
	}
}

// ============================================================
// BuildEditorLineCommands — EditorLines + GridLines
// ============================================================
void FDrawCommandBuilder::BuildEditorLineCommands(EViewMode ViewMode)
{
	FShader* EditorShader = FShaderManager::Get().GetOrCreate(EShaderPath::Editor);
	const FDrawCommandRenderState EditorLinesRS = PassRenderStateTable->ToDrawCommandState(ERenderPass::EditorLines, ViewMode);

	EmitLineCommand(EditorLines, EditorShader, EditorLinesRS);
	EmitLineCommand(GridLines, EditorShader, EditorLinesRS);

	FDrawCommandRenderState BoneLinesRS = EditorLinesRS;
	BoneLinesRS.DepthStencil = EDepthStencilState::NoDepth;

	EmitLineCommand(DebugBoneLines, EditorShader, BoneLinesRS);
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
	if (Frame.RenderOptions.ShowFlags.bFog && CollectScene && CollectScene->GetEnvironment().HasFog())
	{
		FShader* FogShader = FShaderManager::Get().GetOrCreate(EShaderPath::HeightFog);
		if (FogShader)
		{
			const FFogParams& FogParams = CollectScene->GetEnvironment().GetFogParams();
			FFogConstants fogData = {};
			fogData.InscatteringColor = FogParams.InscatteringColor;
			fogData.Density = FogParams.Density;
			fogData.HeightFalloff = FogParams.HeightFalloff;
			fogData.FogBaseHeight = FogParams.FogBaseHeight;
			fogData.StartDistance = FogParams.StartDistance;
			fogData.CutoffDistance = FogParams.CutoffDistance;
			fogData.MaxOpacity = FogParams.MaxOpacity;
			FogCB.Update(Ctx, &fogData, sizeof(FFogConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(FogShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = &FogCB;
			Cmd.BuildSortKey(0);
		}
	}

	// Outline (UserBits=1 → HeightFog 뒤)
	if (bHasSelectionMaskCommands)
	{
		FShader* PPShader = FShaderManager::Get().GetOrCreate(EShaderPath::Outline);
		if (PPShader)
		{
			FOutlinePostProcessConstants ppConstants;
			ppConstants.OutlineColor = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			ppConstants.OutlineThickness = 3.0f;
			OutlineCB.Update(Ctx, &ppConstants, sizeof(ppConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(PPShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = &OutlineCB;
			Cmd.BuildSortKey(1);
		}
	}

	// SceneDepth (UserBits=2 → Outline 뒤)
	if (CollectViewMode == EViewMode::SceneDepth)
	{
		FShader* DepthShader = FShaderManager::Get().GetOrCreate(EShaderPath::SceneDepth);
		if (DepthShader)
		{
			FViewportRenderOptions Opts = Frame.RenderOptions;
			FSceneDepthPConstants depthData = {};
			depthData.Exponent = Opts.Exponent;
			depthData.NearClip = Frame.NearClip;
			depthData.FarClip = Frame.FarClip;
			depthData.Mode = Opts.SceneDepthVisMode;
			SceneDepthCB.Update(Ctx, &depthData, sizeof(FSceneDepthPConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(DepthShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = &SceneDepthCB;
			Cmd.BuildSortKey(2);
		}
	}

	// WorldNormal (UserBits=3 → SceneDepth 뒤)
	if (CollectViewMode == EViewMode::WorldNormal)
	{
		FShader* NormalShader = FShaderManager::Get().GetOrCreate(EShaderPath::SceneNormal);
		if (NormalShader)
		{
			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(NormalShader, ERenderPass::PostProcess, PPRS);
			Cmd.BuildSortKey(3);
		}
	}

	// LightCulling (UserBits=4 → WorldNormal 뒤)
	if (CollectViewMode == EViewMode::LightCulling)
	{
		FShader* CullingShader = FShaderManager::Get().GetOrCreate(EShaderPath::LightCulling);
		if (CullingShader)
		{
			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(CullingShader, ERenderPass::PostProcess, PPRS);
			Cmd.BuildSortKey(4);
		}
	}

	// FXAA
	if (Frame.RenderOptions.ShowFlags.bFXAA)
	{
		FShader* FXAAShader = FShaderManager::Get().GetOrCreate(EShaderPath::FXAA);
		if (FXAAShader)
		{
			FViewportRenderOptions Opts = Frame.RenderOptions;
			FFXAAConstants FXAAData = {};
			FXAAData.EdgeThreshold = Opts.EdgeThreshold;
			FXAAData.EdgeThresholdMin = Opts.EdgeThresholdMin;
			FXAACB.Update(Ctx, &FXAAData, sizeof(FFXAAConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(FXAAShader, ERenderPass::FXAA,
				PassRenderStateTable->ToDrawCommandState(ERenderPass::FXAA, ViewMode));
			Cmd.Bindings.PerShaderCB[0] = &FXAACB;
			Cmd.BuildSortKey(0);
		}
	}

	// Camera Fade
	if (Frame.CameraFade.bEnabled && Frame.CameraFade.Amount > 0.0f)
	{
		FShader* FadeShader = FShaderManager::Get().GetOrCreate(EShaderPath::CameraFade);
		if (FadeShader)
		{
			FCameraFadeConstants FadeData = {};
			FadeData.FadeColor = Frame.CameraFade.Color.ToVector4();
			FadeData.FadeAmount = Frame.CameraFade.Amount;

			CameraFadeCB.Update(Ctx, &FadeData, sizeof(FCameraFadeConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(FadeShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = &CameraFadeCB;
			Cmd.BuildSortKey(5);
		}
	}

	// Camera Vignette
	if (Frame.CameraVignette.bEnabled && Frame.CameraVignette.Intensity > 0.0f)
	{
		FShader* VignetteShader = FShaderManager::Get().GetOrCreate(EShaderPath::CameraVignette);
		if (VignetteShader)
		{
			FCameraVignetteConstants VignetteData = {};
			VignetteData.VignetteColor = Frame.CameraVignette.Color.ToVector4();
			VignetteData.VignetteIntensity = Frame.CameraVignette.Intensity;
			VignetteData.VignetteRadius = Frame.CameraVignette.Radius;
			VignetteData.VignetteSoftness = Frame.CameraVignette.Softness;

			CameraVignetteCB.Update(Ctx, &VignetteData, sizeof(FCameraVignetteConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(VignetteShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = &CameraVignetteCB;
			Cmd.BuildSortKey(6);
		}
	}

	// Camera Letterbox
	if (Frame.CameraLetterbox.bEnabled && Frame.CameraLetterbox.Amount > 0.0f)
	{
		FShader* LetterboxShader = FShaderManager::Get().GetOrCreate(EShaderPath::CameraLetterbox);
		if (LetterboxShader)
		{
			FCameraLetterboxConstants LetterboxData = {};
			LetterboxData.LetterboxColor = Frame.CameraLetterbox.Color.ToVector4();
			LetterboxData.LetterboxAmount = Frame.CameraLetterbox.Amount;
			LetterboxData.LetterboxThickness = Frame.CameraLetterbox.Thickness;

			CameraLetterboxCB.Update(Ctx, &LetterboxData, sizeof(FCameraLetterboxConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(LetterboxShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = &CameraLetterboxCB;
			Cmd.BuildSortKey(7);
		}
	}

	if (Frame.RenderOptions.ShowFlags.bGammaCorrection)
	{
		FShader* GammaShader = FShaderManager::Get().GetOrCreate(EShaderPath::GammaCorrection);
		if (GammaShader)
		{
			FGammaCorrectionConstants GammaData = {};
			GammaData.Gamma = Frame.RenderOptions.Gamma;
			GammaCorrectionCB.Update(Ctx, &GammaData, sizeof(FGammaCorrectionConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(GammaShader, ERenderPass::GammaCorrection,
				PassRenderStateTable->ToDrawCommandState(ERenderPass::GammaCorrection, ViewMode));
			Cmd.Bindings.PerShaderCB[0] = &GammaCorrectionCB;
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
		Cmd.Pass = ERenderPass::AlphaBlend;
		Cmd.Shader = FShaderManager::Get().GetOrCreate(EShaderPath::Font);
		Cmd.RenderState = PassRenderStateTable->ToDrawCommandState(ERenderPass::AlphaBlend, ViewMode);
		Cmd.Buffer = { FontGeometry.GetWorldVBBuffer(), FontGeometry.GetWorldVBStride(), FontGeometry.GetWorldIBBuffer() };
		Cmd.Buffer.IndexCount = FontGeometry.GetWorldIndexCount();
		Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = FontRes->SRV;
		Cmd.BuildSortKey();
	}

	if (FontGeometry.GetScreenQuadCount() > 0 && FontGeometry.UploadScreenBuffers(Ctx))
	{
		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		Cmd.Pass = ERenderPass::OverlayFont;
		Cmd.Shader = FShaderManager::Get().GetOrCreate(EShaderPath::OverlayFont);
		Cmd.RenderState = PassRenderStateTable->ToDrawCommandState(ERenderPass::OverlayFont, ViewMode);
		Cmd.Buffer = { FontGeometry.GetScreenVBBuffer(), FontGeometry.GetScreenVBStride(), FontGeometry.GetScreenIBBuffer() };
		Cmd.Buffer.IndexCount = FontGeometry.GetScreenIndexCount();
		Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = FontRes->SRV;
		Cmd.BuildSortKey();
	}
}

// ============================================================
// PerObjectCB 풀 관리
// ============================================================
void FDrawCommandBuilder::EnsurePerObjectCBPoolCapacity(FScene* Scene, uint32 RequiredCount)
{
	if (!Scene) return;

	TArray<FConstantBuffer>& Pool = PerSceneObjectCBPool[Scene];

	if (Pool.size() >= RequiredCount) return;

	const size_t OldCount = Pool.size();
	Pool.resize(RequiredCount);

	for (size_t Index = OldCount; Index < Pool.size(); ++Index)
	{
		Pool[Index].Create(CachedDevice, sizeof(FPerObjectConstants), "PerObjectCB");
	}
}

FConstantBuffer* FDrawCommandBuilder::GetPerObjectCBForProxy(FScene* Scene, const FPrimitiveSceneProxy& Proxy)
{
	if (!Scene || Proxy.GetProxyId() == UINT32_MAX) return nullptr;

	EnsurePerObjectCBPoolCapacity(Scene, Proxy.GetProxyId() + 1);
	return &PerSceneObjectCBPool[Scene][Proxy.GetProxyId()];
}
