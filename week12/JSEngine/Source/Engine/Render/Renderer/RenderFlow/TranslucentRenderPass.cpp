#include "TranslucentRenderPass.h"
#include "GeometryDrawPacket.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderHelper.h"
#include "Render/Resource/VertexFactoryTypes.h"
#include "Component/PrimitiveComponent.h"
#include "Particle/ParticleTypes.h"
#include "Core/Logging/Stats.h"
#include "Core/ResourceManager.h"

#include <algorithm>

namespace
{
	FShaderProgram* GetShaderProgram(const FRenderCommand& Cmd, uint32 PermutationKey)
	{
		if (!Cmd.Material)
		{
			return nullptr;
		}
		if (Cmd.Material->GetShaderType() == EMaterialShaderType::None)
		{
			UE_LOG_WARNING("[Render] ShaderType None material cannot be drawn by TranslucentRenderPass: %s", Cmd.Material->GetName().c_str());
			return nullptr;
		}

		const FVertexFactoryDesc& VertexFactoryDesc = FVertexFactoryRegistry::Get(Cmd.VertexFactoryType);

		FShaderStageKey VSKey;
		VSKey.FilePath = VertexFactoryDesc.VertexShaderPath;
		VSKey.EntryPoint = VertexFactoryDesc.BasePassVSEntry;
		VSKey.Target = "vs_5_0";
		VSKey.PermutationKey = PermutationKey;

		FShaderStageKey PSKey;
		PSKey.FilePath = Cmd.Material->GetPixelShaderPath();
		PSKey.EntryPoint = Cmd.Material->GetPixelShaderEntryPoint();
		PSKey.Target = "ps_5_0";
		PSKey.PermutationKey = PermutationKey;

		TArray<D3D_SHADER_MACRO> Macros = FShaderHelper::BuildUberLitMacros(PermutationKey);
		return FResourceManager::Get().GetOrCreateShaderProgram(
			VSKey,
			PSKey,
			Macros.data(),
			Macros.data(),
			&VertexFactoryDesc.VertexLayout);
	}

	FShaderProgram* GetParticleSpriteShaderProgram()
	{
		const FVertexFactoryDesc& ParticleSpriteDesc = FVertexFactoryRegistry::Get(EVertexFactoryType::ParticleSprite);

		FShaderStageKey VSKey;
		VSKey.FilePath = ParticleSpriteDesc.VertexShaderPath;
		VSKey.EntryPoint = ParticleSpriteDesc.BasePassVSEntry;
		VSKey.Target = "vs_5_0";

		FShaderStageKey PSKey;
		PSKey.FilePath = FShaderPaths::VFXParticle;
		PSKey.EntryPoint = "PS";
		PSKey.Target = "ps_5_0";

		return FResourceManager::Get().GetOrCreateShaderProgram(
			VSKey,
			PSKey,
			nullptr,
			nullptr,
			&ParticleSpriteDesc.VertexLayout);
	}

	FShaderProgram* GetParticleBeamShaderProgram()
	{
		const FVertexFactoryDesc& ParticleBeamDesc = FVertexFactoryRegistry::Get(EVertexFactoryType::ParticleBeam);

		FShaderStageKey VSKey;
		VSKey.FilePath = ParticleBeamDesc.VertexShaderPath;
		VSKey.EntryPoint = ParticleBeamDesc.BasePassVSEntry;
		VSKey.Target = "vs_5_0";

		FShaderStageKey PSKey;
		PSKey.FilePath = FShaderPaths::VFXParticleBeam;
		PSKey.EntryPoint = "PS";
		PSKey.Target = "ps_5_0";

		return FResourceManager::Get().GetOrCreateShaderProgram(
			VSKey,
			PSKey,
			nullptr,
			nullptr,
			&ParticleBeamDesc.VertexLayout);
	}

