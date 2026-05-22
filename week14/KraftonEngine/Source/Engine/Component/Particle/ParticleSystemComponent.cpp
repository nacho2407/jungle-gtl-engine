#include "ParticleSystemComponent.h"

#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Particles/Runtime/ParticleRuntimeTypes.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"

#include <cstring>

UParticleSystemComponent::~UParticleSystemComponent()
{
	ClearEmitterInstances();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	if (ParticleSystem.Get() == InTemplate)
	{
		return;
	}

	ParticleSystem = InTemplate;
	ParticleSystemPath = InTemplate ? InTemplate->GetAssetPathFileName() : "None";
	ParticleSystemPath.SetCachedObject(InTemplate);
	ResetSystem();
	MarkRenderStateDirty();
}

void UParticleSystemComponent::ResetSystem()
{
	ClearEmitterInstances();
	InitializeEmitterInstances();
}

void UParticleSystemComponent::Activate()
{
	UPrimitiveComponent::Activate();

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->SetActive(true);
		}
	}
}

void UParticleSystemComponent::Deactivate()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->SetActive(false);
		}
	}

	UPrimitiveComponent::Deactivate();
}

void UParticleSystemComponent::EndPlay()
{
	ClearEmitterInstances();
	UPrimitiveComponent::EndPlay();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

void UParticleSystemComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();

	if (!ParticleSystemPath.IsNull())
	{
		LoadTemplateFromPath();
	}

	if (ParticleSystem)
	{
		ParticleSystemPath.SetCachedObject(ParticleSystem.Get());
		ResetSystem();
		MarkRenderStateDirty();
	}
}

void UParticleSystemComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "ParticleSystemPath") == 0 || strcmp(PropertyName, "Particle System") == 0)
	{
		if (ParticleSystemPath.IsNull())
		{
			SetTemplate(nullptr);
		}
		else
		{
			LoadTemplateFromPath();
		}
	}
}

void UParticleSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsActive())
	{
		return;
	}

	bool bAnyEmitterTicked = false;
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance || !Instance->IsActive())
		{
			continue;
		}

		Instance->Tick(DeltaTime);
		bAnyEmitterTicked = true;
	}

	if (bAnyEmitterTicked)
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

void UParticleSystemComponent::InitializeEmitterInstances()
{
	if (!ParticleSystem)
	{
		return;
	}

	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	EmitterInstances.reserve(Emitters.size());

	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter)
		{
			continue;
		}

		if (FParticleEmitterInstance* Instance = Emitter->CreateInstance(this))
		{
			EmitterInstances.push_back(Instance);
		}
	}
}

void UParticleSystemComponent::LoadTemplateFromPath()
{
	if (ParticleSystemPath.IsNull())
	{
		return;
	}

	UParticleSystem* LoadedTemplate = FParticleSystemManager::Get().Load(ParticleSystemPath.ToString());
	if (!LoadedTemplate)
	{
		return;
	}

	if (ParticleSystem.Get() == LoadedTemplate)
	{
		ParticleSystemPath.SetCachedObject(LoadedTemplate);
		return;
	}

	ParticleSystem = LoadedTemplate;
	ParticleSystemPath.SetCachedObject(LoadedTemplate);
	ResetSystem();
	MarkRenderStateDirty();
}

void UParticleSystemComponent::ClearEmitterInstances()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		delete Instance;
	}
	EmitterInstances.clear();
}
