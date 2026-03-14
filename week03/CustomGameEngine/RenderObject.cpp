#include "RenderObject.h"
#include "FGeometry.h"
#include "FMaterial.h"
URenderObject::URenderObject()
{
}

URenderObject::~URenderObject()
{
}

void URenderObject::Render(ID3D11DeviceContext* Context)
{
	if (!Material || !Geometry) return;

	Material->BindShaders(Context);
	Geometry->BindBuffers(Context);
	
	Context->Draw(Geometry->VertexCount, 0);
}


REGISTER_CLASS(URenderObject);
