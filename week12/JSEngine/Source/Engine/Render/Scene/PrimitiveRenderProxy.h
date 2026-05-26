#pragma once

class FRenderBus;
class FMeshBufferManager;
struct ID3D11Device;
struct ID3D11DeviceContext;

struct FPrimitiveRenderProxyCollectionContext
{
	FRenderBus& RenderBus;
	// TODO: replace this concrete dependency with a render resource provider once a scene resource layer exists.
	FMeshBufferManager& MeshBufferManager;
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
};

class FPrimitiveRenderProxy
{
public:
	virtual ~FPrimitiveRenderProxy() = default;
	virtual void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) = 0;
	virtual void ReleaseResources() {}
};
