#pragma once

#include "Core/Singleton.h"
#include "Render/Common/ComPtr.h"
#include "Core/CoreMinimal.h"
#include <d3d11.h>

static constexpr uint32 ShadowAtlasResolution2D = 8192;
static constexpr int CUBE_FACE_COUNT = 6;
static constexpr int MAX_SHADOW_CUBES = 32;
static constexpr uint32 SHADOW_CUBE_SIZE = 512;

static constexpr int32 AtlasSizeTier[4] = { 256, 512, 1024, 2048 };
static constexpr int32 TierCount = sizeof(AtlasSizeTier) / sizeof(AtlasSizeTier[0]);

struct FShadowAtlasTile
{
    int32 TileX = 0;
    int32 TileY = 0;
    int32 PixelX = 0;
    int32 PixelY = 0;

    int32 Width = 0;
    int32 Height = 0;

    FVector4 ScaleOffset;
};

struct FShadowAtlasDebugRect
{
	int32 X = 0;
	int32 Y = 0;
	int32 Width = 0;
	int32 Height = 0;
	int32 Depth = 0;
	bool bUsed = false;
};

// shadowmap 역할
struct Node
{
    Node* Child[2] = { nullptr, nullptr };
    int32 X = 0;
    int32 Y = 0;
    int32 Width = 0;
    int32 Height = 0;
	bool bUsed = false;

	bool IsLeaf() const
	{
		return Child[0] == nullptr && Child[1] == nullptr;
    }	

	Node(int32 InX, int32 InY, int32 InW, int32 InH)
		: X(InX), Y(InY), Width(InW), Height(InH)
	{
	}
	~Node()
	{
		delete Child[0];
		delete Child[1];
    }

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    bool Insert(int32 RequestTileSize, int32& OutX, int32& OutY)
    {
        if (!IsLeaf())
        {
            if (Child[0] && Child[0]->Insert(RequestTileSize, OutX, OutY))
            {	
                return true;
            }

            if (Child[1] && Child[1]->Insert(RequestTileSize, OutX, OutY))
            {
                return true;
            }
			return false;
        }
        
		if (bUsed || RequestTileSize > Width || RequestTileSize > Height)
		{
			return false;
        }

		if (RequestTileSize == Width && RequestTileSize == Height)
        {
            bUsed = true;

			OutX = X;
            OutY = Y;

			return true;
		}

		int32 RemainingWidth = Width - RequestTileSize;
        int32 RemainingHeight = Height - RequestTileSize;

		if (RemainingHeight > RemainingWidth)
		{
			Child[0] = new Node(X, Y, Width, RequestTileSize);
			Child[1] = new Node(X, Y + RequestTileSize, Width, RemainingHeight);
		}
		else
		{
			Child[0] = new Node(X, Y, RequestTileSize, Height);
			Child[1] = new Node(X + RequestTileSize, Y, RemainingWidth, Height);
        }

        return Child[0]->Insert(RequestTileSize, OutX, OutY);
    }

	void FreeAll()
	{
		bUsed = false;
		if (Child[0])
		{
			Child[0]->FreeAll();
            Child[0] = nullptr;
		}
		if (Child[1])
		{
			Child[1]->FreeAll();
            Child[1] = nullptr;
		}
    }

	void CollectLeafRects(TArray<FShadowAtlasDebugRect>& OutRects, int32 Depth = 0) const
	{
		if (!IsLeaf())
		{
			if (Child[0])
			{
				Child[0]->CollectLeafRects(OutRects, Depth + 1);
			}
			if (Child[1])
			{
				Child[1]->CollectLeafRects(OutRects, Depth + 1);
			}
			return;
		}

		FShadowAtlasDebugRect Rect = {};
		Rect.X = X;
		Rect.Y = Y;
		Rect.Width = Width;
		Rect.Height = Height;
		Rect.Depth = Depth;
		Rect.bUsed = bUsed;
		OutRects.push_back(Rect);
	}
};

