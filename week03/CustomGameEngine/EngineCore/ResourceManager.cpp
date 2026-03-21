#include "ResourceManager.h"
#include "VertexSimple.h"
#include "Geometry/Sphere.h"
#include "Geometry/Triangle.h"
#include "Renderer/Renderer.h"

void AddLine(
	std::vector<FVertexSimple>& outVertices,
	float x0, float y0, float z0,
	float x1, float y1, float z1,
	float r, float g, float b, float a)
{
	outVertices.push_back({ x0, y0, z0,r,g,b,a });
	outVertices.push_back({ x1, y1, z1,r,g,b,a });
}
//------------------------------Helper Functions------------------------------


void ResourceManager::Initialize(URenderer* InRenderer)
{
	Renderer = InRenderer;
	D3D11_INPUT_ELEMENT_DESC layout[] = {
{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
 D3D11_INPUT_PER_VERTEX_DATA, 0},
{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
 D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	FMaterial& MainShader = CreateMaterial(L"Shader/ShaderW0.hlsl", L"Shader/ShaderW0.hlsl", Renderer->Device.Get(), layout, 2);
	MainShader.ConstantBuffer.Create(Renderer->Device.Get(), sizeof(FObjectConstant), 1);

	FMaterial& LineShader = CreateMaterial(L"Shader/ShaderLine.hlsl", L"Shader/ShaderLine.hlsl", Renderer->Device.Get(), layout, 2);
	//LineShader.ConstantBuffer.Create(Renderer->Device.Get(), sizeof(FObjectConstant), 1);


	AddCubeMesh(Renderer->Device.Get());
	AddSphereMesh(Renderer->Device.Get());
	AddTriangleMesh(Renderer->Device.Get());
	AddPlaneMesh(Renderer->Device.Get());
	AddGizmoTranslationMesh(Renderer->Device.Get());
	AddGizmoRotationMesh(Renderer->Device.Get());
	AddGizmoScaleMesh(Renderer->Device.Get());
	AddAxisMesh(Renderer->Device.Get());
	AddGridMesh(Renderer->Device.Get(), 1000.f, 100.f);

}

void ResourceManager::CreateNoBufferGeometry(FString ResourceName, const TArray<FVector>& Verticies)
{
	auto iter = GeometryMap.find(ResourceName);
	if (iter != GeometryMap.end())
	{
		return;
	}
	FGeometry& Geometry = GeometryMap[ResourceName];
	Geometry.VertexBuffer.Reset();
	Geometry.VertexStride = sizeof(FVector);
	Geometry.VertexCount = (UINT)Verticies.size();
	Geometry.Offset = 0;
	Geometry.Vertices = Verticies;
}

FMaterial& ResourceManager::CreateMaterial(WFString VertexShaderName, WFString PixelShaderName, ID3D11Device* Device, D3D11_INPUT_ELEMENT_DESC* layout, UINT layoutCount)
{
	FMaterial& Mat = MaterialMap[VertexShaderName];

	ID3DBlob* vertexshaderCSO = nullptr;
	D3DCompileFromFile(VertexShaderName.c_str(), nullptr, nullptr, "mainVS", "vs_5_0", 0,
		0, &vertexshaderCSO, nullptr);
	Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(),
		vertexshaderCSO->GetBufferSize(), nullptr,
		Mat.VertexShader.GetAddressOf());
	if (layout != nullptr)
		Device->CreateInputLayout(
			layout, layoutCount, vertexshaderCSO->GetBufferPointer(),
			vertexshaderCSO->GetBufferSize(), Mat.InputLayout.GetAddressOf());

	ID3DBlob* pixelshaderCSO = nullptr;
	D3DCompileFromFile(PixelShaderName.c_str(), nullptr, nullptr, "mainPS", "ps_5_0", 0,
		0, &pixelshaderCSO, nullptr);
	Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(),
		pixelshaderCSO->GetBufferSize(), nullptr,
		Mat.PixelShader.GetAddressOf());


	pixelshaderCSO->Release();
	vertexshaderCSO->Release();
	return Mat;
}

void ResourceManager::AddCubeMesh(ID3D11Device* Device)
{
	MeshData data = CreateCubeMeshData();
	TArray<FVector> Vertices(data.Vertices.size());
	for (size_t i = 0; i < data.Vertices.size(); ++i)
	{
		Vertices[i] = FVector(data.Vertices[i].x, data.Vertices[i].y, data.Vertices[i].z);
	}
	CreateStaticGeometry("Cube", data.Vertices.data(), (UINT)(sizeof(FVertexSimple) * data.Vertices.size()), Device, 0, Vertices, data.Indices.data(), (UINT)(sizeof(uint32_t) * data.Indices.size()));
}

