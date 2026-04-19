#include "Render/Builders/Fullscreen/FullscreenDrawCommandBuilder.h"
#include "Render/Core/PassRenderState.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Commands/DrawCommand.h"
#include "Render/Resource/Managers/ShaderManager.h"
void FFullscreenDrawCommandBuilder::Build(ERenderPass Pass, FRenderPassContext& Context, FDrawCommandList& OutList, uint16 UserBits){ FShader* Shader=nullptr; switch(Pass){ case ERenderPass::Lighting: Shader=FShaderManager::Get().GetShader(EShaderType::StaticMesh); break; case ERenderPass::FXAA: Shader=FShaderManager::Get().GetShader(EShaderType::FXAA); break; default: Shader=nullptr; break;} if(!Shader) return; const FPassRenderState& S=Context.GetPassState(Pass); FDrawCommand& Cmd=OutList.AddCommand(); Cmd.Shader=Shader; Cmd.DepthStencil=S.DepthStencil; Cmd.Blend=S.Blend; Cmd.Rasterizer=S.Rasterizer; Cmd.Topology=S.Topology; Cmd.VertexCount=3; Cmd.Pass=Pass; Cmd.SortKey=FDrawCommand::BuildSortKey(Pass,Shader,nullptr,nullptr,UserBits); }
