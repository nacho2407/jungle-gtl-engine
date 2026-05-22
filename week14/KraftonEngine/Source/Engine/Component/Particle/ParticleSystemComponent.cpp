#include "ParticleSystemComponent.h"

#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Particles/Render/ParticleSystemSceneProxy.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Particles/Runtime/ParticleRuntimeTypes.h"

#include <cstring>

UParticleSystemComponent::~UParticleSystemComponent()
{
	ClearEmitterRenderData();
	ClearEmitterInstances();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	if (ParticleSystemTemplate.Get() == InTemplate)
	{
		return;
	}

	ParticleSystemTemplate = InTemplate;
	ParticleSystemTemplatePath = InTemplate ? InTemplate->GetAssetPathFileName() : "None";
	ParticleSystemTemplatePath.SetCachedObject(InTemplate);
	ResetSystem();
	MarkRenderStateDirty();
}

void UParticleSystemComponent::ResetSystem()
{
	ClearEmitterRenderData();
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
	ClearEmitterRenderData();
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
	LoadTemplateFromPath();
}

void UParticleSystemComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "ParticleSystemTemplatePath") == 0 || strcmp(PropertyName, "Particle System") == 0)
	{
		LoadTemplateFromPath();
	}
}

void UParticleSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsActive())
	{
		return;
	}

	ClearEmitterRenderData();

	bool bAnyEmitterTicked = false;
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance || !Instance->IsActive())
		{
			continue;
		}

		Instance->Tick(DeltaTime);
		if (FDynamicEmitterDataBase* RenderData = Instance->BuildRenderData())
		{
			EmitterRenderData.push_back(RenderData);
		}
		bAnyEmitterTicked = true;
	}

	if (bAnyEmitterTicked)
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

void UParticleSystemComponent::InitializeEmitterInstances()
{
	if (!ParticleSystemTemplate)
	{
		return;
	}

	const TArray<UParticleEmitter*>& Emitters = ParticleSystemTemplate->GetEmitters();
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
	if (ParticleSystemTemplatePath.IsNull())
	{
		SetTemplate(nullptr);
		return;
	}

	UParticleSystem* LoadedTemplate = FParticleSystemManager::Get().Load(ParticleSystemTemplatePath.ToString());
	if (ParticleSystemTemplate.Get() == LoadedTemplate)
	{
		ParticleSystemTemplatePath.SetCachedObject(LoadedTemplate);
		return;
	}

	ParticleSystemTemplate = LoadedTemplate;
	ParticleSystemTemplatePath.SetCachedObject(LoadedTemplate);
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

void UParticleSystemComponent::ClearEmitterRenderData()
{
	for (FDynamicEmitterDataBase* RenderData : EmitterRenderData)
	{
		delete RenderData;
	}
	EmitterRenderData.clear();
}
