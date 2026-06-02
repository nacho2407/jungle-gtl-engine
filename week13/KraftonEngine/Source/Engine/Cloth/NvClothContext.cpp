#include "Cloth/NvClothContext.h"

#include "Cloth/ClothBuildConfig.h"
#include "Core/Logging/Log.h"

#include <string>

#if WITH_NV_CLOTH
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <NvCloth/Callbacks.h>
#include <NvCloth/DxContextManagerCallback.h>
#include <NvCloth/Factory.h>
#include <PxPhysicsAPI.h>

#include <mutex>
#endif

namespace
{
/**
 * @brief backend detail 문자열에 항목을 추가합니다
 *
 * @param Detail 누적할 detail 문자열
 *
 * @param Entry 추가할 detail 항목
 */
void AppendBackendDetail(FString& Detail, const FString& Entry)
{
	if (Entry.empty())
	{
		return;
	}

	if (!Detail.empty())
	{
		Detail += " | ";
	}

	Detail += Entry;
}
}

#if WITH_NV_CLOTH
namespace
{
constexpr int GCudaSuccess = 0;

/**
 * @brief NvCloth 오류 callback
 */
class FNvClothErrorCallback : public physx::PxErrorCallback
{
public:
	void reportError(physx::PxErrorCode::Enum Code, const char* Message, const char* File, int Line) override
	{
		const char* Severity = "Info";
		if (Code == physx::PxErrorCode::eABORT || Code == physx::PxErrorCode::eOUT_OF_MEMORY)
		{
			Severity = "Fatal";
		}
		else if (Code == physx::PxErrorCode::eINTERNAL_ERROR || Code == physx::PxErrorCode::eINVALID_OPERATION)
		{
			Severity = "Error";
		}
		else if (Code == physx::PxErrorCode::eINVALID_PARAMETER || Code == physx::PxErrorCode::ePERF_WARNING || Code == physx::PxErrorCode::eDEBUG_WARNING)
		{
			Severity = "Warning";
		}

		UE_LOG("[NvCloth %s] %s (%s:%d)", Severity, Message ? Message : "", File ? File : "", Line);
	}
};

/**
 * @brief NvCloth assert 처리기
 */
class FNvClothAssertHandler : public nv::cloth::PxAssertHandler
{
public:
	void operator()(const char* Exp, const char* File, int Line, bool& Ignore) override
	{
		// NvCloth assert 위치 기록
		UE_LOG("[NvCloth Assert] %s (%s:%d)", Exp ? Exp : "", File ? File : "", Line);
		Ignore = false;
	}
};

static physx::PxDefaultAllocator GNvClothAllocator;
static FNvClothErrorCallback GNvClothErrorCallback;
static FNvClothAssertHandler GNvClothAssertHandler;
static bool GNvClothCallbacksInitialized = false;

/**
 * @brief CUDA driver API runtime loader
 */
class FNvClothCudaDriverApi
{
public:
	using FCudaResult = int;
	using FCudaDevice = int;
	using FCuInit = FCudaResult(WINAPI*)(unsigned int);
	using FCuDeviceGet = FCudaResult(WINAPI*)(FCudaDevice*, int);
	using FCuCtxCreate = FCudaResult(WINAPI*)(CUcontext*, unsigned int, FCudaDevice);
	using FCuCtxDestroy = FCudaResult(WINAPI*)(CUcontext);
	using FCuCtxSetCurrent = FCudaResult(WINAPI*)(CUcontext);