	FShaderProgram* GetParticleRibbonShaderProgram()
	{
		const FVertexFactoryDesc& ParticleRibbonDesc = FVertexFactoryRegistry::Get(EVertexFactoryType::ParticleRibbon);

		FShaderStageKey VSKey;
		VSKey.FilePath = ParticleRibbonDesc.VertexShaderPath;
		VSKey.EntryPoint = ParticleRibbonDesc.BasePassVSEntry;
		VSKey.Target = "vs_5_0";

		FShaderStageKey PSKey;
		PSKey.FilePath = FShaderPaths::VFXParticleRibbon;
		PSKey.EntryPoint = "PS";
		PSKey.Target = "ps_5_0";

		return FResourceManager::Get().GetOrCreateShaderProgram(
			VSKey,
			PSKey,
			nullptr,
			nullptr,
			&ParticleRibbonDesc.VertexLayout);
	}

	bool IsParticleSpriteCommand(const FRenderCommand& Cmd)
	{
		return Cmd.Type == ERenderCommandType::Particle
			&& Cmd.VertexFactoryType == EVertexFactoryType::ParticleSprite
			&& Cmd.HasInstanceBuffer();
	}

	bool IsParticleBeamCommand(const FRenderCommand& Cmd)
	{
		return Cmd.Type == ERenderCommandType::Particle
			&& Cmd.VertexFactoryType == EVertexFactoryType::ParticleBeam
			&& Cmd.HasInstanceBuffer();
	}


	bool IsParticleRibbonCommand(const FRenderCommand& Cmd)
	{
		return Cmd.Type == ERenderCommandType::Particle
			&& Cmd.VertexFactoryType == EVertexFactoryType::ParticleRibbon
			&& Cmd.HasInstanceBuffer();
	}

	bool IsInstancedSurfaceCommand(const FRenderCommand& Cmd)
	{
		return SupportsInstancedSurfaceVertexFactory(Cmd.VertexFactoryType)
			&& Cmd.HasInstanceBuffer();
	}

	bool IsParticleEmitterCommand(const FRenderCommand& Cmd)
	{
		return Cmd.Type == ERenderCommandType::Particle
			&& Cmd.SourcePrimitive != nullptr
			&& Cmd.ParticleEmitterData != nullptr;
	}

	bool IsSameParticleSystemCommand(const FRenderCommand& A, const FRenderCommand& B)
	{
		return IsParticleEmitterCommand(A)
			&& IsParticleEmitterCommand(B)
			&& A.SourcePrimitive == B.SourcePrimitive;
	}

