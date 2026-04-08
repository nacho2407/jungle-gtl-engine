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
