#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Render/Types/LightTypes.h"
#include "Render/Pipelines/Context/View/SceneView.h"
#include <d3d11.h>

// ============================================================
// LightCulling 상수 버퍼 (b2 레지스터, 셰이더와 레이아웃 일치)
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

    // ---- 초기화 / 해제 ----
    void Initialize(FD3DDevice* InDevice);
    void Release();
    bool IsInitialized() const { return Device != nullptr && LightCullingCS != nullptr; }
    void ResizeTiles(uint32 InWidth, uint32 InHeight);

    // ---- 화면 리사이즈 ----
    void OnResize(uint32 InWidth, uint32 InHeight);

    // ---- 조명 데이터 갱신 ----
    void SetPointLightData(const uint32 InLightsCount);

    // ---- Dispatch ----
    void Dispatch(const FFrameContext& frameContext, bool bEnable25DCulling = true);

    // ---- 결과 SRV (Pixel Shader에서 타일별 마스크 읽기) ----
    ID3D11ShaderResourceView* GetPerTileMaskSRV()    const { return PerTilePointLightIndexMaskSRV; }
    ID3D11ShaderResourceView* GetDebugHitMapSRV()    const { return DebugHitMapSRV; }
    //ID3D11ShaderResourceView* GetPointLightDataSRV() const { return PointLightDataSRV; }

	//LightPass에 던질 param 정보
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
    ID3D11ComputeShader* LightCullingCS = nullptr;

    // ---- PointLight Buffer (SRV, t0) ----
    //ID3D11Buffer*              PointLightBuffer    = nullptr;
    //ID3D11ShaderResourceView*  PointLightDataSRV   = nullptr;
    //TArray<FLocalLightInfo> Lights;
    uint32 LightCount= 0;
    // ---- PerTile Mask Buffer (UAV u1 / SRV for PS) ----
    ID3D11Buffer*              PerTilePointLightIndexMaskBuffer  = nullptr;
    ID3D11UnorderedAccessView* PerTilePointLightIndexMaskOutUAV  = nullptr;
    ID3D11ShaderResourceView*  PerTilePointLightIndexMaskSRV     = nullptr;

    // ---- Culled (OR 누적) Mask Buffer (UAV u2, Shadow Map용) ----
    ID3D11Buffer*              CulledPointLightIndexMaskBuffer   = nullptr;
    ID3D11UnorderedAccessView* CulledPointLightIndexMaskOUTUAV   = nullptr;

    // ---- Debug HitMap (UAV u3 / SRV for 후처리) ----
    ID3D11Texture2D*           DebugHitMapTexture = nullptr;
    ID3D11UnorderedAccessView* DebugHitMapUAV     = nullptr;
    ID3D11ShaderResourceView*  DebugHitMapSRV     = nullptr;

    // ---- LightCullingParams 상수 버퍼 (b2) ----
    ID3D11Buffer* LightCullingParamsCB = nullptr;

    // ---- Tile 메타데이터 ----
    uint32 NumTilesX        = 0;
    uint32 NumTilesY        = 0;
    uint32 NumTiles         = 0;
    uint32 NumBucketsPerTile = 0;
    uint32 CurrentWidth     = 0;
    uint32 CurrentHeight    = 0;
    float NearZ;
    float FarZ;
};
