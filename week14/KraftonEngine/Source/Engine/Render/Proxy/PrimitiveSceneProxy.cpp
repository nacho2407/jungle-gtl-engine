#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Component/PrimitiveComponent.h"
#include "Render/Resource/ShaderManager.h"

// ============================================================
// FPrimitiveSceneProxy — 기본 구현
// ============================================================
FPrimitiveSceneProxy::FPrimitiveSceneProxy(UPrimitiveComponent* InComponent)
	: Owner(InComponent)
{
}

void FPrimitiveSceneProxy::UpdateTransform()
{
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(Owner->GetWorldMatrix());
}

void FPrimitiveSceneProxy::UpdateMaterial()
{
	// 기본 PrimitiveComponent는 섹션별 머티리얼이 없음 — 서브클래스에서 오버라이드
}

void FPrimitiveSceneProxy::UpdateVisibility()
{
	bVisible = Owner->IsVisible();
}

void FPrimitiveSceneProxy::UpdateMesh()
{
	MeshBuffer = Owner->GetMeshBuffer();
	Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
	Pass = ERenderPass::Opaque;
}
