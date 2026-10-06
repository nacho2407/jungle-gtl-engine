#include "ClusteredLightCuller.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Types/FrameContext.h"
#include "Render/Shader/ShaderManager.h"

namespace
{
	constexpr uint32 ClusterCullingThreadGroupSizeX = 8;
	constexpr uint32 ClusterCullingThreadGroupSizeY = 3;
	constexpr uint32 ClusterCullingThreadGroupSizeZ = 4;
}
void FClusteredLightCuller::UpdateFrameState(const FFrameContext& Frame)
{
	State.NearZ = Frame.NearClip;
	State.FarZ = Frame.FarClip;
	State.ScreenWidth = static_cast<uint32>(Frame.ViewportWidth);
	State.ScreenHeight = static_cast<uint32>(Frame.ViewportHeight);
	State.bIsOrtho = Frame.bIsOrtho ? 1 : 0;
	State.OrthoWidth = Frame.OrthoWidth;
}

void FClusteredLightCuller::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
	Device = InDevice;
	Context = InContext;

	ViewSpaceAABBCS = FShaderManager::Get().GetOrCreateCS("Shaders/Lighting/ClusterConstructCS.hlsl", "CSMain");
	LightCullingCS  = FShaderManager::Get().GetOrCreateCS("Shaders/Lighting/LightCullingCS.hlsl", "CSMain");

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

	bIsInitialized = (ViewSpaceAABBCS != nullptr && ViewSpaceAABBCS->IsValid());
}

void FClusteredLightCuller::Release()
{
	ViewSpaceAABBCS = nullptr;  // FShaderManager 소유
	LightCullingCS = nullptr;

	if (gClusterAABBs)      { gClusterAABBs->Release();      gClusterAABBs = nullptr; }
	if (gLightIndexList)    { gLightIndexList->Release();    gLightIndexList = nullptr; }
	if (gLightGrid)         { gLightGrid->Release();         gLightGrid = nullptr; }
	if (gGlobalCounter)     { gGlobalCounter->Release();     gGlobalCounter = nullptr; }
	if (gClusterAABBsSRV)   { gClusterAABBsSRV->Release();   gClusterAABBsSRV = nullptr; }
	if (gLightIndexListSRV) { gLightIndexListSRV->Release(); gLightIndexListSRV = nullptr; }
	if (gLightGridSRV)      { gLightGridSRV->Release();      gLightGridSRV = nullptr; }
	if (gClusterAABBsUAV)   { gClusterAABBsUAV->Release();   gClusterAABBsUAV = nullptr; }
	if (gLightIndexListUAV) { gLightIndexListUAV->Release(); gLightIndexListUAV = nullptr; }
	if (gLightGridUAV)      { gLightGridUAV->Release();      gLightGridUAV = nullptr; }
	if (gGlobalCounterUAV)  { gGlobalCounterUAV->Release();  gGlobalCounterUAV = nullptr; }

	bIsInitialized = false;
}

void FClusteredLightCuller::DispatchViewSpaceAABB()
{
	if (!ViewSpaceAABBCS || !ViewSpaceAABBCS->IsValid())
	{
		return;
	}

	ViewSpaceAABBCS->Bind(Context);
	Context->CSSetUnorderedAccessViews(ELightCullingUAVSlot::ClusterAABB, 1, &gClusterAABBsUAV, nullptr);
	Context->Dispatch(
		(State.ClusterX + ClusterCullingThreadGroupSizeX - 1) / ClusterCullingThreadGroupSizeX,
		(State.ClusterY + ClusterCullingThreadGroupSizeY - 1) / ClusterCullingThreadGroupSizeY,
		(State.ClusterZ + ClusterCullingThreadGroupSizeZ - 1) / ClusterCullingThreadGroupSizeZ);
	ID3D11UnorderedAccessView* NullUAV = nullptr;
	Context->CSSetUnorderedAccessViews(ELightCullingUAVSlot::ClusterAABB, 1, &NullUAV, nullptr);
}

void FClusteredLightCuller::DispatchLightCullingCS(ID3D11ShaderResourceView* LightInfos)
{
	if (!LightCullingCS || !LightCullingCS->IsValid())
	{
		return;
	}
	const UINT ClearValues[4] = { 0,0,0,0 };
	Context->ClearUnorderedAccessViewUint(gGlobalCounterUAV, ClearValues);
	ID3D11ShaderResourceView* SRVs[2] = { gClusterAABBsSRV,LightInfos };
	ID3D11UnorderedAccessView* UAVs[3] = { gLightIndexListUAV,gLightGridUAV,gGlobalCounterUAV };
	LightCullingCS->Bind(Context);
	Context->CSSetUnorderedAccessViews(ELightCullingUAVSlot::LightIndexList, 3, UAVs, nullptr);
	Context->CSSetShaderResources(ELightCullingSRVSlot::ClusterAABB, 2, SRVs);
	const uint32 ClusterCount = State.ClusterX * State.ClusterY * State.ClusterZ;
	Context->Dispatch(ClusterCount, 1, 1);
	ID3D11UnorderedAccessView* NullUAV[3] = {};
	ID3D11ShaderResourceView* NullSRV[2] = {};
	Context->CSSetUnorderedAccessViews(ELightCullingUAVSlot::LightIndexList, 3, NullUAV, nullptr);
	Context->CSSetShaderResources(ELightCullingSRVSlot::ClusterAABB, 2, NullSRV);

}
