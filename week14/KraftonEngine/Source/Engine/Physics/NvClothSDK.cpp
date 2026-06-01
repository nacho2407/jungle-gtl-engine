#include "Physics/NvClothSDK.h"
#include "Physics/PhysXSDK.h"

#include "Core/Logging/Log.h"

void FNvClothSDK::Initialize()
{
	if (Factory) return;

	nv::cloth::InitializeNvCloth(FPhysXSDK::Get().GetAllocatorCallback(),
		FPhysXSDK::Get().GetErrorCallback(), &AssertHandler, nullptr);

	Factory = NvClothCreateFactoryCPU();

	if (!Factory)
	{
		UE_LOG("NvCloth CPU factory creation failed.");
		return;
	}

	Solver = Factory->createSolver();

	if (!Solver)
	{
		UE_LOG("NvCloth solver creation failed.");
		NvClothDestroyFactory(Factory);
		Factory = nullptr;
		return;
	}
}

void FNvClothSDK::Shutdown()
{
	if (Solver)
	{
		delete Solver;
		Solver = nullptr;
	}

	if (Factory)
	{
		NvClothDestroyFactory(Factory);
		Factory = nullptr;
	}
}
