#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Scene/Proxies/Light/LightTypes.h"
#include <d3d11.h>

// ============================================================
// LightCulling ��� ���� (b2 ��������, ���̴��� ���̾ƿ� ��ġ)
// ============================================================
struct FLightCullingParams
{
    uint32 ScreenSizeX;
    uint32 ScreenSizeY;
    uint32 TileSizeX;
    uint32 TileSizeY;

    uint32 Enable25DCulling;
    float  NearZ;
    float  FarZ;
    float  NumLights;
};

class FViewportClient;
class FD3DDevice;
class FConstantBuffer;
using FFrameContext = FSceneView;

    // ============================================================
// FTileBasedLightCulling
// ============================================================
class FTileBasedLightCulling
{
public:
    FTileBasedLightCulling()  = default;
    ~FTileBasedLightCulling();

    FTileBasedLightCulling(const FTileBasedLightCulling&)            = delete;
    FTileBasedLightCulling& operator=(const FTileBasedLightCulling&) = delete;

    // ---- �ʱ�ȭ / ���� ----
    void Initialize(FD3DDevice* InDevice);
    void Release();
    bool IsInitialized() const { return Device != nullptr && LightCullingCS_Tile != nullptr && LightCullingCS_25D != nullptr; }
    void ResizeTiles(uint32 InWidth, uint32 InHeight);

    // ---- ȭ�� �������� ----
    void OnResize(uint32 InWidth, uint32 InHeight);

    // ---- ���� ������ ���� ----
    void SetPointLightData(const uint32 InLightsCount);
	
	// ---- ����� ��Ʈ�� ����(wireframe ����϶� ���)
    void ClearDebugHitMap();

    // ---- Dispatch ----
    void Dispatch(const FFrameContext& frameContext, bool bEnable25DCulling = true);

    // ---- ��� SRV (Pixel Shader���� Ÿ�Ϻ� ����ũ �б�) ----
    ID3D11ShaderResourceView* GetPerTileMaskSRV()    const { return PerTilePointLightIndexMaskSRV; }
    ID3D11ShaderResourceView* GetDebugHitMapSRV()    const { return DebugHitMapSRV; }
    //ID3D11ShaderResourceView* GetPointLightDataSRV() const { return PointLightDataSRV; }

	//----WRAPPER for LightCullingParamsCB----
    FConstantBuffer* GetLightCullingParamsCBWrapper() { return LightCullingParamsCBWrapper; }


	//LightPass�� ���� param ����
    ID3D11Buffer* GetLightCullingParamsCB() const { return LightCullingParamsCB; }

    uint32 GetNumTilesX()        const { return NumTilesX; }
    uint32 GetNumTilesY()        const { return NumTilesY; }
    uint32 GetNumBucketsPerTile() const { return NumBucketsPerTile; }

private:
    void CreatePointLightBufferGPU();
    void CreateTileMaskBuffers();
    void CreateDebugHitMap(uint32 InWidth, uint32 InHeight);
    void UpdateLightCullingParamsCB(const FFrameContext& frameContext, bool bEnable25DCulling);

    // ---- Device ----
    FD3DDevice* Device = nullptr;

    // ---- Shaders ----
    //ID3D11ComputeShader* LightCullingCS = nullptr;
    ID3D11ComputeShader* LightCullingCS_Tile = nullptr;
    ID3D11ComputeShader* LightCullingCS_25D = nullptr;

    // ---- PointLight Buffer (SRV, t0) ----
    //ID3D11Buffer*              PointLightBuffer    = nullptr;
    //ID3D11ShaderResourceView*  PointLightDataSRV   = nullptr;
    //TArray<FLocalLightInfo> Lights;
    uint32 LightCount= 0;
    // ---- PerTile Mask Buffer (UAV u1 / SRV for PS) ----
    ID3D11Buffer*              PerTilePointLightIndexMaskBuffer  = nullptr;
    ID3D11UnorderedAccessView* PerTilePointLightIndexMaskOutUAV  = nullptr;
    ID3D11ShaderResourceView*  PerTilePointLightIndexMaskSRV     = nullptr;

    // ---- Culled (OR ����) Mask Buffer (UAV u2, Shadow Map��) ----
    ID3D11Buffer*              CulledPointLightIndexMaskBuffer   = nullptr;
    ID3D11UnorderedAccessView* CulledPointLightIndexMaskOUTUAV   = nullptr;

    // ---- Debug HitMap (UAV u3 / SRV for ��ó��) ----
    ID3D11Texture2D*           DebugHitMapTexture = nullptr;
    ID3D11UnorderedAccessView* DebugHitMapUAV     = nullptr;
    ID3D11ShaderResourceView*  DebugHitMapSRV     = nullptr;

    // ---- LightCullingParams ��� ���� (b2) ----
    ID3D11Buffer* LightCullingParamsCB = nullptr;
    FConstantBuffer* LightCullingParamsCBWrapper = nullptr;

    // ---- Tile ��Ÿ������ ----
    uint32 NumTilesX        = 0;
    uint32 NumTilesY        = 0;
    uint32 NumTiles         = 0;
    uint32 NumBucketsPerTile = 0;
    uint32 CurrentWidth     = 0;
    uint32 CurrentHeight    = 0;
    float NearZ;
    float FarZ;
};
