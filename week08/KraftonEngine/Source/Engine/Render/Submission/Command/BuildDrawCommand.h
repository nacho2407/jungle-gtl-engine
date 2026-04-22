#pragma once

#include "Render/Execute/Passes/Base/RenderPassTypes.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"

class FPrimitiveSceneProxy;
class FTextRenderSceneProxy;
class FDrawCommandList;
struct FRenderPipelineContext;

namespace DrawCommandBuilder
{
    // �Ϲ� �޽� ���Ͻø� ��ο� Ŀ�ǵ�� ��ȯ�մϴ�.
    void BuildMeshDrawCommand(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList);

    // Ǯ��ũ�� �н��� ��ο� Ŀ�ǵ带 �����մϴ�.
    void BuildFullscreenDrawCommand(ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList, EViewModePostProcessVariant PostProcessVariant = EViewModePostProcessVariant::None);

    // ����� ���� ��ġ�� ��ο� Ŀ�ǵ�� ��ȯ�մϴ�.
    void BuildLineDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

    // ������ helper billboard�� �������� ��ο� Ŀ�ǵ带 �����մϴ�.
    void BuildOverlayBillboardDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

    // ȭ�� �������� �ؽ�Ʈ�� editor helper world text�� ��ο� Ŀ�ǵ带 �����մϴ�.
    void BuildOverlayTextDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

    // ���� �ؽ�Ʈ ���Ͻø� ��ο� Ŀ�ǵ�� ��ȯ�մϴ�.
    void BuildWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);
    void BuildOverlayWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);

    // ��Į ���Ͻø� ��ο� Ŀ�ǵ�� ��ȯ�մϴ�.
    void BuildDecalDrawCommand(const FPrimitiveSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);
}