	void BindDefaultTranslucentStates(const FRenderPassContext* Context)
	{
		ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
		ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::DepthReadOnly);
		ID3D11BlendState* BlendState = FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
		ID3D11RasterizerState* RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(
			Context->RenderBus->GetViewMode() == EViewMode::Wireframe ? ERasterizerType::WireFrame : ERasterizerType::SolidBackCull);
		ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
		DeviceContext->OMSetDepthStencilState(DepthState, 0);
		DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);
		DeviceContext->RSSetState(RasterizerState);
		DeviceContext->PSSetSamplers(0, 1, &Sampler);
	}

	bool BuildParticleSpriteDrawResources(
		const FRenderPassContext* Context,
		const FRenderCommand& Cmd,
		FGeometryDrawPacket& OutDraw)
	{
		const FParticleSpriteQuadResource QuadResource =
			FResourceManager::Get().GetOrCreateParticleSpriteQuadResource(Context->Device);
		if (!QuadResource.IsValid())
		{
			return false;
		}

		OutDraw.VertexBuffers[0] = QuadResource.VertexBuffer;
		OutDraw.VertexBuffers[1] = Cmd.InstanceBufferView.Buffer;
		OutDraw.Strides[0] = QuadResource.VertexStride;
		OutDraw.Strides[1] = Cmd.InstanceBufferView.Stride;
		OutDraw.Offsets[0] = 0;
		OutDraw.Offsets[1] = Cmd.InstanceBufferView.Offset;
		OutDraw.VertexBufferCount = 2;
		OutDraw.IndexBuffer = QuadResource.IndexBuffer;
		OutDraw.IndexCount = QuadResource.IndexCount;
		OutDraw.InstanceCount = Cmd.InstanceBufferView.InstanceCount;
		OutDraw.bInstanced = true;
		return true;
	}

	bool BuildParticleBeamDrawResources(
		const FRenderPassContext* Context,
		const FRenderCommand& Cmd,
		FGeometryDrawPacket& OutDraw)
	{
		const FParticleSpriteQuadResource QuadResource =
			FResourceManager::Get().GetOrCreateParticleSpriteQuadResource(Context->Device);
		if (!QuadResource.IsValid())
		{
			UE_LOG_WARNING("[Particle] Beam draw skipped because the shared quad resource is missing.");
			return false;
		}

		OutDraw.VertexBuffers[0] = QuadResource.VertexBuffer;
		OutDraw.VertexBuffers[1] = Cmd.InstanceBufferView.Buffer;
		OutDraw.Strides[0] = QuadResource.VertexStride;
		OutDraw.Strides[1] = Cmd.InstanceBufferView.Stride;
		OutDraw.Offsets[0] = 0;
		OutDraw.Offsets[1] = Cmd.InstanceBufferView.Offset;
		OutDraw.VertexBufferCount = 2;
		OutDraw.IndexBuffer = QuadResource.IndexBuffer;
		OutDraw.IndexCount = QuadResource.IndexCount;
		OutDraw.InstanceCount = Cmd.InstanceBufferView.InstanceCount;
		OutDraw.bInstanced = true;
		return true;
	}


	bool BuildParticleRibbonDrawResources(
		const FRenderPassContext* Context,
		const FRenderCommand& Cmd,
		FGeometryDrawPacket& OutDraw)
	{
		const FParticleSpriteQuadResource QuadResource =
			FResourceManager::Get().GetOrCreateParticleSpriteQuadResource(Context->Device);
		if (!QuadResource.IsValid())
		{
			UE_LOG_WARNING("[Particle] Ribbon draw skipped because the shared quad resource is missing.");
			return false;
		}

		OutDraw.VertexBuffers[0] = QuadResource.VertexBuffer;
		OutDraw.VertexBuffers[1] = Cmd.InstanceBufferView.Buffer;
		OutDraw.Strides[0] = QuadResource.VertexStride;
		OutDraw.Strides[1] = Cmd.InstanceBufferView.Stride;
		OutDraw.Offsets[0] = 0;
		OutDraw.Offsets[1] = Cmd.InstanceBufferView.Offset;
		OutDraw.VertexBufferCount = 2;
		OutDraw.IndexBuffer = QuadResource.IndexBuffer;
		OutDraw.IndexCount = QuadResource.IndexCount;
		OutDraw.InstanceCount = Cmd.InstanceBufferView.InstanceCount;
		OutDraw.bInstanced = true;
		return true;
	}

	void BindParticleBeamRasterizerState(const FRenderPassContext* Context)
	{
		ID3D11RasterizerState* RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(
			Context->RenderBus->GetViewMode() == EViewMode::Wireframe ? ERasterizerType::WireFrame : ERasterizerType::SolidNoCull);
		Context->DeviceContext->RSSetState(RasterizerState);
	}

	float CalculateTranslucentSortKey(
		const FRenderCommand& Cmd,
		const FVector& CameraPosition,
		const FVector& CameraForward)
	{
		const FAABB& ComponentAABB = IsParticleEmitterCommand(Cmd)
			? Cmd.SourcePrimitive->GetWorldAABB()
			: Cmd.WorldAABB;
		const FVector Center = ComponentAABB.IsValid()
			? ComponentAABB.GetCenter()
			: Cmd.PerObjectConstants.Model.GetOrigin();
		const FVector Delta = Center - CameraPosition;
		return FVector::DotProduct(Delta, CameraForward);
	}
} // namespace

bool FTranslucentRenderPass::Initialize()
{
	return true;
}

bool FTranslucentRenderPass::Release()
{
	return true;
}

bool FTranslucentRenderPass::Begin(const FRenderPassContext* Context)
{
	if (IsDebugViewMode(Context->RenderBus->GetViewMode()))
	{
		OutSRV = PrevPassSRV;
		OutRTV = PrevPassRTV;
		return true;
	}

	ID3D11RenderTargetView* RTV = PrevPassRTV;
	ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
	Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
	Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	OutSRV = PrevPassSRV;
	OutRTV = PrevPassRTV;

	return true;
}

