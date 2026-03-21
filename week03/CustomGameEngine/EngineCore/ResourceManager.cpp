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
	FMaterial& Mat = CreateMaterial(L"Shader/ShaderW0.hlsl", L"Shader/ShaderW0.hlsl", Renderer->Device.Get(), layout, 2);
	Mat.ConstantBuffer.Create(Renderer->Device.Get(), sizeof(FObjectConstant), 1);

	//CreateMaterial(L"Shader/ShaderInfiniteGrid.hlsl", L"Shader/ShaderInfiniteGrid.hlsl", Renderer->Device.Get(), nullptr, 0);
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
	Geometry.VertexCount = Verticies.size();
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
	FVertexSimple cubeVertices[] =
	{
		// Front (+Z) - Red
		{ -0.5f,-0.5f, 0.5f, 1,0,0,1 },
		{  0.5f,-0.5f, 0.5f, 1,0,0,1 },
		{  0.5f, 0.5f, 0.5f, 1,0,0,1 },

		{ -0.5f,-0.5f, 0.5f, 1,0,0,1 },
		{  0.5f, 0.5f, 0.5f, 1,0,0,1 },
		{ -0.5f, 0.5f, 0.5f, 1,0,0,1 },


		// Back (-Z) - Green
		{  0.5f,-0.5f,-0.5f, 0,1,0,1 },
		{ -0.5f,-0.5f,-0.5f, 0,1,0,1 },
		{ -0.5f, 0.5f,-0.5f, 0,1,0,1 },

		{  0.5f,-0.5f,-0.5f, 0,1,0,1 },
		{ -0.5f, 0.5f,-0.5f, 0,1,0,1 },
		{  0.5f, 0.5f,-0.5f, 0,1,0,1 },


		// Left (-X) - Blue
		{ -0.5f,-0.5f,-0.5f, 0,0,1,1 },
		{ -0.5f,-0.5f, 0.5f, 0,0,1,1 },
		{ -0.5f, 0.5f, 0.5f, 0,0,1,1 },

		{ -0.5f,-0.5f,-0.5f, 0,0,1,1 },
		{ -0.5f, 0.5f, 0.5f, 0,0,1,1 },
		{ -0.5f, 0.5f,-0.5f, 0,0,1,1 },


		// Right (+X) - Yellow
		{ 0.5f,-0.5f, 0.5f, 1,1,0,1 },
		{ 0.5f,-0.5f,-0.5f, 1,1,0,1 },
		{ 0.5f, 0.5f,-0.5f, 1,1,0,1 },

		{ 0.5f,-0.5f, 0.5f, 1,1,0,1 },
		{ 0.5f, 0.5f,-0.5f, 1,1,0,1 },
		{ 0.5f, 0.5f, 0.5f, 1,1,0,1 },


		// Top (+Y) - Cyan
		{ -0.5f,0.5f, 0.5f, 0,1,1,1 },
		{  0.5f,0.5f, 0.5f, 0,1,1,1 },
		{  0.5f,0.5f,-0.5f, 0,1,1,1 },

		{ -0.5f,0.5f, 0.5f, 0,1,1,1 },
		{  0.5f,0.5f,-0.5f, 0,1,1,1 },
		{ -0.5f,0.5f,-0.5f, 0,1,1,1 },


		// Bottom (-Y) - Magenta
		{ -0.5f,-0.5f,-0.5f, 1,0,1,1 },
		{  0.5f,-0.5f,-0.5f, 1,0,1,1 },
		{  0.5f,-0.5f, 0.5f, 1,0,1,1 },

		{ -0.5f,-0.5f,-0.5f, 1,0,1,1 },
		{  0.5f,-0.5f, 0.5f, 1,0,1,1 },
		{ -0.5f,-0.5f, 0.5f, 1,0,1,1 },
	};

	int Cnt = sizeof(cubeVertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(Cnt);
	for (int i = 0; i < Cnt; ++i)
	{
		Vertices[i] = FVector(cubeVertices[i].x, cubeVertices[i].y, cubeVertices[i].z);
	}
	CreateStaticGeometry("Cube", cubeVertices, sizeof(cubeVertices), Device, 0, Vertices);
}

