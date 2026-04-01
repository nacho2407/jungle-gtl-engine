#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/TextMeshBuilder.h"
#include "Renderer/SubUVRenderer.h"
#include "ShaderManager.h"
#include <d3d11.h>
#include <filesystem>
#include <functional>
#include <memory>

struct FVertex;
struct FRenderMesh;
class FPixelShader;
class FMaterial;
class UScene;

using FGUICallback = std::function<void()>;
class FRenderer;
using FPostRenderCallback = std::function<void(FRenderer*)>;

struct FOutlineRenderItem
{
	FRenderMesh* Mesh = nullptr;
	FMatrix WorldMatrix = FMatrix::Identity;
};

/**
 * ?붿쭊???듭떖 ?뚮뜑留??쒖뒪??
 * ?뚮뜑留??뺤콉???곕씪 ?쒖텧??紐낅졊?ㅼ쓣 GPU?먯꽌 ?ㅽ뻾??
 */
class ENGINE_API FRenderer
{
public:
	FRenderer(HWND InHwnd, int32 InWidth, int32 InHeight);
	~FRenderer();

	/** ?쒖뒪??珥덇린??諛?D3D11 ?μ튂 ?앹꽦 */
	bool Initialize(HWND InHwnd, int32 InWidth, int32 InHeight);
	
	/** ?꾨젅???쒖옉 泥섎━ (?뚮뜑 ?寃??대━???? */
	void BeginFrame();
	
	/** ?꾨젅??醫낅즺 泥섎━ (Present) */
	void EndFrame();
	
	/** 由ъ냼???댁젣 */
	void Release();
	
	/** ?붾㈃ 媛由??щ? ?뺤씤 */
	bool IsOccluded();
	
	/** 酉고룷???ш린 蹂寃????*/
	void OnResize(int32 NewWidth, int32 NewHeight);
	
	/** ???뚮뜑 ?寃??ㅼ젙 (?몃? ?ㅻ쾭?쇱씠?쒖슜) */
	void SetSceneRenderTarget(ID3D11RenderTargetView* InRenderTargetView, ID3D11DepthStencilView* InDepthStencilView, const D3D11_VIEWPORT& InViewport);
	void ClearSceneRenderTarget();

	/** 硫?곕럭?ы듃 ???⑥뒪 */
	void BeginScenePass(ID3D11RenderTargetView* InRTV, ID3D11DepthStencilView* InDSV, const D3D11_VIEWPORT& InVP);
	void EndScenePass();
	void BindSwapChainRTV();

	void SetVSync(bool bEnable) { bVSyncEnabled = bEnable; }
	bool IsVSyncEnabled() const { return bVSyncEnabled; }

	bool bSwapChainOccluded = false;
	// ??? GUI 諛?肄쒕갚 ???
	/** ImGui ???몃? GUI ?쒖뒪???곕룞??肄쒕갚 */
	void SetGUICallbacks(FGUICallback InInit, FGUICallback InShutdown, FGUICallback InNewFrame, FGUICallback InRender, FGUICallback InPostPresent = nullptr);
	void ClearViewportCallbacks();
	void SetGUIUpdateCallback(FGUICallback InUpdate);
	void SetPostRenderCallback(FPostRenderCallback InCallback) { PostRenderCallback = std::move(InCallback); }

	// ??? 紐낅졊 ?ㅽ뻾 ???
	/** 而ㅻ㎤?????쒖텧 諛?GPU 踰꾪띁 ?낅뜲?댄듃 */
	void SubmitCommands(const FRenderCommandQueue& Queue);
	
	/** ?섏쭛??而ㅻ㎤???뺣젹 諛??ㅽ뻾 */
	void ExecuteCommands();
	
	/** ?뱀젙 ?덉씠?댁쓽 紐낅졊?ㅼ쓣 ?ㅼ젣 ?쒕줈??肄쒕줈 蹂??*/
	void ExecuteRenderPass(ERenderLayer RenderLayer);

	// ??? ?붾쾭洹?諛??쇱씤 ?뚮뜑留????
	void DrawLine(const FVector& Start, const FVector& End, const FVector4& Color);
	void DrawCube(const FVector& Center, const FVector& BoxExtent, const FVector4& Color);
	void ExecuteLineCommands();

	// ??? ?뱀닔 ?④낵 ???
	/** ?좏깮???ㅻ툕?앺듃 ?깆쓽 ?꾩썐?쇱씤 ?뚮뜑留?*/
	bool InitOutlineResources();
	void RenderOutlines(const TArray<FOutlineRenderItem>& Items);

	// Texture ?앹꽦???꾪빐 ?곕줈 類륁쓬. - 異뷀썑 TextureManager 由ы럺?좊쭅???꾩꽦?섎㈃ ?꾩슂 ?놁뼱吏덇쾬.
	bool CreateTextureFromSTB(ID3D11Device* Device, const char* FilePath, ID3D11ShaderResourceView** OutSRV);
	bool CreateTextureFromSTB(ID3D11Device* Device, const std::filesystem::path& FilePath, ID3D11ShaderResourceView** OutSRV);