struct FShadowAtlasAllocator
{
public:
    static constexpr int32 AtlasSize = 8192;
    static constexpr int32 GridSize = 256;
    static constexpr int32 GridCount = AtlasSize / GridSize; // 32
private:
    bool Used[GridCount][GridCount] = {};
    Node* RootNode = nullptr;

public:
    void Reset()
    {
        memset(Used, 0, sizeof(Used));
    }

    int32 QuantizeShadowSize(const int32 RequestTileSize) const
    {
        if (RequestTileSize <= 0)
        {
            return AtlasSizeTier[0];
        }

        for (int32 i = 0; i < TierCount; ++i)
        {
            if (RequestTileSize <= AtlasSizeTier[i])
            {
                return AtlasSizeTier[i];
            }
        }

        return AtlasSizeTier[TierCount - 1];
    }

	bool CanAllocate(int32 StartX, int32 StartY, int32 TileGridCount) const
	{
		for (int32 y = 0; y < TileGridCount; ++y)
		{
			for (int32 x = 0; x < TileGridCount; ++x)
			{
                if (Used[StartY + y][StartX + x])
                {
					return false;
				}
			}
		}
		return true;
    }

	void MarkUsed(int32 StartX, int32 StartY, int32 TileGridCount)
	{
		for (int32 y = 0; y < TileGridCount; ++y)
		{
			for (int32 x = 0; x < TileGridCount; ++x)
			{
				Used[StartY + y][StartX + x] = true;
			}
		}
    }

	bool AllocateTiled(int32& RequestTileSize, int32& OutTileX, int32& OutTileY)
	{
        RequestTileSize = QuantizeShadowSize(RequestTileSize);

        const int32 LocalTileSize = RequestTileSize;
        const int32 LocalGridCount = LocalTileSize / GridSize;

		for (int32 y = 0; y <= GridCount - LocalGridCount; ++y)
		{
            for (int32 x = 0; x <= GridCount - LocalGridCount; ++x)
            {
				if (CanAllocate(x, y, LocalGridCount)) {
					MarkUsed(x, y, LocalGridCount);
					OutTileX = x;
					OutTileY = y;
                    return true;
				}
            }
        }

        return false;
    }

	void FreeTile(int32 TileX, int32 TileY, int32 GridSize)
	{
        for (int32 y = 0; y < GridSize; ++y)
        {
            for (int32 x = 0; x < GridSize; ++x)
            {
                if (TileX + x >= 0 && TileX + x < GridCount && TileY + y >= 0 && TileY + y < GridCount)
                {
                    Used[TileY + y][TileX + x] = false;
                }
            }
		}
    }

    void FreeAllTiles()
    {
        for (int32 Y = 0; Y < GridCount; ++Y)
        {
            for (int32 X = 0; X < GridCount; ++X)
            {
                Used[Y][X] = false;
            }
        }
    }
};

struct FShadowAtlasCube
{
    TComPtr<ID3D11Texture2D> CubeShadowMap;
    TComPtr<ID3D11ShaderResourceView> CubeSRV;

    TComPtr<ID3D11DepthStencilView> CubeDSV[MAX_SHADOW_CUBES][CUBE_FACE_COUNT] = {};
    TComPtr<ID3D11Texture2D> CubeDebugTexture[MAX_SHADOW_CUBES][CUBE_FACE_COUNT] = {};
    TComPtr<ID3D11ShaderResourceView> CubeDebugSRV[MAX_SHADOW_CUBES][CUBE_FACE_COUNT] = {};

    uint32 CurrentCubeCount = 0;