void ResourceManager::AddSphereMesh(ID3D11Device* Device)
{
	int Cnt = sizeof(sphere_vertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(Cnt);
	for (int i = 0; i < Cnt; ++i)
	{
		Vertices[i] = FVector(sphere_vertices[i].x, sphere_vertices[i].y, sphere_vertices[i].z);
	}
	CreateStaticGeometry("Sphere", sphere_vertices, sizeof(sphere_vertices), Device, 0, Vertices);
}

void ResourceManager::AddTriangleMesh(ID3D11Device* Device)
{
	int Cnt = sizeof(triangle_vertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(Cnt);
	for (int i = 0; i < Cnt; ++i)
	{
		Vertices[i] = FVector(triangle_vertices[i].x, triangle_vertices[i].y, triangle_vertices[i].z);
	}
	CreateStaticGeometry("Triangle", triangle_vertices, sizeof(triangle_vertices), Device, 0, Vertices);
}

void ResourceManager::AddPlaneMesh(ID3D11Device* Device)
{
	FVertexSimple planeVertices[] =
	{
		{ -5.0f,-5.0f, 0.0f, 81 / 255.0f, 91 / 255.0f, 212 / 255.0f,1.f },
		{  5.0f,-5.0f, 0.0f, 129 / 255.0f, 52 / 255.0f, 175 / 255.0f,1.f },
		{  5.0f, 5.0f, 0.0f, 221 / 255.0f, 42 / 255.0f, 123 / 255.0f,1.f },

		{ -5.0f,-5.0f, 0.0f, 81 / 255.0,91 / 255.0f, 212 / 255.0f,1.f },
		{  5.0f, 5.0f, 0.0f, 221 / 255.0, 42 / 255.0f, 123 / 255.0f,1.f },
		{ -5.0f, 5.0f, 0.0f, 254 / 255.0, 218 / 255.0f, 119 / 255.0f,1.f }
	};
	int Cnt = sizeof(planeVertices) / sizeof(FVertexSimple);
	TArray<FVector> Vertices(Cnt);
	for (int i = 0; i < Cnt; ++i)
	{
		Vertices[i] = FVector(planeVertices[i].x, planeVertices[i].y, planeVertices[i].z);
	}
	CreateStaticGeometry("Plane", planeVertices, sizeof(planeVertices), Device, 0, Vertices);
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

TArray<FVertexSimple> ResourceManager::CreateCubeVertices()
{
	TArray<FVertexSimple> cubeVertices =
	{
		// Front (+X) - Red (과거 +Z)
		{  0.5f, -0.5f, -0.5f, 1,1,1,1 },
		{  0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{  0.5f,  0.5f,  0.5f, 1,1,1,1 },

		{  0.5f, -0.5f, -0.5f, 1,1,1,1 },
		{  0.5f,  0.5f,  0.5f, 1,1,1,1 },
		{  0.5f, -0.5f,  0.5f, 1,1,1,1 },


		// Back (-X) - Green (과거 -Z)
		{ -0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{ -0.5f, -0.5f, -0.5f, 1,1,1,1 },
		{ -0.5f, -0.5f,  0.5f, 1,1,1,1 },

		{ -0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{ -0.5f, -0.5f,  0.5f, 1,1,1,1 },
		{ -0.5f,  0.5f,  0.5f, 1,1,1,1 },


		// Left (-Y) - Blue (과거 -X)
		{ -0.5f, -0.5f, -0.5f, 1,1,1,1 },
		{  0.5f, -0.5f, -0.5f, 1,1,1,1 },
		{  0.5f, -0.5f,  0.5f, 1,1,1,1 },

		{ -0.5f, -0.5f, -0.5f, 1,1,1,1 },
		{  0.5f, -0.5f,  0.5f, 1,1,1,1 },
		{ -0.5f, -0.5f,  0.5f, 1,1,1,1 },


		// Right (+Y) - Yellow (과거 +X)
		{  0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{ -0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{ -0.5f,  0.5f,  0.5f, 1,1,1,1 },

		{  0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{ -0.5f,  0.5f,  0.5f, 1,1,1,1 },
		{  0.5f,  0.5f,  0.5f, 1,1,1,1 },


		// Top (+Z)
		{  0.5f, -0.5f,  0.5f, 1,1,1,1 },
		{  0.5f,  0.5f,  0.5f, 1,1,1,1 },
		{ -0.5f,  0.5f,  0.5f, 1,1,1,1 },

		{  0.5f, -0.5f,  0.5f, 1,1,1,1 },
		{ -0.5f,  0.5f,  0.5f, 1,1,1,1 },
		{ -0.5f, -0.5f,  0.5f, 1,1,1,1 },


		// Bottom (-Z)
		{ -0.5f, -0.5f, -0.5f, 1,1,1,1 },
		{ -0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{  0.5f,  0.5f, -0.5f, 1,1,1,1 },

		{ -0.5f, -0.5f, -0.5f, 1,1,1,1 },
		{  0.5f,  0.5f, -0.5f, 1,1,1,1 },
		{  0.5f, -0.5f, -0.5f, 1,1,1,1 },
	};
	return cubeVertices;
}

TArray<FVertexSimple> ResourceManager::CreateCircleVertices(int sliceCount, float radius, float height)
{
	TArray<FVertexSimple> circleVertices;
	float dTheta = 2.0f * MathHelper::PI / sliceCount;
	for (int i = 0; i < sliceCount; ++i)
	{
		float theta0 = i * dTheta;
		float theta1 = (i + 1) * dTheta;

		// 옛날: X=cos, Z=sin, Y=height
		// UE변환: oldX->Y, oldZ->X, oldY(height)->Z
		float y0 = radius * cosf(theta0);
		float x0 = radius * sinf(theta0);
		float y1 = radius * cosf(theta1);
		float x1 = radius * sinf(theta1);

		circleVertices.push_back({ x0, y0, height, 1, 1, 1, 1 });
		circleVertices.push_back({ x1, y1, height, 1, 1, 1, 1 });
	}
	return circleVertices;
}

TArray<FVertexSimple> ResourceManager::CreateTorusVertices(float majorRadius, float minorRadius, int radialSegments, int tubularSegments)
{
	TArray<FVertexSimple> torusVertices;

	float dU = 0.5f * MathHelper::PI / radialSegments;
	float dV = 2.0f * MathHelper::PI / tubularSegments;

	float flatScaleZ = 0.2f;

	for (int i = 0; i < radialSegments; ++i)
	{
		for (int j = 0; j < tubularSegments; ++j)
		{
			float u1 = i * dU;
			float v1 = j * dV + MathHelper::PI / 4.0f;
			float u2 = (i + 1) * dU;
			float v2 = (j + 1) * dV + (MathHelper::PI / 4.0f);

			auto GetTorusPoint = [=](float u, float v) -> FVector {
				float sectionX = minorRadius * cosf(v);
				float sectionY = minorRadius * sinf(v) * flatScaleZ;

				float r = majorRadius + sectionX;

				float y = r * cosf(u);
				float x = r * sinf(u);
				float z = sectionY;

				return FVector(x, y, z);
				};

			FVector p00 = GetTorusPoint(u1, v1);
			FVector p10 = GetTorusPoint(u2, v1);
			FVector p11 = GetTorusPoint(u2, v2);
			FVector p01 = GetTorusPoint(u1, v2);

			torusVertices.push_back({ p00.x, p00.y, p00.z, 1, 1, 1, 1 });
			torusVertices.push_back({ p10.x, p10.y, p10.z, 1, 1, 1, 1 });
			torusVertices.push_back({ p11.x, p11.y, p11.z, 1, 1, 1, 1 });

			torusVertices.push_back({ p00.x, p00.y, p00.z, 1, 1, 1, 1 });
			torusVertices.push_back({ p11.x, p11.y, p11.z, 1, 1, 1, 1 });
			torusVertices.push_back({ p01.x, p01.y, p01.z, 1, 1, 1, 1 });
		}
	}

	return torusVertices;
}

TArray<FVertexSimple> ResourceManager::CreateCylinderVertices(int sliceCount, float radius, float height)
{
	TArray<FVertexSimple> cylinderVertices;

	float halfHeight = height * 0.5f;
	float dTheta = 2.0f * MathHelper::PI / sliceCount;

	// UE변환 헬퍼 (oldX -> Y, oldZ -> X, oldY -> Z)
	auto pushCyl = [&](float oldX, float oldY, float oldZ) {
		cylinderVertices.push_back({ oldZ, oldX, oldY, 1,1,1,1 });
		};

	for (int i = 0; i < sliceCount; ++i)
	{
		float theta0 = i * dTheta;
		float theta1 = (i + 1) * dTheta;

		float oldx0 = radius * cosf(theta0);
		float oldz0 = radius * sinf(theta0);
		float oldx1 = radius * cosf(theta1);
		float oldz1 = radius * sinf(theta1);

		pushCyl(oldx0, -halfHeight, oldz0);
		pushCyl(oldx1, -halfHeight, oldz1);
		pushCyl(oldx1, halfHeight, oldz1);

		pushCyl(oldx0, -halfHeight, oldz0);
		pushCyl(oldx1, halfHeight, oldz1);
		pushCyl(oldx0, halfHeight, oldz0);
	}

	for (int i = 0; i < sliceCount; ++i)
	{
		float theta0 = i * dTheta;
		float theta1 = (i + 1) * dTheta;

		float oldx0 = radius * cosf(theta0);
		float oldz0 = radius * sinf(theta0);
		float oldx1 = radius * cosf(theta1);
		float oldz1 = radius * sinf(theta1);

		pushCyl(oldx0, halfHeight, oldz0);
		pushCyl(oldx1, halfHeight, oldz1);
		pushCyl(0.0f, halfHeight, 0.0f);
	}

	for (int i = 0; i < sliceCount; ++i)
	{
		float theta0 = i * dTheta;
		float theta1 = (i + 1) * dTheta;

		float oldx0 = radius * cosf(theta0);
		float oldz0 = radius * sinf(theta0);
		float oldx1 = radius * cosf(theta1);
		float oldz1 = radius * sinf(theta1);

		pushCyl(oldx0, -halfHeight, oldz0);
		pushCyl(0.0f, -halfHeight, 0.0f);
		pushCyl(oldx1, -halfHeight, oldz1);
	}

	return cylinderVertices;
}

TArray<FVertexSimple> ResourceManager::CreateConeVertices(int sliceCount, float radius, float height)
{
	TArray<FVertexSimple> coneVertices;

	float halfHeight = height * 0.5f;
	float dTheta = 2.0f * MathHelper::PI / sliceCount;

	// UE변환 헬퍼 (oldX -> Y, oldZ -> X, oldY -> Z)
	auto pushCone = [&](float oldX, float oldY, float oldZ) {
		coneVertices.push_back({ oldZ, oldX, oldY, 1,1,1,1 });
		};

	// 측면
	for (int i = 0; i < sliceCount; ++i)
	{
		float theta0 = i * dTheta;
		float theta1 = (i + 1) * dTheta;

		float oldx0 = radius * cosf(theta0);
		float oldz0 = radius * sinf(theta0);
		float oldx1 = radius * cosf(theta1);
		float oldz1 = radius * sinf(theta1);

		pushCone(oldx0, -halfHeight, oldz0);
		pushCone(0.0f, halfHeight, 0.0f);
		pushCone(oldx1, -halfHeight, oldz1);
	}

	// 밑면
	for (int i = 0; i < sliceCount; ++i)
	{
		float theta0 = i * dTheta;
		float theta1 = (i + 1) * dTheta;

		float oldx0 = radius * cosf(theta0);
		float oldz0 = radius * sinf(theta0);
		float oldx1 = radius * cosf(theta1);
		float oldz1 = radius * sinf(theta1);

		pushCone(0.0f, -halfHeight, 0.0f);
		pushCone(oldx0, -halfHeight, oldz0);
		pushCone(oldx1, -halfHeight, oldz1);
	}

	return coneVertices;
}

void ResourceManager::AddGizmoTranslationMesh(ID3D11Device* Device)
{
	TArray<FVertexSimple> vertices;

	TArray<FVertexSimple> cylinderVertices = CreateCylinderVertices();
	FMatrix cylinderScale = FMatrix::ScaleMatrix(0.05f, 0.05f, 1.0f);
	FMatrix cylinderMatrix = cylinderScale;

	for (FVertexSimple& v : cylinderVertices)
	{
		float px = v.x;
		float py = v.y;
		float pz = v.z;

		v.x = px * cylinderMatrix[0][0] + py * cylinderMatrix[1][0] + pz * cylinderMatrix[2][0] + 1.0f * cylinderMatrix[3][0];
		v.y = px * cylinderMatrix[0][1] + py * cylinderMatrix[1][1] + pz * cylinderMatrix[2][1] + 1.0f * cylinderMatrix[3][1];
		v.z = px * cylinderMatrix[0][2] + py * cylinderMatrix[1][2] + pz * cylinderMatrix[2][2] + 1.0f * cylinderMatrix[3][2];
	}
	vertices.insert(vertices.end(), cylinderVertices.begin(), cylinderVertices.end());

	TArray<FVertexSimple> coneVertices = CreateConeVertices();
	FMatrix coneTransform = FMatrix::TranslationMatrix(0.0f, 0.0f, 0.5f);
	FMatrix coneScale = FMatrix::ScaleMatrix(0.2f, 0.2f, 0.2f);
	FMatrix coneMatrix = coneScale * coneTransform;

	for (FVertexSimple& v : coneVertices)
	{
		float px = v.x;
		float py = v.y;
		float pz = v.z;

		v.x = px * coneMatrix[0][0] + py * coneMatrix[1][0] + pz * coneMatrix[2][0] + 1.0f * coneMatrix[3][0];
		v.y = px * coneMatrix[0][1] + py * coneMatrix[1][1] + pz * coneMatrix[2][1] + 1.0f * coneMatrix[3][1];
		v.z = px * coneMatrix[0][2] + py * coneMatrix[1][2] + pz * coneMatrix[2][2] + 1.0f * coneMatrix[3][2];
	}
	vertices.insert(vertices.end(), coneVertices.begin(), coneVertices.end());

	size_t cnt = vertices.size();
	TArray<FVector> Vertices(cnt);
	for (size_t i = 0; i < cnt; ++i)
	{
		Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
	}

	CreateStaticGeometry("GizmoTranslation", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
}

void ResourceManager::AddGizmoRotationMesh(ID3D11Device* Device)
{
	TArray<FVertexSimple> vertices = CreateTorusVertices();
	FMatrix torusScale = FMatrix::ScaleMatrix(1.5f, 1.5f, 1.5f);
	FMatrix torusMatrix = torusScale;

	for (FVertexSimple& v : vertices)
	{
		float px = v.x;
		float py = v.y;
		float pz = v.z;

		v.x = px * torusMatrix[0][0] + py * torusMatrix[1][0] + pz * torusMatrix[2][0] + 1.0f * torusMatrix[3][0];
		v.y = px * torusMatrix[0][1] + py * torusMatrix[1][1] + pz * torusMatrix[2][1] + 1.0f * torusMatrix[3][1];
		v.z = px * torusMatrix[0][2] + py * torusMatrix[1][2] + pz * torusMatrix[2][2] + 1.0f * torusMatrix[3][2];
	}

	size_t cnt = vertices.size();
	TArray<FVector> Vertices(cnt);
	for (size_t i = 0; i < cnt; ++i)
	{
		Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
	}

	CreateStaticGeometry("GizmoRotation", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
}

void ResourceManager::AddGizmoScaleMesh(ID3D11Device* Device)
{
	TArray<FVertexSimple> vertices;

	TArray<FVertexSimple> cylinderVertices = CreateCylinderVertices();
	FMatrix cylinderScale = FMatrix::ScaleMatrix(0.05f, 0.05f, 1.0f);
	FMatrix cylinderMatrix = cylinderScale;

	for (FVertexSimple& v : cylinderVertices)
	{
		float px = v.x;
		float py = v.y;
		float pz = v.z;

		v.x = px * cylinderMatrix[0][0] + py * cylinderMatrix[1][0] + pz * cylinderMatrix[2][0] + 1.0f * cylinderMatrix[3][0];
		v.y = px * cylinderMatrix[0][1] + py * cylinderMatrix[1][1] + pz * cylinderMatrix[2][1] + 1.0f * cylinderMatrix[3][1];
		v.z = px * cylinderMatrix[0][2] + py * cylinderMatrix[1][2] + pz * cylinderMatrix[2][2] + 1.0f * cylinderMatrix[3][2];
	}
	vertices.insert(vertices.end(), cylinderVertices.begin(), cylinderVertices.end());

	TArray<FVertexSimple> cubeVertices = CreateCubeVertices();
	FMatrix cubeTransform = FMatrix::TranslationMatrix(0.0f, 0.0f, 0.5f);
	FMatrix cubeScale = FMatrix::ScaleMatrix(0.2f, 0.2f, 0.2f);
	FMatrix cubeMatrix = cubeScale * cubeTransform;

	for (FVertexSimple& v : cubeVertices)
	{
		float px = v.x;
		float py = v.y;
		float pz = v.z;

		v.x = px * cubeMatrix[0][0] + py * cubeMatrix[1][0] + pz * cubeMatrix[2][0] + 1.0f * cubeMatrix[3][0];
		v.y = px * cubeMatrix[0][1] + py * cubeMatrix[1][1] + pz * cubeMatrix[2][1] + 1.0f * cubeMatrix[3][1];
		v.z = px * cubeMatrix[0][2] + py * cubeMatrix[1][2] + pz * cubeMatrix[2][2] + 1.0f * cubeMatrix[3][2];
	}
	vertices.insert(vertices.end(), cubeVertices.begin(), cubeVertices.end());

	size_t cnt = vertices.size();
	TArray<FVector> Vertices(cnt);
	for (size_t i = 0; i < cnt; ++i)
	{
		Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
	}

	CreateStaticGeometry("GizmoScale", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
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
	UpdateStaticGeomtry("Grid", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Renderer->Device.Get(), 0, Vertices);
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
