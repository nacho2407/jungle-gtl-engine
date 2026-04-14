#pragma once
#include "Core/CoreMinimal.h"
#include "RenderPassContext.h"

class FFXAARenderPass;
class FFogRenderPass;
class FLightRenderPass;
class FDecalRenderPass;
class FFontRenderPass;
class FSubUVRenderPass;
class FTranslucentRenderPass;
class FSelectionMaskRenderPass;
class FGridRenderPass;
class FEditorRenderPass;
class FDepthLessRenderPass;
class FPostProcessOutlineRenderPass;
class FOpaqueRenderPass;
class FBaseRenderPass;

class FRenderPipeline
{
public:
    bool Initialize();
    bool Render(const FRenderPassContext* Context);
    void Release();

	ID3D11ShaderResourceView* GetOutSRV() const { return OutSRV; }

private:
    std::shared_ptr<FOpaqueRenderPass> OpaqueRenderPass;
    std::shared_ptr<FDecalRenderPass> DecalRenderPass;
    std::shared_ptr<FLightRenderPass> LightRenderPass;
    std::shared_ptr<FFogRenderPass> FogRenderPass;
    std::shared_ptr<FFXAARenderPass> FXAARenderPass;
    std::shared_ptr<FFontRenderPass> FontRenderPass;
    std::shared_ptr<FSubUVRenderPass> SubUVRenderPass;
    std::shared_ptr<FTranslucentRenderPass> TranslucentRenderPass;
    std::shared_ptr<FSelectionMaskRenderPass> SelectionMaskRenderPass;
    std::shared_ptr<FGridRenderPass> GridRenderPass;
    std::shared_ptr<FEditorRenderPass> EditorRenderPass;
    std::shared_ptr<FDepthLessRenderPass> DepthLessRenderPass;
    std::shared_ptr<FPostProcessOutlineRenderPass> PostProcessOutlineRenderPass;
    ID3D11ShaderResourceView* OutSRV = nullptr;
    ID3D11RenderTargetView* OutRTV = nullptr;

	TArray<std::shared_ptr<FBaseRenderPass>> RenderPasses;
};
