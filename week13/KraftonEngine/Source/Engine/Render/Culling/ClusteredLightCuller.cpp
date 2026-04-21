#include "ClusteredLightCuller.h"
#include "Render/Pipeline/RenderConstants.h"
void FClusteredLightCuller::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
	Device = InDevice;
	Context = InContext;
	CompileComputeShader(L"Shaders\\ClusterConstructCS.hlsl", "CSMain", ViewSpaceAABBCS);
	CompileComputeShader(L"Shaders\\LightCullingCS.hlsl", "CSMain", LightCullingCS);
	const uint32 ClusterCount = State.ClusterX * State.ClusterY * State.ClusterZ;
	struct Pair
	{
		int Offset, Count;
	};
	InitializeBuffer<FAABB>(gClusterAABBs, ClusterCount, gClusterAABBsSRV, gClusterAABBsUAV);
	InitializeBuffer<uint32>(gLightIndexList, ClusterCount * State.MaxLightsPerCluster, gLightIndexListSRV, gLightIndexListUAV);
	InitializeBuffer<Pair>(gLightGrid, ClusterCount, gLightGridSRV, gLightGridUAV);
	ID3D11ShaderResourceView* NullSRV = nullptr;
	InitializeBuffer<uint32>(gGlobalCounter, 1, NullSRV, gGlobalCounterUAV, false, true);

	bIsInitialized = (ViewSpaceAABBCS != nullptr);
}

void FClusteredLightCuller::DispatchViewSpaceAABB()
{
	if (!ViewSpaceAABBCS)
	{
		return;
	}

	Context->CSSetShader(ViewSpaceAABBCS, nullptr, 0);
	Context->CSSetUnorderedAccessViews(ELightCullingUAVSlot::ClusterAABB, 1, &gClusterAABBsUAV, nullptr);
	Context->Dispatch(State.ClusterX, State.ClusterY, State.ClusterZ);
	ID3D11UnorderedAccessView* NullUAV = nullptr;
	Context->CSSetUnorderedAccessViews(ELightCullingUAVSlot::ClusterAABB, 1, &NullUAV, nullptr);
}

void FClusteredLightCuller::DispatchLightCullingCS(ID3D11ShaderResourceView* LightInfos)
{
	if (!LightCullingCS)
	{
		return;
	}
	const UINT ClearValues[4] = { 0,0,0,0 };
	Context->ClearUnorderedAccessViewUint(gGlobalCounterUAV, ClearValues);
	ID3D11ShaderResourceView* SRVs[2] = { gClusterAABBsSRV,LightInfos };
	ID3D11UnorderedAccessView* UAVs[3] = { gLightIndexListUAV,gLightGridUAV,gGlobalCounterUAV };
	Context->CSSetShader(LightCullingCS, nullptr, 0);
	Context->CSSetUnorderedAccessViews(ELightCullingUAVSlot::LightIndexList, 3, UAVs, nullptr);
	Context->CSSetShaderResources(ELightCullingSRVSlot::ClusterAABB, 2, SRVs);
	Context->Dispatch(State.ClusterX, State.ClusterY, State.ClusterZ);
	ID3D11UnorderedAccessView* NullUAV[3] = {};
	ID3D11ShaderResourceView* NullSRV[2] = {};
	Context->CSSetUnorderedAccessViews(ELightCullingUAVSlot::LightIndexList, 3, NullUAV, nullptr);
	Context->CSSetShaderResources(ELightCullingSRVSlot::ClusterAABB, 2, NullSRV);

}

void FClusteredLightCuller::CompileComputeShader(const wchar_t* Path, const char* Entry, ID3D11ComputeShader*& CS)
{
	ID3DBlob* ShaderBlob = nullptr;
	ID3DBlob* ErrorBlob = nullptr;
	HRESULT Hr = D3DCompileFromFile(Path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		Entry, "cs_5_0", 0, 0, &ShaderBlob, &ErrorBlob);
	if (FAILED(Hr))
	{
		if (ErrorBlob)
		{
			OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
			ErrorBlob->Release();
		}
		return;
	}
	Device->CreateComputeShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &CS);
	ShaderBlob->Release();
}

