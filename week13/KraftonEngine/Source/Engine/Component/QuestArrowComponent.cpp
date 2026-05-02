#include "Component/QuestArrowComponent.h"

#include "Materials/Material.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/RenderConstants.h"

#include <cmath>

IMPLEMENT_CLASS(UQuestArrowComponent, UPrimitiveComponent)

namespace
{
	struct FQuestArrowColorConstants
	{
		FVector4 DiffuseColor;
	};

	class FQuestArrowSceneProxy : public FPrimitiveSceneProxy
	{
	public:
		explicit FQuestArrowSceneProxy(UQuestArrowComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent)
		{
			ProxyFlags &= ~(EPrimitiveProxyFlags::SupportsOutline | EPrimitiveProxyFlags::ShowAABB);
		}

		~FQuestArrowSceneProxy() override
		{
			DiffuseColorCB.Release();
		}

		void UpdateMesh() override
		{
			UQuestArrowComponent* Arrow = static_cast<UQuestArrowComponent*>(GetOwner());
			MeshBuffer = Arrow->GetMeshBuffer();

			if (!DefaultMaterial)
			{
				DefaultMaterial = UMaterial::CreateTransient(
					ERenderPass::Opaque,
					EBlendState::Opaque,
					EDepthStencilState::DepthReadOnly,
					ERasterizerState::SolidBackCull,
					FShaderManager::Get().GetOrCreate(EShaderPath::Primitive));
			}

			auto& ColorCB = DefaultMaterial->BindPerShaderCB<FQuestArrowColorConstants>(
				&DiffuseColorCB,
				ECBSlot::PerShader0);
			ColorCB.DiffuseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

			SectionDraws.clear();
			if (MeshBuffer && DefaultMaterial)
			{
				const uint32 IdxCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
				SectionDraws.push_back({ DefaultMaterial, 0, IdxCount });
			}
		}

	private:
		FConstantBuffer DiffuseColorCB;
	};
}

UQuestArrowComponent::UQuestArrowComponent()
{
	LocalExtents = FVector(0.17f, 0.055f, 0.055f);
	SetCastShadow(false);
}

FMeshBuffer* UQuestArrowComponent::GetMeshBuffer() const
{
	return &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::QuestArrow);
}

FMeshDataView UQuestArrowComponent::GetMeshDataView() const
{
	return FMeshDataView::FromMeshData(FMeshBufferManager::Get().GetMeshData(EMeshShape::QuestArrow));
}

FPrimitiveSceneProxy* UQuestArrowComponent::CreateSceneProxy()
{
	return new FQuestArrowSceneProxy(this);
}

void UQuestArrowComponent::SetWorldDirection(const FVector& Direction)
{
	FVector SafeDirection = Direction;
	if (SafeDirection.Length() <= 1.0e-4f)
	{
		return;
	}

	SafeDirection.Normalize();

	const float HorizontalLength = std::sqrt(SafeDirection.X * SafeDirection.X + SafeDirection.Y * SafeDirection.Y);
	const float Yaw = std::atan2(SafeDirection.Y, SafeDirection.X) * RAD_TO_DEG;
	const float Pitch = std::atan2(SafeDirection.Z, HorizontalLength) * RAD_TO_DEG;

	SetRelativeRotation(FRotator(Pitch, Yaw, 0.0f));
}
