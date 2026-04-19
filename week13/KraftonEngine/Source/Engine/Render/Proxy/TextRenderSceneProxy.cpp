#include "Render/Proxy/TextRenderSceneProxy.h"
#include "Component/TextRenderComponent.h"
#include "Render/Pipeline/FrameContext.h"
#include "Render/Resource/ShaderManager.h"

// ============================================================
// FTextRenderSceneProxy
// ============================================================
FTextRenderSceneProxy::FTextRenderSceneProxy(UTextRenderComponent* InComponent)
	: FBillboardSceneProxy(static_cast<UBillboardComponent*>(InComponent))
{
}

void FTextRenderSceneProxy::UpdateTransform()
{
	FBillboardSceneProxy::UpdateTransform();
}

void FTextRenderSceneProxy::UpdateMesh()
{
	// SelectionMask 아웃라인 패스에서 사용할 mesh/shader
	MeshBuffer = GetOwner()->GetMeshBuffer();
	Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
	Pass = ERenderPass::AlphaBlend;
	ProxyFlags |= EPrimitiveProxyFlags::FontBatched;

	// 텍스트/폰트 데이터 캐싱 (UpdatePerViewport에서 Owner 접근 제거)
	UTextRenderComponent* TextComp = GetTextRenderComponent();
	CachedText = TextComp->GetText();
	CachedFontScale = TextComp->GetFontSize();
	CachedFont = TextComp->GetFont();
	CachedCharWidth = TextComp->GetCharWidth();
	CachedCharHeight = TextComp->GetCharHeight();
}

UTextRenderComponent* FTextRenderSceneProxy::GetTextRenderComponent() const
{
	return static_cast<UTextRenderComponent*>(GetOwner());
}

// ============================================================
// UpdatePerViewport — 빌보드 행렬 계산 + 아웃라인 행렬 갱신
// ============================================================
void FTextRenderSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	// 텍스트/폰트 미설정 시 비가시
	if (CachedText.empty() || !CachedFont || !CachedFont->IsLoaded())
	{
		bVisible = false;
		return;
	}

	if (!Frame.RenderOptions.ShowFlags.bBillboardText)
	{
		bVisible = false;
		return;
	}

	if (!bVisible) return;

	// 빌보드 행렬
	FVector BillboardForward = Frame.CameraForward * -1.0f;
	FMatrix RotMatrix;
	RotMatrix.SetAxes(BillboardForward, Frame.CameraRight * -1.0f, Frame.CameraUp);
	CachedBillboardMatrix = FMatrix::MakeScaleMatrix(CachedScale)
		* RotMatrix * FMatrix::MakeTranslationMatrix(CachedLocation);

	// SelectionMask용 아웃라인 행렬 (캐싱된 CharWidth/CharHeight로 직접 계산)
	int32 Len = 0;
	for (size_t i = 0; i < CachedText.length(); ++i)
	{
		if ((CachedText[i] & 0xC0) != 0x80) Len++;
	}

	if (Len > 0)
	{
		float TotalLocalWidth = Len * CachedCharWidth;
		float CenterY = TotalLocalWidth * -0.5f;

		FMatrix ScaleMatrix = FMatrix::MakeScaleMatrix(FVector(1.0f, TotalLocalWidth, CachedCharHeight));
		FMatrix TransMatrix = FMatrix::MakeTranslationMatrix(FVector(0.0f, CenterY, 0.0f));

		FMatrix OutlineMatrix = (ScaleMatrix * TransMatrix) * CachedBillboardMatrix;
		PerObjectConstants = FPerObjectConstants::FromWorldMatrix(OutlineMatrix);
	}
	else
	{
		PerObjectConstants = FPerObjectConstants::FromWorldMatrix(FMatrix::Identity);
	}
	MarkPerObjectCBDirty();
}
