#include "Render/Builders//TextDrawCommandBuilder.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Scene/TextRenderSceneProxy.h"

void FTextDrawCommandBuilder::BuildOverlay(FRenderPassContext&, FDrawCommandList&) {}
void FTextDrawCommandBuilder::BuildWorld(const FTextRenderSceneProxy&, FRenderPassContext&, FDrawCommandList&) {}