    void Initialize(ID3D11Device* Device)
    {
        if (Device == nullptr)
            return;
        // Cube Shadow Map Texture 생성
        D3D11_TEXTURE2D_DESC CubeShadowDesc = {};
        CubeShadowDesc.Width = SHADOW_CUBE_SIZE;
        CubeShadowDesc.Height = SHADOW_CUBE_SIZE;
        CubeShadowDesc.MipLevels = 1;
        CubeShadowDesc.ArraySize = 6 * MAX_SHADOW_CUBES; // Cube Map은 6개의 면으로 구성
        CubeShadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        CubeShadowDesc.SampleDesc.Count = 1;
        CubeShadowDesc.Usage = D3D11_USAGE_DEFAULT;
        CubeShadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        CubeShadowDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        HRESULT hr = Device->CreateTexture2D(&CubeShadowDesc, nullptr, CubeShadowMap.ReleaseAndGetAddressOf());

        for (int CubeIndex = 0; CubeIndex < MAX_SHADOW_CUBES; ++CubeIndex)
        {
            for (int FaceIndex = 0; FaceIndex < CUBE_FACE_COUNT; ++FaceIndex)
            {
                // 각 면에 대한 DSV 생성
                D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
                DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
                DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;                       // Cube Map은 Texture2DArray로 처리
                DsvDesc.Texture2DArray.ArraySize = 1;                                             // 각 DSV는 하나의 면만 참조
                DsvDesc.Texture2DArray.FirstArraySlice = FaceIndex + CubeIndex * CUBE_FACE_COUNT; // 각 면에 대한 슬라이스 계산
                DsvDesc.Texture2DArray.MipSlice = 0;
                hr = Device->CreateDepthStencilView(CubeShadowMap.Get(), &DsvDesc, CubeDSV[CubeIndex][FaceIndex].ReleaseAndGetAddressOf());

                D3D11_TEXTURE2D_DESC DebugTexDesc = {};
                DebugTexDesc.Width = SHADOW_CUBE_SIZE;
                DebugTexDesc.Height = SHADOW_CUBE_SIZE;
                DebugTexDesc.MipLevels = 1;
                DebugTexDesc.ArraySize = 1;
                DebugTexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                DebugTexDesc.SampleDesc.Count = 1;
                DebugTexDesc.Usage = D3D11_USAGE_DEFAULT;
                DebugTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                hr = Device->CreateTexture2D(
                    &DebugTexDesc,
                    nullptr,
                    CubeDebugTexture[CubeIndex][FaceIndex].ReleaseAndGetAddressOf());

                D3D11_SHADER_RESOURCE_VIEW_DESC FaceSrvDesc = {};
                FaceSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
                FaceSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                FaceSrvDesc.Texture2D.MostDetailedMip = 0;
                FaceSrvDesc.Texture2D.MipLevels = 1;

                hr = Device->CreateShaderResourceView(
                    CubeDebugTexture[CubeIndex][FaceIndex].Get(),
                    &FaceSrvDesc,
                    CubeDebugSRV[CubeIndex][FaceIndex].ReleaseAndGetAddressOf());
            }
        }


        // Shader Resource View(SRV) 생성
        D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
        SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY; // Cube Map은 TextureCubeArray로 처리
        SrvDesc.TextureCubeArray.MostDetailedMip = 0;
        SrvDesc.TextureCubeArray.MipLevels = 1;
        SrvDesc.TextureCubeArray.First2DArrayFace = 0;
        SrvDesc.TextureCubeArray.NumCubes = MAX_SHADOW_CUBES;
        hr = Device->CreateShaderResourceView(CubeShadowMap.Get(), &SrvDesc, CubeSRV.ReleaseAndGetAddressOf());
    }

	bool AllocateCube(int32& OutCubeIndex) {
		if (CurrentCubeCount < MAX_SHADOW_CUBES)
        {
            OutCubeIndex = CurrentCubeCount;
            ++CurrentCubeCount;
            return true;
		}
		return false;
	}

	void FreeCube(int32 CubeIndex) {
		if (CubeIndex >= 0 && CubeIndex < MAX_SHADOW_CUBES)
		{
			// 실제로는 DSV와 SRV를 재사용할 수 있도록 관리하는 로직이 필요하지만, 간단히 카운트만 감소시키는 방식으로 구현
			--CurrentCubeCount;
		}
	}

	void FreeAllCubes() {
		CurrentCubeCount = 0;
	}

    bool IsValid() const { return CubeShadowMap != nullptr && CubeDSV != nullptr && CubeSRV != nullptr; }
};

