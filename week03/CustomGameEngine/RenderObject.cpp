#include "RenderObject.h"
#include "Geometry.h"
#include "Material.h"

void RenderObject::Render(ID3D11DeviceContext* Context)
{
	if (!Material || !Geometry) return;

	Material->BindShaders(Context);
	Geometry->BindBuffers(Context);

	Context->IASetPrimitiveTopology(PrimitiveTopology);
	
	Context->Draw(Geometry->VertexCount, 0);
}
