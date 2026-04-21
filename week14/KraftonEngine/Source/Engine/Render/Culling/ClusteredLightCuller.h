#pragma once
#include "Render/Types/RenderTypes.h"
#include "Engine/Math/Vector.h"
#include "Engine/Render/Pipeline/ForwardLightData.h"
#include <cstring>

struct FAABB
{
	FVector4 Min;
	FVector4 Max;
};
static_assert(sizeof(FAABB) == sizeof(FVector4) * 2, "FAABB size mismatch with HLSL");

class FClusteredLightCuller
{
public:
	void Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext);
	void DispatchViewSpaceAABB();
	void DispatchLightCullingCS(ID3D11ShaderResourceView* LightInfos);
	bool IsInitialized() const { return bIsInitialized; }

	template<typename T>
	void InitializeBuffer(
		ID3D11Buffer*& InBuffer,
		uint32 Size,
		ID3D11ShaderResourceView*& SRV,
		ID3D11UnorderedAccessView*& UAV,
		bool bCreateSRV = true,
		bool bCreateUAV = true)
	{
		D3D11_BUFFER_DESC Desc = {};
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE   // SRV용
			| D3D11_BIND_UNORDERED_ACCESS;  // UAV용
		Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		Desc.StructureByteStride = sizeof(T);
		Desc.ByteWidth = sizeof(T) * Size;

		Device->CreateBuffer(&Desc, nullptr, &InBuffer);

		if (bCreateSRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
			SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = Size;
			Device->CreateShaderResourceView(InBuffer, &SRVDesc, &SRV);
		}
		if (bCreateUAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
			UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
			UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			UAVDesc.Buffer.FirstElement = 0;
			UAVDesc.Buffer.NumElements = Size;
			Device->CreateUnorderedAccessView(InBuffer, &UAVDesc, &UAV);
		}
	}

	FClusterCullingState& GetCullingState() { return State; }
	ID3D11ShaderResourceView* GetLightIndexListSRV() const { return gLightIndexListSRV; }
	ID3D11ShaderResourceView* GetLightGridSRV() const { return gLightGridSRV; }

private:
	void CompileComputeShader(const wchar_t* Path, const char* Entry, ID3D11ComputeShader*& CS);

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* Context = nullptr;
	ID3D11ComputeShader* ViewSpaceAABBCS = nullptr; //RWStructuredBuffer 하나 필요 gClusterAABBs
	ID3D11ComputeShader* LightCullingCS = nullptr; //StructuredBuffer 두개 gClusterAABBs gLightInfo, RWStructedBuffer gLightIndexList, gLightGrid, gGlobalCounter 총 3개 필요

	ID3D11Buffer* gClusterAABBs = nullptr; //Input, Output
	ID3D11Buffer* gLightIndexList = nullptr; //Input, Output
	ID3D11Buffer* gLightGrid = nullptr; //Input, Output
	ID3D11Buffer* gGlobalCounter = nullptr; //Input

	ID3D11ShaderResourceView* gClusterAABBsSRV = nullptr; //LightCullingCS의 Input(Structured Buffer)
	ID3D11ShaderResourceView* gLightIndexListSRV = nullptr; //최종 픽셀Shader의 Input(Structured Buffer)
	ID3D11ShaderResourceView* gLightGridSRV = nullptr; //최종 픽셀Shader의 Input(Structured Buffer)
	//LightCullingShader와 최종PixelShader에 들어갈 gLights(LightInfos)는 밖에 있을 것

	ID3D11UnorderedAccessView* gClusterAABBsUAV = nullptr; //RWStructuredBuffer (ViewSpaceAABBCS의 I/O)
	ID3D11UnorderedAccessView* gLightIndexListUAV = nullptr; //LightCulling의 I/O
	ID3D11UnorderedAccessView* gLightGridUAV = nullptr; // LightCulling의 I/O
	ID3D11UnorderedAccessView* gGlobalCounterUAV = nullptr; //LightCulling의 I/O
	//최종 그리는 픽셀셰이더에서는 StructuredBuffer [gLightIndexList, gLightGird, gLights(LightInfos)]필요함

	FClusterCullingState State;

	bool bIsInitialized = false;
};
