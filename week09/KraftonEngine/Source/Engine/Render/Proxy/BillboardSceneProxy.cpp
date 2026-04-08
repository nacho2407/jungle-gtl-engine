#include "Render/Proxy/BillboardSceneProxy.h"
#include "Component/BillboardComponent.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Pipeline/RenderBus.h"
#include "Render/Pipeline/RenderConstants.h"

// ============================================================
// FBillboardSceneProxy
// ============================================================
FBillboardSceneProxy::FBillboardSceneProxy(UBillboardComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	bPerViewportUpdate = true;
	bShowAABB = false;
	// 텍스처가 세팅돼 있으면 SubUV batcher 경로를 사용한다 (1x1 atlas로 단일 텍스처 렌더링).
	// 텍스처가 없으면 기존 Primitive 셰이더 경로 유지.
	bBatcherRendered = (InComponent && InComponent->GetTexture() != nullptr);
}

UBillboardComponent* FBillboardSceneProxy::GetBillboardComponent() const
{
	return static_cast<UBillboardComponent*>(Owner);
}

// ============================================================
// UpdateMesh — Quad 메시 캐싱 + 텍스처 유무에 따라 batcher/Primitive 경로 결정
// ============================================================
void FBillboardSceneProxy::UpdateMesh()
{
	MeshBuffer = Owner->GetMeshBuffer();

	UBillboardComponent* Comp = GetBillboardComponent();
	const bool bHasTexture = (Comp && Comp->GetTexture() != nullptr);
	bBatcherRendered = bHasTexture;

	if (bHasTexture)
	{
		// SubUV batcher가 자체 셰이더를 사용 — 여기서는 Pass만 SubUV로 분류.
		Shader = nullptr;
		Pass = ERenderPass::SubUV;
	}
	else
	{
		Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
		Pass = ERenderPass::Opaque;
	}
	UpdateSortKey();
}

// ============================================================
// CollectEntries — 텍스처가 있을 때만 SubUV batcher에 단일 프레임 엔트리 제출
// ============================================================
void FBillboardSceneProxy::CollectEntries(FRenderBus& Bus)
{
	UBillboardComponent* Comp = GetBillboardComponent();
	if (!Comp) return;

	const FTextureResource* Texture = Comp->GetTexture();
	if (!Texture || !Texture->IsLoaded()) return;

	FSubUVEntry Entry = {};
	Entry.PerObject = PerObjectConstants;
	Entry.SubUV.Particle = Texture;	// 1x1 atlas — 단일 프레임 텍스처
	Entry.SubUV.FrameIndex = 0;
	Entry.SubUV.Width = Comp->GetWidth();
	Entry.SubUV.Height = Comp->GetHeight();
	Bus.AddSubUVEntry(std::move(Entry));
}

// ============================================================
// UpdatePerViewport — 뷰포트 카메라 기반 빌보드 행렬 갱신
// ============================================================
void FBillboardSceneProxy::UpdatePerViewport(const FRenderBus& Bus)
{
	UBillboardComponent* Comp = GetBillboardComponent();
	bVisible = Comp->IsVisible();
	if (!bVisible) return;

	// Bus 카메라 벡터로 per-view 빌보드 행렬 계산
	FVector BillboardForward = Bus.GetCameraForward() * -1.0f;
	FMatrix RotMatrix;
	RotMatrix.SetAxes(BillboardForward, Bus.GetCameraRight() * -1.0f, Bus.GetCameraUp());
	FMatrix BillboardMatrix = FMatrix::MakeScaleMatrix(Comp->GetWorldScale())
		* RotMatrix * FMatrix::MakeTranslationMatrix(Comp->GetWorldLocation());

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
	MarkPerObjectCBDirty();
}