	/**
	 * @brief nvcuda.dll과 필요한 driver API 함수를 불러옵니다
	 *
	 * @param OutFailureDetail load 실패 사유
	 *
	 * @return load 성공 여부
	 */
	bool Load(FString& OutFailureDetail)
	{
		if (Module)
		{
			return true;
		}

		/*
		 * note: CUDA driver API는 CUDA Toolkit 설치 여부와 무관하게 NVIDIA driver가 설치되어 있다면 사용할 수 있음
		 */

		// CUDA Toolkit 설치 여부와 무관하게 NVIDIA driver runtime만 확인
		Module = LoadLibraryA("nvcuda.dll");
		if (!Module)
		{
			OutFailureDetail = "nvcuda.dll load failed, win32 error " + std::to_string(GetLastError());
			return false;
		}

		if (!LoadProc("cuInit", CuInit)
			|| !LoadProc("cuDeviceGet", CuDeviceGet)
			|| !LoadProc("cuCtxCreate_v2", CuCtxCreate)
			|| !LoadProc("cuCtxDestroy_v2", CuCtxDestroy))
		{
			OutFailureDetail = "required CUDA driver API symbol is missing";
			Unload();
			return false;
		}

		// 현재 context 지정은 일부 driver/runtime 조합에서만 필요하므로 optional로 둠
		LoadProc("cuCtxSetCurrent", CuCtxSetCurrent);

		return true;
	}

	/**
	 * @brief load된 CUDA driver API module을 해제합니다
	 */
	void Unload()
	{
		CuInit = nullptr;
		CuDeviceGet = nullptr;
		CuCtxCreate = nullptr;
		CuCtxDestroy = nullptr;
		CuCtxSetCurrent = nullptr;

		if (Module)
		{
			FreeLibrary(Module);
			Module = nullptr;
		}
	}

	/**
	 * @brief CUDA driver API load 여부를 반환합니다
	 *
	 * @return CUDA driver API load 여부
	 */
	bool IsLoaded() const
	{
		return Module != nullptr;
	}

	FCuInit CuInit = nullptr;
	FCuDeviceGet CuDeviceGet = nullptr;
	FCuCtxCreate CuCtxCreate = nullptr;
	FCuCtxDestroy CuCtxDestroy = nullptr;
	FCuCtxSetCurrent CuCtxSetCurrent = nullptr;

private:
	template<typename F>
	bool LoadProc(const char* Name, F& OutFunc)
	{
		OutFunc = reinterpret_cast<F>(GetProcAddress(Module, Name));
		return OutFunc != nullptr;
	}

