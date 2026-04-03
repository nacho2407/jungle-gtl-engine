#include "Render/Pipeline/BillboardSceneProxy.h"
#include "Component/BillboardComponent.h"
#include "Render/Resource/ShaderManager.h"

// ============================================================
// FBillboardSceneProxy
// ============================================================
FBillboardSceneProxy::FBillboardSceneProxy(UBillboardComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	bPerViewportUpdate = true;
}

UBillboardComponent* FBillboardSceneProxy::GetBillboardComponent() const
{
	return static_cast<UBillboardComponent*>(Owner);
}

// ============================================================
// UpdateMesh — Quad 메시 + Primitive 셰이더 캐싱
// ============================================================
void FBillboardSceneProxy::UpdateMesh()
{
	MeshBuffer = Owner->GetMeshBuffer();
	Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
	Pass = ERenderPass::Opaque;
}
