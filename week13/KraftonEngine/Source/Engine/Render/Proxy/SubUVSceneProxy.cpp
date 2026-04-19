#include "Render/Proxy/SubUVSceneProxy.h"
#include "Component/SubUVComponent.h"
#include "Render/Pipeline/FrameContext.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/MeshBufferManager.h"

// ============================================================
// FSubUVSceneProxy
// ============================================================
FSubUVSceneProxy::FSubUVSceneProxy(USubUVComponent* InComponent)
	: FBillboardSceneProxy(static_cast<UBillboardComponent*>(InComponent))
{
	ProxyFlags &= ~EPrimitiveProxyFlags::ShowAABB;
}

FSubUVSceneProxy::~FSubUVSceneProxy()
{
	UVRegionCB.Release();
}

void FSubUVSceneProxy::UpdateMesh()
{
	USubUVComponent* Comp = GetSubUVComponent();

	// TexturedQuad (FVertexPNCT with UVs) for rendering
	MeshBuffer = &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::TexturedQuad);
	Shader = FShaderManager::Get().GetShader(EShaderType::SubUV);
	Pass = ERenderPass::AlphaBlend;

	// ExtraCB bind (UV region, b2 slot) — 실제 GPU 버퍼는 Renderer에서 lazy 생성
	ExtraCB.Bind<FSubUVRegionConstants>(&UVRegionCB, ECBSlot::PerShader0);

	// Particle/FrameIndex 캐싱
	CachedParticle = Comp->GetParticle();
	CachedFrameIndex = Comp->GetFrameIndex();

	// Set DiffuseSRV from particle resource
	if (CachedParticle && CachedParticle->IsLoaded())
	{
		DiffuseSRV = CachedParticle->SRV;
	}
}

void FSubUVSceneProxy::UpdateMaterial()
{
	// TickComponent에서 FrameIndex 변경 시 Material dirty로 호출됨
	USubUVComponent* Comp = GetSubUVComponent();
	CachedFrameIndex = Comp->GetFrameIndex();
	CachedParticle = Comp->GetParticle();

	if (CachedParticle && CachedParticle->IsLoaded())
	{
		DiffuseSRV = CachedParticle->SRV;
	}
}

void FSubUVSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (!bVisible) return;

	if (!CachedParticle || !CachedParticle->IsLoaded())
	{
		bVisible = false;
		return;
	}

	// Billboard matrix
	FVector BillboardForward = Frame.CameraForward * -1.0f;
	FMatrix RotMatrix;
	RotMatrix.SetAxes(BillboardForward, Frame.CameraRight, Frame.CameraUp);
	FMatrix BillboardMatrix = FMatrix::MakeScaleMatrix(CachedScale)
		* RotMatrix * FMatrix::MakeTranslationMatrix(CachedLocation);

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
	MarkPerObjectCBDirty();

	// Update UV region from cached frame index
	const uint32 Cols = CachedParticle->Columns;
	const uint32 Rows = CachedParticle->Rows;
	if (Cols > 0 && Rows > 0)
	{
		const float FrameW = 1.0f / static_cast<float>(Cols);
		const float FrameH = 1.0f / static_cast<float>(Rows);
		const uint32 Col = CachedFrameIndex % Cols;
		const uint32 Row = CachedFrameIndex / Cols;

		FSubUVRegionConstants& Region = ExtraCB.As<FSubUVRegionConstants>();
		Region.U = Col * FrameW;
		Region.V = Row * FrameH;
		Region.Width = FrameW;
		Region.Height = FrameH;
	}
}

USubUVComponent* FSubUVSceneProxy::GetSubUVComponent() const
{
	return static_cast<USubUVComponent*>(GetOwner());
}