bool FTranslucentRenderPass::DrawCommand(const FRenderPassContext* Context)
{
	// 현재 Translucent가 렌더링되어야 하는 DebugViewMode 없음
	if (IsDebugViewMode(Context->RenderBus->GetViewMode()))
	{
		return true;
	}
	
	const TArray<FRenderCommand> Commands = SortTranslucentCommands(Context);
	if (Commands.empty())
	{
		return true;
	}

	for (const FRenderCommand& Cmd : Commands)
	{
		if (!DrawEachCommand(Context, Cmd)) 
			continue;        // Draw 실패 시 동작 추가할 거면 여기에 추가
	}

	return true;
}

bool FTranslucentRenderPass::End(const FRenderPassContext* Context)
{
	return true;
}

TArray<FRenderCommand> FTranslucentRenderPass::SortTranslucentCommands(const FRenderPassContext* Context)
{
	const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::Translucent);
	TArray<size_t> SortedIndices;
	SortedIndices.reserve(Commands.size());
	for (size_t CommandIndex = 0; CommandIndex < Commands.size(); ++CommandIndex)
	{
		SortedIndices.push_back(CommandIndex);
	}

	const FVector CameraPosition = Context->RenderBus->GetCameraPosition();
	const FVector CameraForward = Context->RenderBus->GetCameraForward();
	{
		SCOPE_STAT("Translucent.Sort");
		std::stable_sort(SortedIndices.begin(), SortedIndices.end(),
			[&Commands, &CameraPosition, &CameraForward](size_t AIndex, size_t BIndex)
			{
				const FRenderCommand& A = Commands[AIndex];
				const FRenderCommand& B = Commands[BIndex];
				if (IsSameParticleSystemCommand(A, B))
				{
					return A.ParticleEmitterData->EmitterIndex < B.ParticleEmitterData->EmitterIndex;
				}

				return CalculateTranslucentSortKey(A, CameraPosition, CameraForward) >
					CalculateTranslucentSortKey(B, CameraPosition, CameraForward);
			});
	}

	TArray<FRenderCommand> SortedCommands;
	SortedCommands.reserve(Commands.size());
	for (size_t CommandIndex : SortedIndices)
	{
		SortedCommands.push_back(Commands[CommandIndex]);
	}
	return SortedCommands;
}

