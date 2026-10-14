#include "ParticleSystemComponent.h"

#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Particles/Runtime/ParticleRuntimeTypes.h"
#include "Render/Proxy/ParticleSystemSceneProxy.h"
#include "Render/Types/MinimalViewInfo.h"
#include "GameFramework/World.h"

#include <algorithm>
#include <cmath>
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

void UParticleSystemComponent::SetEmitterSpawningEnabled(bool bEnabled)
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->SetSpawningEnabled(bEnabled);
		}
	}
}

void UParticleSystemComponent::SetParticleScaleMultiplier(float InScale)
{
	const float NewScale = (std::max)(0.0f, InScale);
	if (std::fabs(ParticleScaleMultiplier - NewScale) <= 0.0001f)
	{
		return;
	}

	ParticleScaleMultiplier = NewScale;
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UParticleSystemComponent::SetVectorParameter(const FName& ParameterName, const FVector& Value)
{
	if (!ParameterName.IsValid() || ParameterName == FName::None)
	{
		return;
	}

	for (FParticleSysParam& Parameter : InstanceParameters)
	{
		if (Parameter.Name == ParameterName)
		{
			Parameter.Vector = Value;
			MarkProxyDirty(EDirtyFlag::Mesh);
			return;
		}
	}

	FParticleSysParam NewParameter;
	NewParameter.Name = ParameterName;
	NewParameter.Vector = Value;
	InstanceParameters.push_back(NewParameter);
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UParticleSystemComponent::SetVectorParameter(const FString& ParameterName, const FVector& Value)
{
	SetVectorParameter(FName(ParameterName), Value);
}

bool UParticleSystemComponent::GetVectorParameter(const FName& ParameterName, FVector& OutValue) const
{
	if (!ParameterName.IsValid() || ParameterName == FName::None)
	{
		return false;
	}

	for (const FParticleSysParam& Parameter : InstanceParameters)
	{
		if (Parameter.Name == ParameterName)
		{
			OutValue = Parameter.Vector;
			return true;
		}
	}

	return false;
}

bool UParticleSystemComponent::GetVectorParameter(const FString& ParameterName, FVector& OutValue) const
{
	return GetVectorParameter(FName(ParameterName), OutValue);
}

void UParticleSystemComponent::ResetSystem()
{
	CurrentLODIndex = 0;
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
	else if (strcmp(PropertyName, "ParticleScaleMultiplier") == 0 || strcmp(PropertyName, "Particle Scale Multiplier") == 0)
	{
		ParticleScaleMultiplier = (std::max)(0.0f, ParticleScaleMultiplier);
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

void UParticleSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsActive())
	{
		return;
	}

	if (ParticleSystem && PreviewLODIndex >= 0)
	{
		const int32 LODCount = ParticleSystem->GetLODCount();
		CurrentLODIndex = LODCount > 0
			? (PreviewLODIndex < LODCount ? PreviewLODIndex : LODCount - 1)
			: 0;
	}
	else
	{
		FMinimalViewInfo POV;
		const bool bHasViewLocation = GetWorld() && GetWorld()->GetActivePOV(POV);
		if (bHasViewLocation && ParticleSystem)
		{
			const float Distance = FVector::Distance(GetWorldLocation(), POV.Location);
			CurrentLODIndex = ParticleSystem->SelectLODLevelIndex(Distance);
		}
	}

	bool bAnyEmitterTicked = false;
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance || !Instance->IsActive())
		{
			continue;
		}

		Instance->SetLODLevelIndex(CurrentLODIndex);

		const UParticleLODLevel* CurrentLODLevel = Instance->GetCurrentLODLevel();
		if (!CurrentLODLevel || !CurrentLODLevel->IsEnabled())
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

void UParticleSystemComponent::SetPreviewLODIndex(int32 InLODIndex)
{
	PreviewLODIndex = InLODIndex >= 0 ? InLODIndex : -1;
}

void UParticleSystemComponent::SetPreviewSoloEmitterIndex(int32 InEmitterIndex)
{
	PreviewSoloEmitterIndex = InEmitterIndex >= 0 ? InEmitterIndex : -1;
}

bool UParticleSystemComponent::ShouldCreateEmitterInstance(int32 EmitterIndex, const UParticleEmitter* Emitter) const
{
	if (!Emitter)
	{
		return false;
	}

	return PreviewSoloEmitterIndex < 0 || PreviewSoloEmitterIndex == EmitterIndex;
}

void UParticleSystemComponent::BroadcastParticleEvent(const FParticleCollisionEventPayload& Event)
{
	OnParticleEvent.Broadcast(this, Event);
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance && Instance->IsActive())
		{
			Instance->ReceiveParticleEvent(Event);
		}
	}

	if (Event.EventType == EParticleEventType::Collision)
	{
		BroadcastParticleCollisionEvent(Event);
	}
}

void UParticleSystemComponent::BroadcastParticleCollisionEvent(const FParticleCollisionEventPayload& Event)
{
	OnParticleCollideEvent.Broadcast(this, Event);
}

void UParticleSystemComponent::InitializeEmitterInstances()
{
	if (!ParticleSystem)
	{
		return;
	}

	const TArray<UParticleEmitter*>& Emitters = ParticleSystem->GetEmitters();
	EmitterInstances.reserve(Emitters.size());

	for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Emitters.size()); ++EmitterIndex)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if (!ShouldCreateEmitterInstance(EmitterIndex, Emitter))
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
