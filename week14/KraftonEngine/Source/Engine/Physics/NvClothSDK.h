#pragma once

#include "Core/Singleton.h"
#include "Core/Logging/Log.h"
#include "Physics/NVClothInclude.h"
#include "Physics/ClothInstance.h"

class FNvClothAssertHandler : public nv::cloth::PxAssertHandler
{
public:
	void operator()(const char* Exp, const char* File, int Line, bool& Ignore) override
	{
		UE_LOG("NvCloth Assert: %s, %s, line %d", Exp, File, Line);
		Ignore = false;
	}
};

typedef struct CUctx_st* CUcontext;

class FNvClothSDK : public TSingleton<FNvClothSDK>
{
	friend class TSingleton<FNvClothSDK>;
public:
	void Initialize();
	void Shutdown();

	nv::cloth::Factory* GetFactory() const { return Factory; }
	nv::cloth::Solver* GetSolver() const { return Solver; }

private:
	bool TryInitializeCuda();
	void ShutdownCuda();

private:
	FNvClothAssertHandler AssertHandler;

	nv::cloth::Factory* Factory = nullptr;
	nv::cloth::Solver* Solver = nullptr;

	CUcontext CudaContext = nullptr;
	bool bUsingCuda = false;
};
