#include "Cloth/NvClothContext.h"

#include "Cloth/ClothBuildConfig.h"
#include "Core/Logging/Log.h"

#if WITH_NV_CLOTH
#include <NvCloth/Callbacks.h>
#include <NvCloth/Factory.h>
#include <PxPhysicsAPI.h>
#endif

#if WITH_NV_CLOTH
namespace
{
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

FNvClothContext::~FNvClothContext()
{
	Shutdown();
}

bool FNvClothContext::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
{
	Shutdown();

	Device = InDevice;
	DeviceContext = InDeviceContext;
	bInitialized = true;

#if WITH_NV_CLOTH
	EnsureNvClothCallbacksInitialized();

	const bool bCudaCompiled = NvClothCompiledWithCudaSupport();
	const bool bDxCompiled = NvClothCompiledWithDxSupport();
	UE_LOG("[NvCloth] Compiled backend support: CUDA=%s, DX11=%s", bCudaCompiled ? "true" : "false", bDxCompiled ? "true" : "false");

	if (!CreateCpuFactory())
	{
		BackendStatus.Backend = EClothBackendType::Disabled;
		BackendStatus.bAvailable = false;
		BackendStatus.Detail = "NvCloth CPU factory creation failed";
		UE_LOG("[NvCloth] Backend disabled: %s", BackendStatus.Detail.c_str());
		return false;
	}

	BackendStatus.Backend = EClothBackendType::CPU;
	BackendStatus.bAvailable = true;
	BackendStatus.Detail = "NvCloth CPU factory smoke path initialized";
	UE_LOG("[NvCloth] Backend initialized: %s", GetClothBackendName(BackendStatus.Backend));
	return true;
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

	Device = nullptr;
	DeviceContext = nullptr;
	BackendStatus = FClothBackendStatus();
	bInitialized = false;
}

bool FNvClothContext::CreateCpuFactory()
{
#if WITH_NV_CLOTH
	ReleaseFactory();

	// Milestone 1 범위: CUDA/DX11 실제 시도는 Milestone 2의 fallback 작업에서 추가
	Factory = NvClothCreateFactoryCPU();
	return Factory != nullptr;
#else
	return false;
#endif
}

void FNvClothContext::ReleaseFactory()
{
#if WITH_NV_CLOTH
	if (Factory)
	{
		NvClothDestroyFactory(Factory);
		Factory = nullptr;
	}
#else
	Factory = nullptr;
#endif
}
