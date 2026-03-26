#include "RenderObject.h"
#include "Geometry.h"
#include "Material.h"

void RenderObject::Render(ID3D11DeviceContext* Context)
{
	if (!Material || !Geometry) return;

	Material->Bind(Context);
	Geometry->Bind(Context);

	Context->IASetPrimitiveTopology(PrimitiveTopology);

	if (Geometry->IndexBuffer != nullptr)
	{
		Context->DrawIndexed(Geometry->IndexCount, 0, 0);
	}
	else
	{
		Context->Draw(Geometry->VertexCount, 0);
	}
}