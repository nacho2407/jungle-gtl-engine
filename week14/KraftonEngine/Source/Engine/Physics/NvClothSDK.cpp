#include "Physics/NvClothSDK.h"
#include "Physics/PhysXSDK.h"

#include "Core/Logging/Log.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace
{
using CUresult = int;
using CUdevice = int;

constexpr CUresult CUDA_SUCCESS_VALUE = 0;

using PFN_cuInit = CUresult(__stdcall*)(unsigned int);
using PFN_cuDeviceGet = CUresult(__stdcall*)(CUdevice*, int);
using PFN_cuCtxCreate = CUresult(__stdcall*)(CUcontext*, unsigned int, CUdevice);
using PFN_cuCtxDestroy = CUresult(__stdcall*)(CUcontext);

HMODULE GCudaDriverDll = nullptr;
PFN_cuInit GCuInit = nullptr;
PFN_cuDeviceGet GCuDeviceGet = nullptr;
PFN_cuCtxCreate GCuCtxCreate = nullptr;
PFN_cuCtxDestroy GCuCtxDestroy = nullptr;

template <typename T>
T LoadCudaProc(const char* ProcName)
{
	return reinterpret_cast<T>(GetProcAddress(GCudaDriverDll, ProcName));
}

bool LoadCudaDriver()
{
	if (GCudaDriverDll) return true;

	GCudaDriverDll = LoadLibraryA("nvcuda.dll");
	if (!GCudaDriverDll)
	{
		UE_LOG("CUDA driver dll not found. Falling back to NvCloth CPU.");
		return false;
	}

	GCuInit = LoadCudaProc<PFN_cuInit>("cuInit");
	GCuDeviceGet = LoadCudaProc<PFN_cuDeviceGet>("cuDeviceGet");
	GCuCtxCreate = LoadCudaProc<PFN_cuCtxCreate>("cuCtxCreate_v2");
	GCuCtxDestroy = LoadCudaProc<PFN_cuCtxDestroy>("cuCtxDestroy_v2");

	if (!GCuInit || !GCuDeviceGet || !GCuCtxCreate || !GCuCtxDestroy)
	{
		UE_LOG("CUDA driver entry point loading failed. Falling back to NvCloth CPU.");
		FreeLibrary(GCudaDriverDll);
		GCudaDriverDll = nullptr;
		GCuInit = nullptr;
		GCuDeviceGet = nullptr;
		GCuCtxCreate = nullptr;
		GCuCtxDestroy = nullptr;
		return false;
	}

	return true;
}

void UnloadCudaDriver()
{
	GCuInit = nullptr;
	GCuDeviceGet = nullptr;
	GCuCtxCreate = nullptr;
	GCuCtxDestroy = nullptr;

	if (GCudaDriverDll)
	{
		FreeLibrary(GCudaDriverDll);
		GCudaDriverDll = nullptr;
	}
}
}

void FNvClothSDK::Initialize()
{
	if (Factory) return;

	nv::cloth::InitializeNvCloth(FPhysXSDK::Get().GetAllocatorCallback(),
		FPhysXSDK::Get().GetErrorCallback(), &AssertHandler, nullptr);

	if (TryInitializeCuda())
	{
		Factory = NvClothCreateFactoryCUDA(CudaContext);
		bUsingCuda = Factory != nullptr;

		if (!Factory)
		{
			UE_LOG("NvCloth CUDA factory creation failed. Falling back to CPU.");
			ShutdownCuda();
		}
	}

	if (!Factory)
	{
		Factory = NvClothCreateFactoryCPU();
	}

	if (!Factory)
	{
		UE_LOG("NvCloth factory creation failed.");
		return;
	}

	Solver = Factory->createSolver();

	if (!Solver)
	{
		UE_LOG("NvCloth solver creation failed.");
		NvClothDestroyFactory(Factory);
		Factory = nullptr;
		ShutdownCuda();
		return;
	}

	UE_LOG("NvCloth initialized with %s backend.", bUsingCuda ? "CUDA" : "CPU");
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

	ShutdownCuda();
}

bool FNvClothSDK::TryInitializeCuda()
{
	if (!NvClothCompiledWithCudaSupport())
	{
		UE_LOG("NvCloth DLL was not compiled with CUDA support. Falling back to CPU.");
		return false;
	}

	if (!LoadCudaDriver())
	{
		return false;
	}

	if (GCuInit(0) != CUDA_SUCCESS_VALUE)
	{
		UE_LOG("CUDA initialization failed. Falling back to NvCloth CPU.");
		ShutdownCuda();
		return false;
	}

	CUdevice Device = 0;
	if (GCuDeviceGet(&Device, 0) != CUDA_SUCCESS_VALUE)
	{
		UE_LOG("CUDA device 0 not found. Falling back to NvCloth CPU.");
		ShutdownCuda();
		return false;
	}

	if (GCuCtxCreate(&CudaContext, 0, Device) != CUDA_SUCCESS_VALUE || !CudaContext)
	{
		UE_LOG("CUDA context creation failed. Falling back to NvCloth CPU.");
		CudaContext = nullptr;
		ShutdownCuda();
		return false;
	}

	return true;
}

void FNvClothSDK::ShutdownCuda()
{
	if (CudaContext && GCuCtxDestroy)
	{
		GCuCtxDestroy(CudaContext);
		CudaContext = nullptr;
	}

	bUsingCuda = false;
	UnloadCudaDriver();
}