void ResourceManager::AddSphereMesh(ID3D11Device* Device)
{
	TArray<FVector> Vertices(sphere_vertex_count);
	for (int i = 0; i < sphere_vertex_count; ++i)
	{
		Vertices[i] = FVector(sphere_vertices[i].x, sphere_vertices[i].y, sphere_vertices[i].z);
	}
	CreateStaticGeometry("Sphere", sphere_vertices, sizeof(sphere_vertices), Device, 0, Vertices, (void*)sphere_indices, sizeof(sphere_indices));
}

void ResourceManager::AddTriangleMesh(ID3D11Device* Device)
{
	uint32_t triangleIndices[] = { 0, 1, 2 };
	int vCnt = sizeof(triangle_vertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(vCnt);
	for (int i = 0; i < vCnt; ++i)
	{
		Vertices[i] = FVector(triangle_vertices[i].x, triangle_vertices[i].y, triangle_vertices[i].z);
	}
	CreateStaticGeometry("Triangle", triangle_vertices, sizeof(triangle_vertices), Device, 0, Vertices, triangleIndices, sizeof(triangleIndices));
}

void ResourceManager::AddPlaneMesh(ID3D11Device* Device)
{
	FVertexSimple planeVertices[] =
	{
		{ -5.0f,-5.0f, 0.0f, 81 / 255.0f, 91 / 255.0f, 212 / 255.0f,1.f },
		{  5.0f,-5.0f, 0.0f, 129 / 255.0f, 52 / 255.0f, 175 / 255.0f,1.f },
		{  5.0f, 5.0f, 0.0f, 221 / 255.0f, 42 / 255.0f, 123 / 255.0f,1.f },
		{ -5.0f, 5.0f, 0.0f, 254 / 255.0f, 218 / 255.0f, 119 / 255.0f,1.f }
	};
	uint32_t planeIndices[] = { 0, 1, 2, 0, 2, 3 };

	int vCnt = sizeof(planeVertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(vCnt);
	for (int i = 0; i < vCnt; ++i)
	{
		Vertices[i] = FVector(planeVertices[i].x, planeVertices[i].y, planeVertices[i].z);
	}
	CreateStaticGeometry("Plane", planeVertices, sizeof(planeVertices), Device, 0, Vertices, planeIndices, sizeof(planeIndices));
}

void ResourceManager::AddAxisMesh(ID3D11Device* Device)
{
	FVertexSimple AxisVertices[] =
	{
		//X축 (Red): (0,0,0) -> (Inf,0,0)
	   { 0.0f, 0.0f, 0.0f, 1,0,0,1 },
	   { 10000000.0f, 0.0f, 0.0f, 1,0,0,1 },

	   // Y축 (Green): (0,0,0) -> (0,Inf,0)
	   { 0.0f, 0.0f, 0.0f, 0,1,0,1 },
	   { 0.0f, 10000000.0f, 0.0f, 0,1,0,1 },

	   // Z축 (Blue): (0,0,0) -> (0,0,Inf)
	   { 0.0f, 0.0f, 0.0f, 0,0,1,1 },
	   { 0.0f, 0.0f, 10000000.0f, 0,0,1,1 }
	};

	int Cnt = sizeof(AxisVertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(Cnt);
	TArray<FColor> Colors(Cnt);
	for (int i = 0; i < Cnt; ++i)
	{
		Vertices[i] = FVector(AxisVertices[i].x, AxisVertices[i].y, AxisVertices[i].z);
		Colors[i] = FColor(AxisVertices[i].r, AxisVertices[i].g, AxisVertices[i].b, AxisVertices[i].a);
	}
	FGeometry* Geo = CreateStaticGeometry("Axis", AxisVertices, sizeof(AxisVertices), Device, 0, Vertices);
	Geo->Colors = Colors;
}

MeshData ResourceManager::CreateCubeMeshData()
{
	MeshData data;
	data.Vertices =
	{
		// Front (+X)
		{  0.5f, -0.5f, -0.5f, 1,1,1,1 }, {  0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{  0.5f,  0.5f,  0.5f, 1,1,1,1 }, {  0.5f, -0.5f,  0.5f, 1,1,1,1 },
		// Back (-X)
		{ -0.5f, -0.5f, -0.5f, 1,1,1,1 }, { -0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{ -0.5f,  0.5f,  0.5f, 1,1,1,1 }, { -0.5f, -0.5f,  0.5f, 1,1,1,1 },
		// Left (-Y)
		{ -0.5f, -0.5f, -0.5f, 1,1,1,1 }, {  0.5f, -0.5f, -0.5f, 1,1,1,1 },
		{  0.5f, -0.5f,  0.5f, 1,1,1,1 }, { -0.5f, -0.5f,  0.5f, 1,1,1,1 },
		// Right (+Y)
		{ -0.5f,  0.5f, -0.5f, 1,1,1,1 }, {  0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{  0.5f,  0.5f,  0.5f, 1,1,1,1 }, { -0.5f,  0.5f,  0.5f, 1,1,1,1 },
		// Top (+Z)
		{ -0.5f, -0.5f,  0.5f, 1,1,1,1 }, {  0.5f, -0.5f,  0.5f, 1,1,1,1 },
		{  0.5f,  0.5f,  0.5f, 1,1,1,1 }, { -0.5f,  0.5f,  0.5f, 1,1,1,1 },
		// Bottom (-Z)
		{ -0.5f, -0.5f, -0.5f, 1,1,1,1 }, {  0.5f, -0.5f, -0.5f, 1,1,1,1 },
		{  0.5f,  0.5f, -0.5f, 1,1,1,1 }, { -0.5f,  0.5f, -0.5f, 1,1,1,1 },
	};

	for (uint32_t i = 0; i < 6; ++i)
	{
		uint32_t base = i * 4;
		data.Indices.push_back(base); data.Indices.push_back(base + 1); data.Indices.push_back(base + 2);
		data.Indices.push_back(base); data.Indices.push_back(base + 2); data.Indices.push_back(base + 3);
	}
	return data;
}

MeshData ResourceManager::CreateCircleMeshData(int sliceCount, float radius, float height)
{
	MeshData data;
	data.Vertices.push_back({ 0.0f, 0.0f, height, 1, 1, 1, 1 }); // Center

	float dTheta = 2.0f * MathHelper::PI / sliceCount;
	for (int i = 0; i <= sliceCount; ++i)
	{
		float theta = i * dTheta;
		float y = radius * cosf(theta);
		float x = radius * sinf(theta);
		data.Vertices.push_back({ x, y, height, 1, 1, 1, 1 });
	}

	for (int i = 1; i <= sliceCount; ++i)
	{
		data.Indices.push_back(0);
		data.Indices.push_back(i);
		data.Indices.push_back(i + 1);
	}
	return data;
}

MeshData ResourceManager::CreateTorusMeshData(float majorRadius, float minorRadius, int radialSegments, int tubularSegments)
{
	MeshData data;
	float dU = 0.5f * MathHelper::PI / radialSegments;
	float dV = 2.0f * MathHelper::PI / tubularSegments;
	float flatScaleZ = 0.2f;

	for (int i = 0; i <= radialSegments; ++i)
	{
		float u = i * dU;
		for (int j = 0; j <= tubularSegments; ++j)
		{
			float v = j * dV + MathHelper::PI / 4.0f;
			float sectionX = minorRadius * cosf(v);
			float sectionY = minorRadius * sinf(v) * flatScaleZ;
			float r = majorRadius + sectionX;
			float y = r * cosf(u);
			float x = r * sinf(u);
			float z = sectionY;
			data.Vertices.push_back({ x, y, z, 1, 1, 1, 1 });
		}
	}

	for (int i = 0; i < radialSegments; ++i)
	{
		for (int j = 0; j < tubularSegments; ++j)
		{
			uint32_t first = i * (tubularSegments + 1) + j;
			uint32_t second = first + tubularSegments + 1;

			data.Indices.push_back(first);
			data.Indices.push_back(second);
			data.Indices.push_back(first + 1);

			data.Indices.push_back(second);
			data.Indices.push_back(second + 1);
			data.Indices.push_back(first + 1);
		}
	}
	return data;
}

MeshData ResourceManager::CreateCylinderMeshData(int sliceCount, float radius, float height)
{
	MeshData data;
	float halfHeight = height * 0.5f;
	float dTheta = 2.0f * MathHelper::PI / sliceCount;

	// Side vertices
	for (int i = 0; i <= sliceCount; ++i)
	{
		float theta = i * dTheta;
		float x = radius * sinf(theta);
		float y = radius * cosf(theta);

		data.Vertices.push_back({ x, y, -halfHeight, 1, 1, 1, 1 }); // Bottom
		data.Vertices.push_back({ x, y, halfHeight, 1, 1, 1, 1 });  // Top
	}

	for (int i = 0; i < sliceCount; ++i)
	{
		uint32_t base = i * 2;
		data.Indices.push_back(base);
		data.Indices.push_back(base + 1);
		data.Indices.push_back(base + 3);

		data.Indices.push_back(base);
		data.Indices.push_back(base + 3);
		data.Indices.push_back(base + 2);
	}

	// Caps (Reuse or create new for flat shading if needed, but here we share)
	uint32_t bottomCenterIndex = (uint32_t)data.Vertices.size();
	data.Vertices.push_back({ 0, 0, -halfHeight, 1, 1, 1, 1 });
	uint32_t topCenterIndex = (uint32_t)data.Vertices.size();
	data.Vertices.push_back({ 0, 0, halfHeight, 1, 1, 1, 1 });

	for (int i = 0; i < sliceCount; ++i)
	{
		data.Indices.push_back(bottomCenterIndex);
		data.Indices.push_back((i + 1) * 2);
		data.Indices.push_back(i * 2);

		data.Indices.push_back(topCenterIndex);
		data.Indices.push_back(i * 2 + 1);
		data.Indices.push_back((i + 1) * 2 + 1);
	}

	return data;
}

MeshData ResourceManager::CreateConeMeshData(int sliceCount, float radius, float height)
{
	MeshData data;
	float halfHeight = height * 0.5f;
	float dTheta = 2.0f * MathHelper::PI / sliceCount;

	uint32_t tipIndex = 0;
	data.Vertices.push_back({ 0, 0, halfHeight, 1, 1, 1, 1 });

	for (int i = 0; i <= sliceCount; ++i)
	{
		float theta = i * dTheta;
		float x = radius * sinf(theta);
		float y = radius * cosf(theta);
		data.Vertices.push_back({ x, y, -halfHeight, 1, 1, 1, 1 });
	}

	for (int i = 1; i <= sliceCount; ++i)
	{
		data.Indices.push_back(tipIndex);
		data.Indices.push_back(i);
		data.Indices.push_back(i + 1);
	}

	uint32_t bottomCenterIndex = (uint32_t)data.Vertices.size();
	data.Vertices.push_back({ 0, 0, -halfHeight, 1, 1, 1, 1 });

	for (int i = 1; i <= sliceCount; ++i)
	{
		data.Indices.push_back(bottomCenterIndex);
		data.Indices.push_back(i + 1);
		data.Indices.push_back(i);
	}

	return data;
}

void ResourceManager::AddGizmoTranslationMesh(ID3D11Device* Device)
{
	MeshData cylinder = CreateCylinderMeshData();
	FMatrix cylinderScale = FMatrix::ScaleMatrix(0.05f, 0.05f, 1.0f);
	for (auto& v : cylinder.Vertices)
	{
		FVector p = cylinderScale.TransformPoint(FVector(v.x, v.y, v.z));
		v.x = p.x; v.y = p.y; v.z = p.z;
	}

	MeshData cone = CreateConeMeshData();
	FMatrix coneTransform = FMatrix::ScaleMatrix(0.2f, 0.2f, 0.2f) * FMatrix::TranslationMatrix(0.0f, 0.0f, 0.5f);
	uint32_t offset = (uint32_t)cylinder.Vertices.size();
	for (auto& v : cone.Vertices)
	{
		FVector p = coneTransform.TransformPoint(FVector(v.x, v.y, v.z));
		v.x = p.x; v.y = p.y; v.z = p.z;
		cylinder.Vertices.push_back(v);
	}
	for (auto idx : cone.Indices)
	{
		cylinder.Indices.push_back(idx + offset);
	}

	TArray<FVector> Vertices(cylinder.Vertices.size());
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i)
	{
		Vertices[i] = FVector(cylinder.Vertices[i].x, cylinder.Vertices[i].y, cylinder.Vertices[i].z);
	}
	CreateStaticGeometry("GizmoTranslation", cylinder.Vertices.data(), (UINT)(sizeof(FVertexSimple) * cylinder.Vertices.size()), Device, 0, Vertices, cylinder.Indices.data(), (UINT)(sizeof(uint32_t) * cylinder.Indices.size()));
}

void ResourceManager::AddGizmoRotationMesh(ID3D11Device* Device)
{
	MeshData torus = CreateTorusMeshData();
	FMatrix torusScale = FMatrix::ScaleMatrix(1.5f, 1.5f, 1.5f);
	TArray<FVector> Vertices(torus.Vertices.size());
	for (size_t i = 0; i < torus.Vertices.size(); ++i)
	{
		FVector p = torusScale.TransformPoint(FVector(torus.Vertices[i].x, torus.Vertices[i].y, torus.Vertices[i].z));
		torus.Vertices[i].x = p.x; torus.Vertices[i].y = p.y; torus.Vertices[i].z = p.z;
		Vertices[i] = p;
	}
	CreateStaticGeometry("GizmoRotation", torus.Vertices.data(), (UINT)(sizeof(FVertexSimple) * torus.Vertices.size()), Device, 0, Vertices, torus.Indices.data(), (UINT)(sizeof(uint32_t) * torus.Indices.size()));
}

void ResourceManager::AddGizmoScaleMesh(ID3D11Device* Device)
{
	MeshData cylinder = CreateCylinderMeshData();
	FMatrix cylinderScale = FMatrix::ScaleMatrix(0.05f, 0.05f, 1.0f);
	for (auto& v : cylinder.Vertices)
	{
		FVector p = cylinderScale.TransformPoint(FVector(v.x, v.y, v.z));
		v.x = p.x; v.y = p.y; v.z = p.z;
	}

	MeshData cube = CreateCubeMeshData();
	FMatrix cubeTransform = FMatrix::ScaleMatrix(0.2f, 0.2f, 0.2f) * FMatrix::TranslationMatrix(0.0f, 0.0f, 0.5f);
	uint32_t offset = (uint32_t)cylinder.Vertices.size();
	for (auto& v : cube.Vertices)
	{
		FVector p = cubeTransform.TransformPoint(FVector(v.x, v.y, v.z));
		v.x = p.x; v.y = p.y; v.z = p.z;
		cylinder.Vertices.push_back(v);
	}
	for (auto idx : cube.Indices)
	{
		cylinder.Indices.push_back(idx + offset);
	}

	TArray<FVector> Vertices(cylinder.Vertices.size());
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i)
	{
		Vertices[i] = FVector(cylinder.Vertices[i].x, cylinder.Vertices[i].y, cylinder.Vertices[i].z);
	}
	CreateStaticGeometry("GizmoScale", cylinder.Vertices.data(), (UINT)(sizeof(FVertexSimple) * cylinder.Vertices.size()), Device, 0, Vertices, cylinder.Indices.data(), (UINT)(sizeof(uint32_t) * cylinder.Indices.size()));
}

TArray<FVertexSimple> ResourceManager::MakeGridMesh(ID3D11Device* Device, float halfExtent, float cellSize, float z)
{
	TArray<FVertexSimple> vertices;

	if (halfExtent <= 0.0f || cellSize <= 0.0f)
		return vertices;

	const int lineCountPerSide = static_cast<int>(std::floor(halfExtent / cellSize));
	vertices.reserve((lineCountPerSide * 2 + 1) * 4);

	for (int i = -lineCountPerSide; i <= lineCountPerSide; ++i)
	{
		const float p = i * cellSize;

		float r = 0.35f;
		float g = 0.35f;
		float b = 0.35f;
		float a = 1.0f;

		if (i == 0)
		{
			r = 0.8f;
			g = 0.8f;
			b = 0.8f;
		}
		else if ((std::abs(i) % 10) == 0)
		{
			r = 0.55f;
			g = 0.55f;
			b = 0.55f;
		}

		AddLine(vertices, p, -halfExtent, z, p, halfExtent, z, r, g, b, a);
		AddLine(vertices, -halfExtent, p, z, halfExtent, p, z, r, g, b, a);
	}

	return vertices;
}

void ResourceManager::AddGridMesh(ID3D11Device* Device, float halfExtent, float cellSize, float z)
{
	TArray<FVertexSimple> vertices = MakeGridMesh(Device, halfExtent, cellSize, z);
	size_t cnt = vertices.size();
	TArray<FVector> Vertices(cnt);
	for (size_t i = 0; i < cnt; ++i)
	{
		Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
	}
	CreateNoBufferGeometry("Grid", Vertices);
}

void ResourceManager::UpdateGridMesh(float halfExtent, float cellSize, float z)
{
	TArray<FVertexSimple> vertices = MakeGridMesh(Renderer->Device.Get(), halfExtent, cellSize, z);
	size_t cnt = vertices.size();
	TArray<FVector> Vertices(cnt);
	for (size_t i = 0; i < cnt; ++i)
	{
		Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
	}
	UpdateStaticGeomtry("Grid", vertices.data(), (UINT)(sizeof(FVertexSimple) * vertices.size()), Renderer->Device.Get(), 0, Vertices);
}




FMaterial& ResourceManager::GetMaterial(WFString ResourceName)
{
	return MaterialMap[ResourceName];
}

FGeometry& ResourceManager::GetGeometry(FString ResourceName)
{
	return GeometryMap[ResourceName];
}

void ResourceManager::Release()
{
	GeometryMap.clear();
	MaterialMap.clear();
}
