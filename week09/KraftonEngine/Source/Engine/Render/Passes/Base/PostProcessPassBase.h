#pragma once

#include "Render/Passes/Base/FullscreenPassBase.h"
#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"

/*
    fullscreen pass 위에 얹는 후처리 공통 베이스 클래스입니다.
    viewport 결과를 SceneColor/Depth 복사본으로 만들고, 후처리 입력 SRV 바인딩/정리를 공통화합니다.
*/
class FPostProcessPassBase : public FFullscreenPassBase
{
public:
    void PrepareTargets(FRenderPipelineContext& Context) override
    {
        BindViewportTarget(Context);
    }

protected:
    void UnbindTargetsForCopy(FRenderPipelineContext& Context) const
    {
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

        if (Context.StateCache)
        {
            Context.StateCache->RTV = nullptr;
            Context.StateCache->DSV = nullptr;
            Context.StateCache->bForceAll = true;
        }
    }

    void CopyViewportColorToSceneColor(FRenderPipelineContext& Context) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        if (!Targets || !Targets->ViewportRenderTexture || !Targets->SceneColorCopyTexture ||
            Targets->ViewportRenderTexture == Targets->SceneColorCopyTexture)
        {
            return;
        }

        UnbindTargetsForCopy(Context);
        Context.Context->CopyResource(Targets->SceneColorCopyTexture, Targets->ViewportRenderTexture);
    }

    void CopyViewportDepthToReadable(FRenderPipelineContext& Context) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        if (!Targets || !Targets->DepthTexture || !Targets->DepthCopyTexture || Targets->DepthTexture == Targets->DepthCopyTexture)
        {
            return;
        }

        UnbindTargetsForCopy(Context);
        Context.Context->CopyResource(Targets->DepthCopyTexture, Targets->DepthTexture);
    }

    void BindSceneColorInput(FRenderPipelineContext& Context, bool bBindSlot0 = false) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        if (!Targets || !Targets->SceneColorCopySRV)
        {
            return;
        }

        ID3D11ShaderResourceView* SceneColorSRV = Targets->SceneColorCopySRV;
        if (bBindSlot0)
        {
            Context.Context->PSSetShaderResources(0, 1, &SceneColorSRV);
        }
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &SceneColorSRV);
    }

    void BindDepthInput(FRenderPipelineContext& Context) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        if (!Targets || !Targets->DepthCopySRV)
        {
            return;
        }

        ID3D11ShaderResourceView* DepthSRV = Targets->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }

    void BindStencilInput(FRenderPipelineContext& Context) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        if (!Targets || !Targets->StencilCopySRV)
        {
            return;
        }

        ID3D11ShaderResourceView* StencilSRV = Targets->StencilCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &StencilSRV);
    }

    void Cleanup(FRenderPipelineContext& Context) override
    {
        ID3D11ShaderResourceView* NullSRV = nullptr;
        Context.Context->PSSetShaderResources(0, 1, &NullSRV);
        Context.Context->PSSetShaderResources(1, 1, &NullSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &NullSRV);

        if (Context.StateCache)
        {
            Context.StateCache->DiffuseSRV = nullptr;
            Context.StateCache->NormalSRV = nullptr;
            Context.StateCache->SpecularSRV = nullptr;
            Context.StateCache->PerShaderCB[0] = nullptr;
            Context.StateCache->PerShaderCB[1] = nullptr;
            Context.StateCache->bForceAll = true;
        }
    }
};