	// ??? ?묎렐?????
	FMaterial* GetDefaultMaterial() const { return DefaultMaterial.get(); }
	FMaterial* GetDefaultTextureMaterial() const { return DefaultTextureMaterial.get(); }
	size_t GetPrevCommandCount() const { return PrevCommandCount; }
	std::unique_ptr<FRenderStateManager>& GetRenderStateManager() { return RenderStateManager; }
	ID3D11Device* GetDevice() const { return Device; }
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }
	ID3D11RenderTargetView* GetRenderTargetView() const { return RenderTargetView; }
	IDXGISwapChain* GetSwapChain() const { return SwapChain; };
	HWND GetHwnd() const { return Hwnd; }

	FTextMeshBuilder& GetTextRenderer() { return TextRenderer; }
	FSubUVRenderer& GetSubUVRenderer() { return SubUVRenderer; }
	FVector GetCameraPosition() const;

	ID3D11ShaderResourceView* GetFolderIconSRV() const { return FolderIconSRV; }
	ID3D11ShaderResourceView* GetFileIconSRV() const { return FileIconSRV; }

private:
	void SetConstantBuffers();
	void AddCommand(const FRenderCommand& Command);
	void ClearCommandList();
	bool CreateDeviceAndSwapChain(HWND InHwnd, int32 Width, int32 Height);
	bool CreateRenderTargetAndDepthStencil(int32 Width, int32 Height);
	bool CreateConstantBuffers();
	bool CreateSamplers();
	bool EnsureOutlineMaskResources(uint32 Width, uint32 Height);
	void ReleaseOutlineMaskResources();
	void UpdateFrameConstantBuffer();
	void UpdateObjectConstantBuffer(const FMatrix& WorldMatrix);
	void UpdateOutlinePostConstantBuffer(const FVector4& OutlineColor, float OutlineThickness, float OutlineThreshold);
	void ClearDepthBuffer();

private:
	std::unique_ptr<FRenderStateManager> RenderStateManager = nullptr;

	HWND Hwnd = nullptr;
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;
	ID3D11RenderTargetView* RenderTargetView = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;
	
	ID3D11Buffer* FrameConstantBuffer = nullptr;
	ID3D11Buffer* ObjectConstantBuffer = nullptr;
	ID3D11Buffer* OutlinePostConstantBuffer = nullptr;
	
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	D3D11_VIEWPORT Viewport = {};
	
	ID3D11RenderTargetView* SceneRenderTargetView = nullptr;
	ID3D11DepthStencilView* SceneDepthStencilView = nullptr;
	D3D11_VIEWPORT SceneViewport = {};
	bool bUseSceneRenderTargetOverride = false;
	bool bVSyncEnabled = false;

	/** ?듯빀???뚮뜑留?紐낅졊 由ъ뒪??*/
	TArray<FRenderCommand> CommandList;
	size_t PrevCommandCount = 0;
	uint64 NextSubmissionOrder = 0;

	/** ?쇱씤 ?뚮뜑留곸슜 ?꾩떆 由ъ냼??*/
	TArray<FVertex> LineVertices;
	ID3D11Buffer* LineVertexBuffer = nullptr;
	UINT LineVertexBufferSize = 0;

	/** ?꾩썐?쇱씤(?ㅽ뀗?? 由ъ냼??*/
	ID3D11DepthStencilState* StencilWriteState = nullptr;
	ID3D11DepthStencilState* StencilEqualState = nullptr;
	ID3D11DepthStencilState* StencilNotEqualState = nullptr;
	ID3D11BlendState* OutlineBlendState = nullptr;
	ID3D11RasterizerState* OutlineRasterizerState = nullptr;
	ID3D11SamplerState* OutlineSampler = nullptr;
	ID3D11VertexShader* OutlinePostVS = nullptr;
	ID3D11PixelShader* OutlineMaskPS = nullptr;
	ID3D11PixelShader* OutlineSobelPS = nullptr;
	ID3D11Texture2D* OutlineMaskTexture = nullptr;
	ID3D11RenderTargetView* OutlineMaskRTV = nullptr;
	ID3D11ShaderResourceView* OutlineMaskSRV = nullptr;
	uint32 OutlineMaskWidth = 0;
	uint32 OutlineMaskHeight = 0;

	FGUICallback GUIInit;
	FGUICallback GUIShutdown;
	FGUICallback GUINewFrame;
	FGUICallback GUIUpdate;
	FGUICallback GUIRender;
	FGUICallback GUIPostPresent;
	FPostRenderCallback PostRenderCallback;

	/** 湲곕낯 怨듭쑀 由ъ냼??*/
	std::shared_ptr<FMaterial> DefaultMaterial;
	std::shared_ptr<FMaterial> DefaultTextureMaterial;

	FTextMeshBuilder TextRenderer;
	FSubUVRenderer SubUVRenderer;

	ID3D11ShaderResourceView* FolderIconSRV = nullptr;
	ID3D11ShaderResourceView* FileIconSRV = nullptr;

	/** SubUV, Text ?댁쇅 ?쇰컲 material texture sample ?⑸룄 */
	ID3D11SamplerState* NormalSampler = nullptr;

public:
	FShaderManager ShaderManager;
};
