#include "Cloth/ClothSimulation.h"

#include "Cloth/NvClothContext.h"

FClothSimulation::~FClothSimulation()
{
	Shutdown();
}

bool FClothSimulation::Initialize(FNvClothContext* InContext)
{
	Shutdown();

	Context = InContext;
	bInitialized = Context && Context->GetBackendStatus().bAvailable;
	return bInitialized;
}

void FClothSimulation::Shutdown()
{
	Context = nullptr;
	bInitialized = false;
}

void FClothSimulation::Tick(float DeltaTime)
{
	(void)DeltaTime;

	// Milestone 1 범위: 실제 solver 진행 없이 simulation 연결 상태만 유지
	if (!IsSimulationAvailable())
	{
		return;
	}
}

bool FClothSimulation::IsSimulationAvailable() const
{
	return bInitialized && Context && Context->GetBackendStatus().bAvailable;
}

const FClothBackendStatus& FClothSimulation::GetBackendStatus() const
{
	static const FClothBackendStatus UnavailableStatus;

	if (!Context)
	{
		return UnavailableStatus;
	}

	return Context->GetBackendStatus();
}
