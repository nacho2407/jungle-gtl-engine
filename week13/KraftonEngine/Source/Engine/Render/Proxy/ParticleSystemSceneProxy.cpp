#include "ParticleSystemSceneProxy.h"

#include "Component/Primitive/ParticleSystemComponent.h"

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent) : FPrimitiveSceneProxy(
    InComponent
)
{
    ProxyFlags |= EPrimitiveProxyFlags::Particle;
}

void FParticleSystemSceneProxy::UpdateTransform()
{
    FPrimitiveSceneProxy::UpdateTransform();
}

void FParticleSystemSceneProxy::UpdateMesh()
{
    MeshBuffer = nullptr;
    SectionDraws.clear();
}

void FParticleSystemSceneProxy::UpdateMaterial()
{
    SectionDraws.clear();
}

UParticleSystemComponent* FParticleSystemSceneProxy::GetParticleSystemComponent() const
{
    return static_cast<UParticleSystemComponent*>(GetOwner());
}