	HMODULE Module = nullptr;
};

/**
 * @brief NvCloth DX11 backend용 context callback
 */
class FNvClothDxContextManagerCallback : public nv::cloth::DxContextManagerCallback
{
public:
	FNvClothDxContextManagerCallback(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
		: Device(InDevice)
		, DeviceContext(InDeviceContext)
	{
	}

	/**
	 * @brief 현재 thread의 D3D11 context 사용을 시작합니다
	 */
	void acquireContext() override
	{
		ContextMutex.lock();
	}

	/**
	 * @brief 현재 thread의 D3D11 context 사용을 종료합니다
	 */
	void releaseContext() override
	{
		ContextMutex.unlock();
	}

	/**
	 * @brief NvCloth compute 작업에 사용할 D3D11 device
	 *
	 * @return D3D11 device
	 */
	ID3D11Device* getDevice() const override
	{
		return Device;
	}

	/**
	 * @brief NvCloth compute 작업에 사용할 D3D11 context
	 *
	 * @return D3D11 context
	 */
	ID3D11DeviceContext* getContext() const override
	{
		return DeviceContext;
	}

	/**
	 * @brief shared keyed mutex resource 사용 여부를 반환합니다
	 *
	 * @return shared keyed mutex resource 사용 여부
	 */
	bool synchronizeResources() const override
	{
		// 이번 milestone은 CPU readback 경로라서 D3D11 shared resource 동기화를 사용하지 않음
		return false;
	}

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;

	// 싱글 스레드라 큰 의미는 없지만, NvCloth callback 계약에 맞추면서 recursive acquire 조건에 대응하기 위해 사용
	mutable std::recursive_mutex ContextMutex;
};

/**
 * @brief NvCloth 전역 callback을 1회 초기화합니다
 */
static void EnsureNvClothCallbacksInitialized()
{
	if (GNvClothCallbacksInitialized)
	{
		return;
	}

	// NvCloth 전역 callback 1회 등록
	nv::cloth::InitializeNvCloth(&GNvClothAllocator, &GNvClothErrorCallback, &GNvClothAssertHandler, nullptr);
	GNvClothCallbacksInitialized = true;
}
}
#endif

struct FNvClothContext::FImpl
{
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;

#if WITH_NV_CLOTH
	nv::cloth::Factory* Factory = nullptr;
	FNvClothCudaDriverApi CudaApi;
	CUcontext CudaContext = nullptr;
	std::unique_ptr<nv::cloth::DxContextManagerCallback> DxContextCallback;
#else
	void* Factory = nullptr;
#endif
};

FNvClothContext::FNvClothContext()
	: Impl(std::make_unique<FImpl>())
{
}

FNvClothContext::~FNvClothContext()
{
	Shutdown();
}

bool FNvClothContext::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
{
	Shutdown();

	Impl->Device = InDevice;
	Impl->DeviceContext = InDeviceContext;
	bInitializeAttempted = true;

#if WITH_NV_CLOTH
	EnsureNvClothCallbacksInitialized();

	const bool bCudaCompiled = NvClothCompiledWithCudaSupport();
	const bool bDxCompiled = NvClothCompiledWithDxSupport();
	UE_LOG("[NvCloth] Compiled backend support: CUDA=%s, DX11=%s", bCudaCompiled ? "true" : "false", bDxCompiled ? "true" : "false");

	FString Detail;
	FString FailureDetail;

	if (CreateCudaFactory(FailureDetail))
	{
		BackendStatus.Backend = EClothBackendType::CUDA;
		BackendStatus.bAvailable = true;
		BackendStatus.Detail = "Selected CUDA backend";
		UE_LOG("[NvCloth] Backend initialized: %s", GetClothBackendName(BackendStatus.Backend));
		return true;
	}
	UE_LOG("[NvCloth] CUDA backend unavailable: %s", FailureDetail.c_str());
	AppendBackendDetail(Detail, "CUDA: " + FailureDetail);
	FailureDetail.clear();

	if (CreateDxFactory(FailureDetail))
	{
		BackendStatus.Backend = EClothBackendType::DX11;
		BackendStatus.bAvailable = true;
		BackendStatus.Detail = Detail;
		AppendBackendDetail(BackendStatus.Detail, "Selected DX11 backend");
		UE_LOG("[NvCloth] Backend initialized: %s", GetClothBackendName(BackendStatus.Backend));
		return true;
	}
	UE_LOG("[NvCloth] DX11 backend unavailable: %s", FailureDetail.c_str());
	AppendBackendDetail(Detail, "DX11: " + FailureDetail);
	FailureDetail.clear();

	if (CreateCpuFactory(FailureDetail))
	{
		BackendStatus.Backend = EClothBackendType::CPU;
		BackendStatus.bAvailable = true;
		BackendStatus.Detail = Detail;
		AppendBackendDetail(BackendStatus.Detail, "Selected CPU backend");
		UE_LOG("[NvCloth] Backend initialized: %s", GetClothBackendName(BackendStatus.Backend));
		return true;
	}
	UE_LOG("[NvCloth] CPU backend unavailable: %s", FailureDetail.c_str());
	AppendBackendDetail(Detail, "CPU: " + FailureDetail);

	BackendStatus.Backend = EClothBackendType::Disabled;
	BackendStatus.bAvailable = false;
	BackendStatus.Detail = Detail.empty() ? "all NvCloth backend creation attempts failed" : Detail;
	UE_LOG("[NvCloth] Backend disabled: %s", BackendStatus.Detail.c_str());
	return false;
#else
	BackendStatus.Backend = EClothBackendType::Disabled;
	BackendStatus.bAvailable = false;
	BackendStatus.Detail = "WITH_NV_CLOTH is disabled";
	UE_LOG("[NvCloth] Backend disabled: %s", BackendStatus.Detail.c_str());
	return false;
#endif
}

void FNvClothContext::Shutdown()
{
	ReleaseFactory();

	Impl->Device = nullptr;
	Impl->DeviceContext = nullptr;
	BackendStatus = FClothBackendStatus();
	bInitializeAttempted = false;
}

bool FNvClothContext::CreateCudaFactory(FString& OutFailureDetail)
{
#if WITH_NV_CLOTH
	ReleaseFactory();

	if (!NvClothCompiledWithCudaSupport())
	{
		OutFailureDetail = "NvCloth binary was not compiled with CUDA support";
		return false;
	}

	if (!Impl->CudaApi.Load(OutFailureDetail))
	{
		return false;
	}

	FNvClothCudaDriverApi::FCudaResult Result = Impl->CudaApi.CuInit(0);
	if (Result != GCudaSuccess)
	{
		OutFailureDetail = "cuInit failed with result " + std::to_string(Result);
		ReleaseFactory();
		return false;
	}

	FNvClothCudaDriverApi::FCudaDevice Device = 0;
	Result = Impl->CudaApi.CuDeviceGet(&Device, 0);
	if (Result != GCudaSuccess)
	{
		OutFailureDetail = "cuDeviceGet failed with result " + std::to_string(Result);
		ReleaseFactory();
		return false;
	}

	Result = Impl->CudaApi.CuCtxCreate(&Impl->CudaContext, 0, Device);
	if (Result != GCudaSuccess || !Impl->CudaContext)
	{
		OutFailureDetail = "cuCtxCreate_v2 failed with result " + std::to_string(Result);
		ReleaseFactory();
		return false;
	}

	if (Impl->CudaApi.CuCtxSetCurrent)
	{
		Result = Impl->CudaApi.CuCtxSetCurrent(Impl->CudaContext);
		if (Result != GCudaSuccess)
		{
			OutFailureDetail = "cuCtxSetCurrent failed with result " + std::to_string(Result);
			ReleaseFactory();
			return false;
		}
	}

	Impl->Factory = NvClothCreateFactoryCUDA(Impl->CudaContext);
	if (!Impl->Factory)
	{
		OutFailureDetail = "NvCloth CUDA factory creation returned nullptr";
		ReleaseFactory();
		return false;
	}

	return true;
#else
	OutFailureDetail = "WITH_NV_CLOTH is disabled";
	return false;
#endif
}

bool FNvClothContext::CreateDxFactory(FString& OutFailureDetail)
{
#if WITH_NV_CLOTH
	ReleaseFactory();

	if (!NvClothCompiledWithDxSupport())
	{
		OutFailureDetail = "NvCloth binary was not compiled with DX11 support";
		return false;
	}

	if (!Impl->Device || !Impl->DeviceContext)
	{
		OutFailureDetail = "D3D11 device or context is null";
		return false;
	}

	Impl->DxContextCallback = std::make_unique<FNvClothDxContextManagerCallback>(Impl->Device, Impl->DeviceContext);
	Impl->Factory = NvClothCreateFactoryDX11(Impl->DxContextCallback.get());
	if (!Impl->Factory)
	{
		OutFailureDetail = "NvCloth DX11 factory creation returned nullptr";
		ReleaseFactory();
		return false;
	}

	return true;
#else
	OutFailureDetail = "WITH_NV_CLOTH is disabled";
	return false;
#endif
}

bool FNvClothContext::CreateCpuFactory(FString& OutFailureDetail)
{
#if WITH_NV_CLOTH
	ReleaseFactory();

	Impl->Factory = NvClothCreateFactoryCPU();
	if (!Impl->Factory)
	{
		OutFailureDetail = "NvCloth CPU factory creation returned nullptr";
		return false;
	}

	return true;
#else
	OutFailureDetail = "WITH_NV_CLOTH is disabled";
	return false;
#endif
}

void FNvClothContext::ReleaseFactory()
{
#if WITH_NV_CLOTH
	if (Impl->Factory)
	{
		NvClothDestroyFactory(Impl->Factory);
		Impl->Factory = nullptr;
	}

	if (Impl->CudaContext && Impl->CudaApi.CuCtxDestroy)
	{
		Impl->CudaApi.CuCtxDestroy(Impl->CudaContext);
		Impl->CudaContext = nullptr;
	}
	else
	{
		Impl->CudaContext = nullptr;
	}

	Impl->DxContextCallback.reset();
	Impl->CudaApi.Unload();
#else
	Impl->Factory = nullptr;
#endif
}