struct FShadowAtlas
{	
	// vsm 사용시 해당 포맷 DXGI_FORMAT_R32G32_FLOAT로 변경 필요
    TComPtr<ID3D11Texture2D> ShadowMapAtlas;
    TComPtr<ID3D11DepthStencilView> ShadowDSV;
    TComPtr<ID3D11ShaderResourceView> ShadowSRV;

    TComPtr<ID3D11Texture2D> VarianceShadowTexture;
    TComPtr<ID3D11RenderTargetView> VarianceShadowRTV;
    TComPtr<ID3D11ShaderResourceView> VarianceShadowSRV;
    TComPtr<ID3D11UnorderedAccessView> VarianceShadowUAV;

	// Blur 처리에 필요한 임시 Texture ,SRV , UAV 
	TComPtr<ID3D11Texture2D> BlurIntermediateTexture;       // 8192x8192, R32G32_FLOAT
    TComPtr<ID3D11ShaderResourceView> BlurIntermediateSRV;  // Vertical CS 읽기
    TComPtr<ID3D11UnorderedAccessView> BlurIntermediateUAV; // Horizontal CS 출력


	// TODO: 타일의 크기가 정해져 있음, 이를 나중에 동적으로 조절할 수 있도록 개선해야 함
	// TODO: Tile 관리 방식 개선 필요. 현재는 간단히 bool 배열로 관리하지만, 더 효율적인 자료구조로 변경 고려 (예: 비트맵, 큐 등)
	
