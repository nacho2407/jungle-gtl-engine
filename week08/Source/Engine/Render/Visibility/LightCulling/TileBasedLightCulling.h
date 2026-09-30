#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include <d3d11.h>

class FViewportClient;
class FD3DDevice;
class FConstantBuffer;
using FFrameContext = FSceneView;

// FTileBasedLightCulling는 조명 계산이나 조명 제출에 필요한 데이터를 다룹니다.
class FTileBasedLightCulling
{
public:
    FTileBasedLightCulling() = default;
    ~FTileBasedLightCulling();

    FTileBasedLightCulling(const FTileBasedLightCulling&)            = delete;
    FTileBasedLightCulling& operator=(const FTileBasedLightCulling&) = delete;

    void Initialize(FD3DDevice* InDevice);
    void Release();
    bool IsInitialized() const { return Device != nullptr && LightCullingCS_Tile != nullptr && LightCullingCS_25D != nullptr; }
    void ResizeTiles(uint32 InWidth, uint32 InHeight);

    void OnResize(uint32 InWidth, uint32 InHeight);

    void SetPointLightData(const uint32 InLightsCount);

    void ClearDebugHitMap();

    // ---- Dispatch ----
    void Dispatch(const FFrameContext& frameContext, bool bEnable25DCulling = true);

    ID3D11ShaderResourceView* GetPerTileMaskSRV() const { return PerTilePointLightIndexMaskSRV; }
    ID3D11ShaderResourceView* GetDebugHitMapSRV() const { return DebugHitMapSRV; }
    // ID3D11ShaderResourceView* GetPointLightDataSRV() const { return PointLightDataSRV; }

    //----WRAPPER for LightCullingParamsCB----
    FConstantBuffer* GetLightCullingParamsCBWrapper() { return LightCullingParamsCBWrapper; }


    ID3D11Buffer* GetLightCullingParamsCB() const { return LightCullingParamsCB; }

    uint32 GetNumTilesX() const { return NumTilesX; }
    uint32 GetNumTilesY() const { return NumTilesY; }
    uint32 GetNumBucketsPerTile() const { return NumBucketsPerTile; }

private:
    void CreatePointLightBufferGPU();
    void CreateTileMaskBuffers();
    void CreateDebugHitMap(uint32 InWidth, uint32 InHeight);
    void UpdateLightCullingParamsCB(const FFrameContext& frameContext, bool bEnable25DCulling);

    // ---- Device ----
    FD3DDevice* Device = nullptr;

    // ---- Shaders ----
    // ID3D11ComputeShader* LightCullingCS = nullptr;
    ID3D11ComputeShader* LightCullingCS_Tile = nullptr;
    ID3D11ComputeShader* LightCullingCS_25D  = nullptr;

    // ---- PointLight Buffer (SRV, t0) ----
    // ID3D11Buffer*              PointLightBuffer    = nullptr;
    // ID3D11ShaderResourceView*  PointLightDataSRV   = nullptr;
    // TArray<FLocalLightCBData> Lights;
    uint32 LightCount = 0;
    // ---- PerTile Mask Buffer (UAV u1 / SRV for PS) ----
    ID3D11Buffer*              PerTilePointLightIndexMaskBuffer = nullptr;
    ID3D11UnorderedAccessView* PerTilePointLightIndexMaskUAV    = nullptr;
    ID3D11ShaderResourceView*  PerTilePointLightIndexMaskSRV    = nullptr;

    ID3D11Buffer*              CulledPointLightIndexMaskBuffer = nullptr;
    ID3D11UnorderedAccessView* CulledPointLightIndexMaskUAV    = nullptr;

    ID3D11Texture2D*           DebugHitMapTexture = nullptr;
    ID3D11UnorderedAccessView* DebugHitMapUAV     = nullptr;
    ID3D11ShaderResourceView*  DebugHitMapSRV     = nullptr;

    ID3D11Buffer*    LightCullingParamsCB        = nullptr;
    FConstantBuffer* LightCullingParamsCBWrapper = nullptr;

    uint32 NumTilesX         = 0;
    uint32 NumTilesY         = 0;
    uint32 NumTiles          = 0;
    uint32 NumBucketsPerTile = 0;
    uint32 CurrentWidth      = 0;
    uint32 CurrentHeight     = 0;
    float  NearZ;
    float  FarZ;
};
