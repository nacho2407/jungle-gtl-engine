#include "Render/Scene/SceneEnvironment.h"
#include <algorithm>

// ============================================================
// Fog
// ============================================================
void FSceneEnvironment::AddFog(const UHeightFogComponent* Owner, const FFogParams& Params)
{
	for (auto& Entry : Fogs)
	{
		if (Entry.Owner == Owner)
		{
			Entry.Params = Params;
			return;
		}
	}
	Fogs.push_back({ Owner, Params });
}

void FSceneEnvironment::RemoveFog(const UHeightFogComponent* Owner)
{
	Fogs.erase(
		std::remove_if(Fogs.begin(), Fogs.end(),
			[Owner](const FFogEntry& E) { return E.Owner == Owner; }),
		Fogs.end());
}

// ============================================================
// Global Ambient Light (배열 관리, 렌더링은 [0]만 사용)
// ============================================================
void FSceneEnvironment::AddGlobalAmbientLight(const UAmbientLightComponent* Owner, const FGlobalAmbientLightParams& Params)
{
	for (auto& Entry : AmbientLights)
	{
		if (Entry.Owner == Owner)
		{
			Entry.Params = Params;
			return;
		}
	}
	AmbientLights.push_back({ Owner, Params });
}

void FSceneEnvironment::RemoveGlobalAmbientLight(const UAmbientLightComponent* Owner)
{
	AmbientLights.erase(
		std::remove_if(AmbientLights.begin(), AmbientLights.end(),
			[Owner](const FAmbientLightEntry& E) { return E.Owner == Owner; }),
		AmbientLights.end());
}

// ============================================================
// Global Directional Light (배열 관리, 렌더링은 [0]만 사용)
// ============================================================
void FSceneEnvironment::AddGlobalDirectionalLight(const UDirectionalLightComponent* Owner, const FGlobalDirectionalLightParams& Params)
{
	for (auto& Entry : DirectionalLights)
	{
		if (Entry.Owner == Owner)
		{
			Entry.Params = Params;
			return;
		}
	}
	DirectionalLights.push_back({ Owner, Params });
}

void FSceneEnvironment::RemoveGlobalDirectionalLight(const UDirectionalLightComponent* Owner)
{
	DirectionalLights.erase(
		std::remove_if(DirectionalLights.begin(), DirectionalLights.end(),
			[Owner](const FDirectionalLightEntry& E) { return E.Owner == Owner; }),
		DirectionalLights.end());
}

// ============================================================
// Point Lights
// ============================================================
void FSceneEnvironment::AddPointLight(const UPointLightComponent* Owner, const FPointLightParams& Params)
{
	for (auto& Entry : PointLights)
	{
		if (Entry.Owner == Owner)
		{
			Entry.Params = Params;
			return;
		}
	}
	PointLights.push_back({ Owner, Params });
}

void FSceneEnvironment::RemovePointLight(const UPointLightComponent* Owner)
{
	PointLights.erase(
		std::remove_if(PointLights.begin(), PointLights.end(),
			[Owner](const FPointLightEntry& E) { return E.Owner == Owner; }),
		PointLights.end());
}

// ============================================================
// Spot Lights
// ============================================================
void FSceneEnvironment::AddSpotLight(const USpotLightComponent* Owner, const FSpotLightParams& Params)
{
	for (auto& Entry : SpotLights)
	{
		if (Entry.Owner == Owner)
		{
			Entry.Params = Params;
			return;
		}
	}
	SpotLights.push_back({ Owner, Params });
}

void FSceneEnvironment::RemoveSpotLight(const USpotLightComponent* Owner)
{
	SpotLights.erase(
		std::remove_if(SpotLights.begin(), SpotLights.end(),
			[Owner](const FSpotLightEntry& E) { return E.Owner == Owner; }),
		SpotLights.end());
}

int32 FSceneEnvironment::FindSpotLightIndex(const USpotLightComponent* Owner) const
{
	for (int32 i = 0; i < static_cast<int32>(SpotLights.size()); ++i)
		if (SpotLights[i].Owner == Owner) return i;
	return -1;
}

int32 FSceneEnvironment::FindPointLightIndex(const UPointLightComponent* Owner) const
{
	for (int32 i = 0; i < static_cast<int32>(PointLights.size()); ++i)
		if (PointLights[i].Owner == Owner) return i;
	return -1;
}