	// ShadowMap, VSM에 사용하고자 하는 Resource를 하나로 통일해야함
	// ShadowManager의 ShadowMapAtlas의 자원을 모두 사용하도록 통일할 것.
	void Initialize(ID3D11Device* Device)
    {
        if (Device == nullptr)
            return;

		// Atlas용 Depth Texture 생성
        D3D11_TEXTURE2D_DESC ShadowMapDesc = {};
        ShadowMapDesc.Width = ShadowAtlasResolution2D;
        ShadowMapDesc.Height = ShadowAtlasResolution2D;
        ShadowMapDesc.MipLevels = 1;
        ShadowMapDesc.ArraySize = 1;
        ShadowMapDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        ShadowMapDesc.SampleDesc.Count = 1;
        ShadowMapDesc.Usage = D3D11_USAGE_DEFAULT;
        ShadowMapDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = Device->CreateTexture2D(&ShadowMapDesc, nullptr, ShadowMapAtlas.ReleaseAndGetAddressOf());

		// Depth Stencil View(DSV) 생성
        D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
        DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        DsvDesc.Flags = 0;
        DsvDesc.Texture2D.MipSlice = 0;

        hr = Device->CreateDepthStencilView(ShadowMapAtlas.Get(), &DsvDesc, ShadowDSV.ReleaseAndGetAddressOf());

		// Shader Resource View(SRV) 생성
        D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
        SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SrvDesc.Texture2D.MostDetailedMip = 0;
        SrvDesc.Texture2D.MipLevels = 1;

        hr = Device->CreateShaderResourceView(ShadowMapAtlas.Get(), &SrvDesc, ShadowSRV.ReleaseAndGetAddressOf());

		// VSM용 Depth Texture 생성
        D3D11_TEXTURE2D_DESC VarianceTexDesc = {};
        VarianceTexDesc.Width = ShadowAtlasResolution2D;
        VarianceTexDesc.Height = ShadowAtlasResolution2D;
        VarianceTexDesc.MipLevels = 1;
        VarianceTexDesc.ArraySize = 1;
        VarianceTexDesc.Format = DXGI_FORMAT_R32G32_TYPELESS; // R32 -> 1차 모멘트, G32 -> 2차 모멘트
        VarianceTexDesc.SampleDesc.Count = 1;
        VarianceTexDesc.SampleDesc.Quality = 0;
        VarianceTexDesc.Usage = D3D11_USAGE_DEFAULT;
        VarianceTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        VarianceTexDesc.CPUAccessFlags = 0;
        VarianceTexDesc.MiscFlags = 0;

        hr = Device->CreateTexture2D(&VarianceTexDesc, nullptr, VarianceShadowTexture.ReleaseAndGetAddressOf());

        // VSM용 Render Target View(RTV) 생성
        D3D11_RENDER_TARGET_VIEW_DESC VarianceRTVDesc = {};
        VarianceRTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        VarianceRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        VarianceRTVDesc.Texture2D.MipSlice = 0;

        hr = Device->CreateRenderTargetView(VarianceShadowTexture.Get(), &VarianceRTVDesc, VarianceShadowRTV.ReleaseAndGetAddressOf());

        // VSM용 Shader Resource View(SRV) 생성
        D3D11_SHADER_RESOURCE_VIEW_DESC VarianceSRVDesc = {};
        VarianceSRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        VarianceSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        VarianceSRVDesc.Texture2D.MostDetailedMip = 0;
        VarianceSRVDesc.Texture2D.MipLevels = 1;

        hr = Device->CreateShaderResourceView(VarianceShadowTexture.Get(), &VarianceSRVDesc, VarianceShadowSRV.ReleaseAndGetAddressOf());

        // VSM용 Unordered Access View(UAV) 생성
        D3D11_UNORDERED_ACCESS_VIEW_DESC VarianceUAVDesc = {};
        VarianceUAVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        VarianceUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        VarianceUAVDesc.Texture2D.MipSlice = 0;

        hr = Device->CreateUnorderedAccessView(VarianceShadowTexture.Get(), &VarianceUAVDesc, VarianceShadowUAV.ReleaseAndGetAddressOf());


		// Blur Intermediate Texture 생성 (가로 blur 임시 저장용)
        D3D11_TEXTURE2D_DESC BlurIntermediateDesc = {};
        BlurIntermediateDesc.Width = ShadowAtlasResolution2D;
        BlurIntermediateDesc.Height = ShadowAtlasResolution2D;
        BlurIntermediateDesc.MipLevels = 1;
        BlurIntermediateDesc.ArraySize = 1;
        BlurIntermediateDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        BlurIntermediateDesc.SampleDesc.Count = 1;
        BlurIntermediateDesc.SampleDesc.Quality = 0;
        BlurIntermediateDesc.Usage = D3D11_USAGE_DEFAULT;
        BlurIntermediateDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        BlurIntermediateDesc.CPUAccessFlags = 0;
        BlurIntermediateDesc.MiscFlags = 0;

        hr = Device->CreateTexture2D(&BlurIntermediateDesc, nullptr, BlurIntermediateTexture.ReleaseAndGetAddressOf());

        // Blur Intermediate SRV 생성 (Vertical CS에서 읽기)
        D3D11_SHADER_RESOURCE_VIEW_DESC BlurSRVDesc = {};
        BlurSRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        BlurSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        BlurSRVDesc.Texture2D.MostDetailedMip = 0;
        BlurSRVDesc.Texture2D.MipLevels = 1;

        hr = Device->CreateShaderResourceView(BlurIntermediateTexture.Get(), &BlurSRVDesc, BlurIntermediateSRV.ReleaseAndGetAddressOf());

        // Blur Intermediate UAV 생성 (Horizontal CS에서 쓰기)
        D3D11_UNORDERED_ACCESS_VIEW_DESC BlurUAVDesc = {};
        BlurUAVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        BlurUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        BlurUAVDesc.Texture2D.MipSlice = 0;

        hr = Device->CreateUnorderedAccessView(BlurIntermediateTexture.Get(), &BlurUAVDesc, BlurIntermediateUAV.ReleaseAndGetAddressOf());

	}

    bool IsValid() const { return ShadowMapAtlas != nullptr && ShadowDSV != nullptr && ShadowSRV != nullptr; }
    bool IsValidVSM() const { return VarianceShadowTexture != nullptr && VarianceShadowRTV != nullptr && VarianceShadowSRV != nullptr && VarianceShadowUAV != nullptr; }
};

