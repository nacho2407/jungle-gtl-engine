#include "Render/Proxy/WheelMeshSceneProxy.h"

#include "Component/Primitive/WheelMeshComponent.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Types/RenderConstants.h"

FWheelMeshSceneProxy::FWheelMeshSceneProxy(UWheelMeshComponent* InComponent)
	: FStaticMeshSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::WheelMesh;
}

UWheelMeshComponent* FWheelMeshSceneProxy::GetWheelMeshComponent() const
{
	return Cast<UWheelMeshComponent>(GetOwner());
}

void FWheelMeshSceneProxy::ApplyDrawCommandOverrides(
	ID3D11Device* Device,
	ID3D11DeviceContext* Context,
	ERenderPass Pass,
	FDrawCommand& Command) const
{
	(void)Pass;

	const UWheelMeshComponent* WheelMesh = GetWheelMeshComponent();
	if (!WheelMesh)
	{
		return;
	}

	FWheelDeformationConstants Constants = {};
	Constants.ContactNormalAndDepth = FVector4(
		WheelMesh->GetContactNormal().X,
		WheelMesh->GetContactNormal().Y,
		WheelMesh->GetContactNormal().Z,
		WheelMesh->GetDeformationDepth());
	Constants.WheelRadius = WheelMesh->GetWheelRadius();

	if (!WheelDeformationCB.GetBuffer())
	{
		WheelDeformationCB.Create(Device, sizeof(FWheelDeformationConstants), "WheelDeformationCB");
	}
	WheelDeformationCB.Update(Context, &Constants, sizeof(FWheelDeformationConstants));
	Command.Bindings.WheelDeformationCB = &WheelDeformationCB;
}
