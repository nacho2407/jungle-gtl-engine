#include "Render/Proxy/SubUVSceneProxy.h"
#include "Component/SubUVComponent.h"
#include "Render/Pipeline/RenderBus.h"

// ============================================================
// FSubUVSceneProxy
// ============================================================
FSubUVSceneProxy::FSubUVSceneProxy(USubUVComponent* InComponent)
	: FBillboardSceneProxy(static_cast<UBillboardComponent*>(InComponent))
{
	bBatcherRendered = true;
	bShowAABB = false;
}

void FSubUVSceneProxy::UpdateMesh()
{
	// Billboard::UpdateMesh는 base Billboard의 CachedTexture 유무에 따라 batcher/primitive 경로를
	// 분기시키는데, SubUV는 자체 ParticleResource를 사용하므로 그 분기가 불리하다.
	// 여기서는 SubUV batcher 경로만 강제한다.
	MeshBuffer = Owner->GetMeshBuffer();
	Shader = nullptr;
	Pass = ERenderPass::SubUV;
	bBatcherRendered = true;
	UpdateSortKey();
}

USubUVComponent* FSubUVSceneProxy::GetSubUVComponent() const
{
	return static_cast<USubUVComponent*>(Owner);
}

// ============================================================
// CollectEntries — SubUVBatcher용 FSubUVEntry 생성
// ============================================================
void FSubUVSceneProxy::CollectEntries(FRenderBus& Bus)
{
	USubUVComponent* Comp = GetSubUVComponent();

	const FParticleResource* Particle = Comp->GetParticle();
	if (!Particle || !Particle->IsLoaded()) return;

	FSubUVEntry Entry = {};
	Entry.PerObject = PerObjectConstants;
	Entry.SubUV.Particle = Particle;
	Entry.SubUV.FrameIndex = Comp->GetFrameIndex();
	Entry.SubUV.Width = Comp->GetWidth();
	Entry.SubUV.Height = Comp->GetHeight();
	Bus.AddSubUVEntry(std::move(Entry));
}