bool FTranslucentRenderPass::DrawEachCommand(const FRenderPassContext* Context, const FRenderCommand& Cmd)
{
	ID3D11DeviceContext* DeviceContext = Context->DeviceContext;
	
	Context->RenderResources->PerObjectConstantBuffer.Update(
		DeviceContext,
		&Cmd.PerObjectConstants,
		sizeof(FPerObjectConstants));
	ID3D11Buffer* cb1 = Context->RenderResources->PerObjectConstantBuffer.GetBuffer();
	DeviceContext->VSSetConstantBuffers(1, 1, &cb1);
	DeviceContext->PSSetConstantBuffers(1, 1, &cb1);

	// TODO: Particle 렌더링도 기존 구조에 좀 더 융화시킬 수 없는지 고민해보기
	if (IsParticleSpriteCommand(Cmd))
	{
		FShaderProgram* Program = GetParticleSpriteShaderProgram();
		if (Program == nullptr)
		{
			return false;
		}

		Program->Bind(DeviceContext);

		if (Cmd.Material != nullptr)
		{
			Cmd.Material->BindRenderStates(DeviceContext);
		}
		else
		{
			BindDefaultTranslucentStates(Context);
		}

		ID3D11ShaderResourceView* DefaultDiffuseSRV = FResourceManager::Get().GetDefaultWhiteSRV();
		DeviceContext->PSSetShaderResources(0, 1, &DefaultDiffuseSRV);
		if (Cmd.Material != nullptr)
		{
			Cmd.Material->BindParameters(DeviceContext, Program->PS);
		}
		if (Cmd.Constants.Particle.Texture != nullptr && Cmd.Constants.Particle.Texture->GetSRV() != nullptr)
		{
			ID3D11ShaderResourceView* ParticleTextureSRV = Cmd.Constants.Particle.Texture->GetSRV();
			DeviceContext->PSSetShaderResources(0, 1, &ParticleTextureSRV);
		}

		FGeometryDrawPacket DrawResources;
		return BuildParticleSpriteDrawResources(Context, Cmd, DrawResources)
			&& ExecuteGeometryDrawPacket(DeviceContext, DrawResources);
	}

	if (IsParticleRibbonCommand(Cmd))
	{
		FShaderProgram* Program = GetParticleRibbonShaderProgram();
		if (Program == nullptr)
		{
			UE_LOG_WARNING("[Particle] Ribbon draw skipped because ParticleRibbon shader failed to compile.");
			return false;
		}

		Program->Bind(DeviceContext);

		if (Cmd.Material != nullptr)
		{
			Cmd.Material->BindRenderStates(DeviceContext);
		}
		else
		{
			BindDefaultTranslucentStates(Context);
		}
		BindParticleBeamRasterizerState(Context);

		ID3D11ShaderResourceView* DefaultDiffuseSRV = FResourceManager::Get().GetDefaultWhiteSRV();
		DeviceContext->PSSetShaderResources(0, 1, &DefaultDiffuseSRV);
		if (Cmd.Material != nullptr)
		{
			Cmd.Material->BindParameters(DeviceContext, Program->PS);
		}

		FGeometryDrawPacket DrawResources;
		return BuildParticleRibbonDrawResources(Context, Cmd, DrawResources)
			&& ExecuteGeometryDrawPacket(DeviceContext, DrawResources);
	}

	if (IsParticleBeamCommand(Cmd))
	{
		FShaderProgram* Program = GetParticleBeamShaderProgram();
		if (Program == nullptr)
		{
			UE_LOG_WARNING("[Particle] Beam draw skipped because ParticleBeam shader failed to compile.");
			return false;
		}

		Program->Bind(DeviceContext);

		if (Cmd.Material != nullptr)
		{
			Cmd.Material->BindRenderStates(DeviceContext);
		}
		else
		{
			BindDefaultTranslucentStates(Context);
		}
		BindParticleBeamRasterizerState(Context);

		ID3D11ShaderResourceView* DefaultDiffuseSRV = FResourceManager::Get().GetDefaultWhiteSRV();
		DeviceContext->PSSetShaderResources(0, 1, &DefaultDiffuseSRV);
		if (Cmd.Material != nullptr)
		{
			Cmd.Material->BindParameters(DeviceContext, Program->PS);
		}

		FGeometryDrawPacket DrawResources;
		return BuildParticleBeamDrawResources(Context, Cmd, DrawResources)
			&& ExecuteGeometryDrawPacket(DeviceContext, DrawResources);
	}

	const bool bInstancedSurfaceDraw = IsInstancedSurfaceCommand(Cmd);
	FGeometryDrawPacket DrawResources;
	if (!BuildMeshGeometryDrawPacket(Cmd, DrawResources, bInstancedSurfaceDraw))
	{
		return false;
	}

	if (Cmd.Material != nullptr)
	{
		uint32 PermutationKey = 0;
		PermutationKey |= GetLightingModelPermutationKey(Context->RenderBus->GetViewMode());
		PermutationKey |= GetTexturePermutationKey(Cmd.Material);
		
		FShaderProgram* Program = GetShaderProgram(Cmd, PermutationKey);
		if (!Program)
		{
			return false;
		}

		Program->Bind(DeviceContext);
		Cmd.Material->BindRenderStates(DeviceContext);
		Cmd.Material->BindParameters(DeviceContext, Program->PS);
		BindVertexFactoryResources(
			DeviceContext,
			Cmd.VertexFactoryType,
			Context->RenderBus->GetBoneMatrixConstants(Cmd),
			Context->RenderResources,
			Cmd.BoneMatrixConstantBuffer);
	}

	return ExecuteGeometryDrawPacket(DeviceContext, DrawResources);
}
