#include "SubUVComponent.h"

#include "Core/ResourceManager.h"

DEFINE_CLASS(USubUVComponent, UPrimitiveComponent)
REGISTER_FACTORY(USubUVComponent)

void USubUVComponent::SetParticle(const FName& InParticleName)
{
	ParticleName   = InParticleName;
	CachedParticle = FResourceManager::Get().FindParticle(InParticleName);
}

void USubUVComponent::TickComponent(float DeltaTime)
{
	if (!CachedParticle) return;

	const uint32 TotalFrames = CachedParticle->Columns * CachedParticle->Rows;
	if (TotalFrames == 0) return;

	TimeAccumulator += DeltaTime;
	const float FrameDuration = 1.0f / FrameRate;
	while (TimeAccumulator >= FrameDuration)
	{
		TimeAccumulator -= FrameDuration;
		FrameIndex = (FrameIndex + 1) % TotalFrames;
	}
}