class FShadowAtlasManager : public TSingleton<FShadowAtlasManager>
{
	friend class TSingleton<FShadowAtlasManager>;
public:
	void Initialize(ID3D11Device* InDevice);
    //void VSMInitialize(ID3D11Device* Device);
	
	

    bool AllocateTile(int32 ResolutionScale, FShadowAtlasTile& OutTile);
    bool FreeTile(const int32& TileX, const int32& TileY, const int32& TileSize);
    void ClearTiles() { 
		ShadowAllocator.FreeAllTiles(); 
		/*if (RootNode)
		{
			RootNode->FreeAll();
		}*/
	}
	void GetDebugLeafRects(TArray<FShadowAtlasDebugRect>& OutRects) const
	{
		OutRects.clear();
		/*if (RootNode)
		{
			RootNode->CollectLeafRects(OutRects);
		}*/
	}
    bool AllocateTile(int32 ResolutionScale, FShadowAtlasTile& OutTile);
    bool FreeTile(const int32& TileIndex);
    void ClearTiles() { ShadowAllocator.FreeAllTiles(); }

    bool AllocateTileCube(int32& OutCubeIndex);
    //bool FreeTileCube(const int32& CubeIndex);
    void ClearTilesCube() { ShadowCubeMapArray.FreeAllCubes(); }
    void UpdateCubeDebugFace(ID3D11DeviceContext* DeviceContext, int32 CubeIndex, int32 FaceIndex);


	// OpaquePass, BlurPass에서 꺼내 쓸 getter
    ID3D11Texture2D* GetVarianceTexture2D() const { return ShadowMapAtlas.VarianceShadowTexture.Get(); }
	ID3D11RenderTargetView* GetVarianceRTV() const { return ShadowMapAtlas.VarianceShadowRTV.Get(); }
    ID3D11ShaderResourceView* GetVarianceSRV() const { return ShadowMapAtlas.VarianceShadowSRV.Get(); }
    ID3D11UnorderedAccessView* GetVarianceUAV() const { return ShadowMapAtlas.VarianceShadowUAV.Get(); }

    ID3D11ShaderResourceView* GetBlurSRV() const { return ShadowMapAtlas.BlurIntermediateSRV.Get(); }
    ID3D11UnorderedAccessView* GetBlurUAV() const { return ShadowMapAtlas.BlurIntermediateUAV.Get(); }



    ID3D11DepthStencilView* GetDSV() const { return ShadowMapAtlas.ShadowDSV.Get(); }
    ID3D11ShaderResourceView* GetSRV() const { return ShadowMapAtlas.ShadowSRV.Get(); }
    ID3D11Texture2D* GetAtlas() const { return ShadowMapAtlas.ShadowMapAtlas.Get(); }

	
    ID3D11ShaderResourceView* GetCubeSRV() const { return ShadowCubeMapArray.CubeSRV.Get(); }
    ID3D11Texture2D* GetCubeAtlas() const { return ShadowCubeMapArray.CubeShadowMap.Get(); }
    uint32 GetAllocatedCubeCount() const { return ShadowCubeMapArray.CurrentCubeCount; }
	ID3D11DepthStencilView* GetCubeDSV(int32 CubeIndex, int32 FaceIndex) const;
    ID3D11ShaderResourceView* GetCubeDebugSRV(int32 CubeIndex, int32 FaceIndex) const;


	TArray<FShadowAtlasTile> GetAllocatedTiles() const;


    TComPtr<ID3D11Device> Device;
    TComPtr<ID3D11Texture2D> ShadowMap;
    TComPtr<ID3D11DepthStencilView> ShadowDSV;
    TComPtr<ID3D11ShaderResourceView> ShadowSRV;



	float ClearColor[4] = { 0.f, 0.f, 0.f, 0.f };

private:


	FShadowAtlas ShadowMapAtlas;
    FShadowAtlasAllocator ShadowAllocator;
    Node* RootNode = nullptr;

	FShadowAtlasCube ShadowCubeMapArray;
};
