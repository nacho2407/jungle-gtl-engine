#include "Render/Proxy/TextRenderSceneProxy.h"
#include "Component/TextRenderComponent.h"
#include "Render/Pipeline/RenderBus.h"

// ============================================================
// FTextRenderSceneProxy
// ============================================================
FTextRenderSceneProxy::FTextRenderSceneProxy(UTextRenderComponent* InComponent)
	: FBillboardSceneProxy(static_cast<UBillboardComponent*>(InComponent))
{
	bBatcherRendered = true; // 렌더링은 FontBatcher 경유, 프록시는 SelectionMask 전용
}

void FTextRenderSceneProxy::UpdateMesh()
{
	// Billboard::UpdateMesh의 CachedTexture 기반 분기를 피하고 FontBatcher 경로만 유지한다.
	MeshBuffer = Owner->GetMeshBuffer();
	Shader = nullptr;
	Pass = ERenderPass::Font;
	bBatcherRendered = true;
	UpdateSortKey();
}

UTextRenderComponent* FTextRenderSceneProxy::GetTextRenderComponent() const
{
	return static_cast<UTextRenderComponent*>(Owner);
}

// ============================================================
// UpdatePerViewport — 빌보드 행렬 계산 + SelectionMask용 아웃라인
// ============================================================
void FTextRenderSceneProxy::UpdatePerViewport(const FRenderBus& Bus)
{
	UTextRenderComponent* TextComp = GetTextRenderComponent();

	// 텍스트/폰트 미설정 시 비가시
	if (TextComp->GetText().empty() || !TextComp->GetFont() || !TextComp->GetFont()->IsLoaded())
	{
		bVisible = false;
		return;
	}

	if (!Bus.GetShowFlags().bBillboardText)
	{
		bVisible = false;
		return;
	}

	bVisible = TextComp->IsVisible();
	if (!bVisible) return;

	// 빌보드 행렬 (CollectEntries에서도 사용)
	FVector BillboardForward = Bus.GetCameraForward() * -1.0f;
	FMatrix RotMatrix;
	RotMatrix.SetAxes(BillboardForward, Bus.GetCameraRight() * -1.0f, Bus.GetCameraUp());
	CachedBillboardMatrix = FMatrix::MakeScaleMatrix(TextComp->GetWorldScale())
		* RotMatrix * FMatrix::MakeTranslationMatrix(TextComp->GetWorldLocation());

	// SelectionMask용 아웃라인 행렬 (텍스트 너비·높이 반영)
	FMatrix OutlineMatrix = TextComp->CalculateOutlineMatrix(CachedBillboardMatrix);
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(OutlineMatrix);
	MarkPerObjectCBDirty();
}

// ============================================================
// CollectEntries — FontBatcher용 FFontEntry 생성
// ============================================================
void FTextRenderSceneProxy::CollectEntries(FRenderBus& Bus)
{
	UTextRenderComponent* TextComp = GetTextRenderComponent();

	FFontEntry Entry = {};
	Entry.PerObject = FPerObjectConstants{ CachedBillboardMatrix };
	Entry.PerObject.Color = TextComp->GetColor();
	Entry.Font.Text = TextComp->GetText();
	Entry.Font.Font = TextComp->GetFont();
	Entry.Font.Scale = TextComp->GetFontSize();
	Bus.AddFontEntry(std::move(Entry));
}
