#pragma once
#include "UEngineTypes.h"
#include "d3d11.h"
#include "d3dcompiler.h"
#include "FVertexSimple.h"
#include "UEngineTypes.h"
#include "FMatrix.h"

#include "FVector.h"
#include "FMaterial.h"
#include "FGeometry.h"
class ResourceManager
{
public:
	static ResourceManager* GetInstance()
	{
		static ResourceManager instance;
		return &instance;
	};

	~ResourceManager() {};

	template<typename T>
	static void CreateGeometry(FString ResourceName, T* vertices, UINT byteWidth, ID3D11Device* Device, int offset, const TArray<FVector>& Verticies)
	{
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };
		ID3D11Buffer* vertexBuffer;
		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

		FGeometry& Geometry = GeometryMap[ResourceName];
		Geometry.VertexBuffer = vertexBuffer;
		Geometry.VertexStride = sizeof(T);
		Geometry.VertexCount = byteWidth / sizeof(T);
		Geometry.Offset = offset;
		Geometry.Vertices = Verticies;
	}

	static void CreateMaterial(WFString VertexShaderName, WFString PixelShaderName, ID3D11Device* Device, D3D11_INPUT_ELEMENT_DESC layout[], UINT layoutCount)
	{
		ID3D11VertexShader* VertexShader = nullptr;
		ID3D11InputLayout* InputLayout = nullptr;
		ID3DBlob* vertexshaderCSO = nullptr;
		D3DCompileFromFile(VertexShaderName.c_str(), nullptr, nullptr, "mainVS", "vs_5_0", 0,
			0, &vertexshaderCSO, nullptr);
		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(),
			vertexshaderCSO->GetBufferSize(), nullptr,
			&VertexShader);
		Device->CreateInputLayout(
			layout, layoutCount, vertexshaderCSO->GetBufferPointer(),
			vertexshaderCSO->GetBufferSize(), &InputLayout);

		ID3D11PixelShader* PixelShader = nullptr;
		ID3DBlob* pixelshaderCSO = nullptr;
		D3DCompileFromFile(PixelShaderName.c_str(), nullptr, nullptr, "mainPS", "ps_5_0", 0,
			0, &pixelshaderCSO, nullptr);
		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(),
			pixelshaderCSO->GetBufferSize(), nullptr,
			&PixelShader);

		FMaterial& Mat = MaterialMap[VertexShaderName];
		Mat.VertexShader = VertexShader;
		Mat.PixelShader = PixelShader;
		Mat.InputLayout = InputLayout;

		pixelshaderCSO->Release();
		vertexshaderCSO->Release();
	}

	//Temporary Function
	static void AddCubeMesh(ID3D11Device* Device);
	static void AddSphereMesh(ID3D11Device* Device);
	static void AddTriangleMesh(ID3D11Device* Device);

	static TArray<FVertexSimple> CreateCubeVertices()
	{
		TArray<FVertexSimple> cubeVertices =
		{
			// Front (+Z) - Red
			{ -0.5f,-0.5f, 0.5f, 1,0,0,1 },
			{ 0.5f,-0.5f, 0.5f, 1,0,0,1 },
			{ 0.5f, 0.5f, 0.5f, 1,0,0,1 },

			{ -0.5f,-0.5f, 0.5f, 1,0,0,1 },
			{ 0.5f, 0.5f, 0.5f, 1,0,0,1 },
			{ -0.5f, 0.5f, 0.5f, 1,0,0,1 },


			// Back (-Z) - Green
			{ 0.5f,-0.5f,-0.5f, 0,1,0,1 },
			{ -0.5f,-0.5f,-0.5f, 0,1,0,1 },
			{ -0.5f, 0.5f,-0.5f, 0,1,0,1 },

			{ 0.5f,-0.5f,-0.5f, 0,1,0,1 },
			{ -0.5f, 0.5f,-0.5f, 0,1,0,1 },
			{ 0.5f, 0.5f,-0.5f, 0,1,0,1 },


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
			{ 0.5f,0.5f, 0.5f, 0,1,1,1 },
			{ 0.5f,0.5f,-0.5f, 0,1,1,1 },

			{ -0.5f,0.5f, 0.5f, 0,1,1,1 },
			{ 0.5f,0.5f,-0.5f, 0,1,1,1 },
			{ -0.5f,0.5f,-0.5f, 0,1,1,1 },


			// Bottom (-Y) - Magenta
			{ -0.5f,-0.5f,-0.5f, 1,0,1,1 },
			{ 0.5f,-0.5f,-0.5f, 1,0,1,1 },
			{ 0.5f,-0.5f, 0.5f, 1,0,1,1 },

			{ -0.5f,-0.5f,-0.5f, 1,0,1,1 },
			{ 0.5f,-0.5f, 0.5f, 1,0,1,1 },
			{ -0.5f,-0.5f, 0.5f, 1,0,1,1 },
		};
		return cubeVertices;
	}

	static TArray<FVertexSimple> CreateCircleVertices(int sliceCount = 20, float radius = 0.5f, float height = 0.0f)
	{
		TArray<FVertexSimple> circleVertices;
		float dTheta = 2.0f * 3.14159265359f / sliceCount;
		for (int i = 0; i < sliceCount; ++i)
		{
			float theta0 = i * dTheta;
			float theta1 = (i + 1) * dTheta;
			float x0 = radius * cosf(theta0);
			float z0 = radius * sinf(theta0);
			float x1 = radius * cosf(theta1);
			float z1 = radius * sinf(theta1);
			circleVertices.push_back({ x0, height, z0, 1, 1, 1, 1 });
			circleVertices.push_back({ x1, height, z1, 1, 1, 1, 1 });
		}
		return circleVertices;
	}

	static TArray<FVertexSimple> CreateTorusVertices(float majorRadius = 1.0f, float minorRadius = 0.05f, int radialSegments = 32, int tubularSegments = 16)
	{
		TArray<FVertexSimple> torusVertices;
		float dU = 2.0f * 3.14159265359f / radialSegments;
		float dV = 2.0f * 3.14159265359f / tubularSegments;

		for (int i = 0; i < radialSegments; ++i)
		{
			for (int j = 0; j < tubularSegments; ++j)
			{
				float u1 = i * dU;
				float v1 = j * dV;
				float u2 = (i + 1) * dU;
				float v2 = (j + 1) * dV;


				auto GetTorusPoint = [=](float u, float v) -> FVector {
					float x = (majorRadius + minorRadius * cosf(v)) * cosf(u);
					float z = (majorRadius + minorRadius * cosf(v)) * sinf(u);
					float y = minorRadius * sinf(v);
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

	static TArray<FVertexSimple> CreateCylinderVertices(int sliceCount = 20, float radius = 0.5f, float height = 1.0f)
	{
		TArray<FVertexSimple> cylinderVertices;

		float halfHeight = height * 0.5f;
		float dTheta = 2.0f * 3.14159265359f / sliceCount;

		for (int i = 0; i < sliceCount; ++i)
		{
			float theta0 = i * dTheta;
			float theta1 = (i + 1) * dTheta;

			float x0 = radius * cosf(theta0);
			float z0 = radius * sinf(theta0);
			float x1 = radius * cosf(theta1);
			float z1 = radius * sinf(theta1);

			cylinderVertices.push_back({ x0, -halfHeight, z0, 1,0,0,1 });
			cylinderVertices.push_back({ x1, -halfHeight, z1, 1,0,0,1 });
			cylinderVertices.push_back({ x1, halfHeight, z1, 1,0,0,1 });

			cylinderVertices.push_back({ x0, -halfHeight, z0, 1,0,0,1 });
			cylinderVertices.push_back({ x1, halfHeight, z1, 1,0,0,1 });
			cylinderVertices.push_back({ x0, halfHeight, z0, 1,0,0,1 });
		}

		for (int i = 0; i < sliceCount; ++i)
		{
			float theta0 = i * dTheta;
			float theta1 = (i + 1) * dTheta;

			float x0 = radius * cosf(theta0);
			float z0 = radius * sinf(theta0);
			float x1 = radius * cosf(theta1);
			float z1 = radius * sinf(theta1);

			cylinderVertices.push_back({ x0, halfHeight, z0, 1,0,0,1 });
			cylinderVertices.push_back({ x1, halfHeight, z1, 1,0,0,1 });
			cylinderVertices.push_back({ 0, halfHeight, 0, 1,0,0,1 });
		}

		for (int i = 0; i < sliceCount; ++i)
		{
			float theta0 = i * dTheta;
			float theta1 = (i + 1) * dTheta;

			float x0 = radius * cosf(theta0);
			float z0 = radius * sinf(theta0);
			float x1 = radius * cosf(theta1);
			float z1 = radius * sinf(theta1);

			cylinderVertices.push_back({ x0, -halfHeight, z0, 1,0,0,1 });
			cylinderVertices.push_back({ 0, -halfHeight, 0, 1,0,0,1 });
			cylinderVertices.push_back({ x1, -halfHeight, z1, 1,0,0,1 });
		}

		return cylinderVertices;
	}

	static TArray<FVertexSimple> CreateConeVertices(int sliceCount = 20, float radius = 0.5f, float height = 1.0f)
	{
		TArray<FVertexSimple> coneVertices;

		float halfHeight = height * 0.5f;
		float dTheta = 2.0f * 3.14159265359f / sliceCount;

		// 측면
		for (int i = 0; i < sliceCount; ++i)
		{
			float theta0 = i * dTheta;
			float theta1 = (i + 1) * dTheta;

			float x0 = radius * cosf(theta0);
			float z0 = radius * sinf(theta0);
			float x1 = radius * cosf(theta1);
			float z1 = radius * sinf(theta1);

			coneVertices.push_back({ x0, -halfHeight, z0, 1, 1, 1, 1 });
			coneVertices.push_back({ 0, halfHeight, 0, 1, 1, 1, 1 });
			coneVertices.push_back({ x1, -halfHeight, z1, 1, 1, 1, 1 });
		}

		// 밑면
		for (int i = 0; i < sliceCount; ++i)
		{
			float theta0 = i * dTheta;
			float theta1 = (i + 1) * dTheta;

			float x0 = radius * cosf(theta0);
			float z0 = radius * sinf(theta0);
			float x1 = radius * cosf(theta1);
			float z1 = radius * sinf(theta1);

			coneVertices.push_back({ 0.0f, -halfHeight, 0.0f, 1, 1, 1, 1 });
			coneVertices.push_back({ x0, -halfHeight, z0, 1, 1, 1, 1 });
			coneVertices.push_back({ x1, -halfHeight, z1, 1, 1, 1, 1 });
		}

		return coneVertices;
	}

	static void AddGizmoTranslationMesh(ID3D11Device* Device)
	{
		// X Axis - Red
		{
			TArray<FVertexSimple> vertices;

			TArray<FVertexSimple> cylinderVertices = CreateCylinderVertices();
			FMatrix cylinderTransform = FMatrix::TranslationMatrix(0.5f, 0.0f, 0.0f);
			FMatrix cylinderRotation = FMatrix::RotationZMatrix(90.0f);
			FMatrix cylinderScale = FMatrix::ScaleMatrix(0.05f, 1.0f, 0.05f);
			FMatrix cylinderMatrix = cylinderScale * cylinderRotation * cylinderTransform;

			for (FVertexSimple& v : cylinderVertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * cylinderMatrix[0][0] + py * cylinderMatrix[1][0] + pz * cylinderMatrix[2][0] + 1.0f * cylinderMatrix[3][0];
				v.y = px * cylinderMatrix[0][1] + py * cylinderMatrix[1][1] + pz * cylinderMatrix[2][1] + 1.0f * cylinderMatrix[3][1];
				v.z = px * cylinderMatrix[0][2] + py * cylinderMatrix[1][2] + pz * cylinderMatrix[2][2] + 1.0f * cylinderMatrix[3][2];

				v.r = 1.0f;
				v.g = 0.0f;
				v.b = 0.0f;
			}
			vertices.insert(vertices.end(), cylinderVertices.begin(), cylinderVertices.end());

			TArray<FVertexSimple> coneVertices = CreateConeVertices();
			FMatrix coneTransform = FMatrix::TranslationMatrix(1.0f, 0.0f, 0.0f);
			FMatrix coneRotation = FMatrix::RotationZMatrix(-90.0f);
			FMatrix coneScale = FMatrix::ScaleMatrix(0.2f, 0.2f, 0.2f);
			FMatrix coneMatrix = coneScale * coneRotation * coneTransform;

			for (FVertexSimple& v : coneVertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * coneMatrix[0][0] + py * coneMatrix[1][0] + pz * coneMatrix[2][0] + 1.0f * coneMatrix[3][0];
				v.y = px * coneMatrix[0][1] + py * coneMatrix[1][1] + pz * coneMatrix[2][1] + 1.0f * coneMatrix[3][1];
				v.z = px * coneMatrix[0][2] + py * coneMatrix[1][2] + pz * coneMatrix[2][2] + 1.0f * coneMatrix[3][2];

				v.r = 1.0f;
				v.g = 0.0f;
				v.b = 0.0f;
			}
			vertices.insert(vertices.end(), coneVertices.begin(), coneVertices.end());

			size_t cnt = vertices.size();
			TArray<FVector> Vertices(cnt);
			for (size_t i = 0; i < cnt; ++i)
			{
				Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
			}

			CreateGeometry("GizmoTranslationX", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);

		}
		// Y Axis - Green
		{
			TArray<FVertexSimple> vertices;

			TArray<FVertexSimple> cylinderVertices = CreateCylinderVertices();
			FMatrix cylinderTransform = FMatrix::TranslationMatrix(0.0f, 0.0f, 0.5f);
			FMatrix cylinderRotation = FMatrix::RotationXMatrix(90.0f);
			FMatrix cylinderScale = FMatrix::ScaleMatrix(0.05f, 1.0f, 0.05f);
			FMatrix cylinderMatrix = cylinderScale * cylinderRotation * cylinderTransform;

			for (FVertexSimple& v : cylinderVertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * cylinderMatrix[0][0] + py * cylinderMatrix[1][0] + pz * cylinderMatrix[2][0] + 1.0f * cylinderMatrix[3][0];
				v.y = px * cylinderMatrix[0][1] + py * cylinderMatrix[1][1] + pz * cylinderMatrix[2][1] + 1.0f * cylinderMatrix[3][1];
				v.z = px * cylinderMatrix[0][2] + py * cylinderMatrix[1][2] + pz * cylinderMatrix[2][2] + 1.0f * cylinderMatrix[3][2];

				v.r = 0.0f;
				v.g = 1.0f;
				v.b = 0.0f;
			}
			vertices.insert(vertices.end(), cylinderVertices.begin(), cylinderVertices.end());

			TArray<FVertexSimple> coneVertices = CreateConeVertices();
			FMatrix coneTransform = FMatrix::TranslationMatrix(0.0f, 0.0f, 1.0f);
			FMatrix coneRotation = FMatrix::RotationXMatrix(90.0f);
			FMatrix coneScale = FMatrix::ScaleMatrix(0.2f, 0.2f, 0.2f);
			FMatrix coneMatrix = coneScale * coneRotation * coneTransform;

			for (FVertexSimple& v : coneVertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * coneMatrix[0][0] + py * coneMatrix[1][0] + pz * coneMatrix[2][0] + 1.0f * coneMatrix[3][0];
				v.y = px * coneMatrix[0][1] + py * coneMatrix[1][1] + pz * coneMatrix[2][1] + 1.0f * coneMatrix[3][1];
				v.z = px * coneMatrix[0][2] + py * coneMatrix[1][2] + pz * coneMatrix[2][2] + 1.0f * coneMatrix[3][2];

				v.r = 0.0f;
				v.g = 1.0f;
				v.b = 0.0f;
			}
			vertices.insert(vertices.end(), coneVertices.begin(), coneVertices.end());

			size_t cnt = vertices.size();
			TArray<FVector> Vertices(cnt);
			for (size_t i = 0; i < cnt; ++i)
			{
				Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
			}

			CreateGeometry("GizmoTranslationY", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
		}
		// Z Axis - Blue
		{
			TArray<FVertexSimple> vertices;

			TArray<FVertexSimple> cylinderVertices = CreateCylinderVertices();
			FMatrix cylinderTransform = FMatrix::TranslationMatrix(0.0f, 0.5f, 0.0f);
			FMatrix cylinderScale = FMatrix::ScaleMatrix(0.05f, 1.0f, 0.05f);
			FMatrix cylinderMatrix = cylinderScale * cylinderTransform;

			for (FVertexSimple& v : cylinderVertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * cylinderMatrix[0][0] + py * cylinderMatrix[1][0] + pz * cylinderMatrix[2][0] + 1.0f * cylinderMatrix[3][0];
				v.y = px * cylinderMatrix[0][1] + py * cylinderMatrix[1][1] + pz * cylinderMatrix[2][1] + 1.0f * cylinderMatrix[3][1];
				v.z = px * cylinderMatrix[0][2] + py * cylinderMatrix[1][2] + pz * cylinderMatrix[2][2] + 1.0f * cylinderMatrix[3][2];

				v.r = 0.0f;
				v.g = 0.0f;
				v.b = 1.0f;
			}
			vertices.insert(vertices.end(), cylinderVertices.begin(), cylinderVertices.end());

			TArray<FVertexSimple> coneVertices = CreateConeVertices();
			FMatrix coneTransform = FMatrix::TranslationMatrix(0.0f, 1.0f, 0.0f);
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

				v.r = 0.0f;
				v.g = 0.0f;
				v.b = 1.0f;
			}
			vertices.insert(vertices.end(), coneVertices.begin(), coneVertices.end());

			size_t cnt = vertices.size();
			TArray<FVector> Vertices(cnt);
			for (size_t i = 0; i < cnt; ++i)
			{
				Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
			}

			CreateGeometry("GizmoTranslationZ", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
		}
	}
	static void AddGizmoRotationMesh(ID3D11Device* Device)
	{
		// X Axis - Red
		{
			TArray<FVertexSimple> vertices = CreateTorusVertices();
			FMatrix torusRotation = FMatrix::RotationZMatrix(90.0f);
			FMatrix torusScale = FMatrix::ScaleMatrix(2.0, 2.0f, 2.0f);
			FMatrix torusMatrix = torusScale * torusRotation;

			for (FVertexSimple& v : vertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * torusMatrix[0][0] + py * torusMatrix[1][0] + pz * torusMatrix[2][0] + 1.0f * torusMatrix[3][0];
				v.y = px * torusMatrix[0][1] + py * torusMatrix[1][1] + pz * torusMatrix[2][1] + 1.0f * torusMatrix[3][1];
				v.z = px * torusMatrix[0][2] + py * torusMatrix[1][2] + pz * torusMatrix[2][2] + 1.0f * torusMatrix[3][2];

				v.r = 1.0f;
				v.g = 0.0f;
				v.b = 0.0f;
			}

			int cnt = vertices.size();
			TArray<FVector> Vertices(cnt);
			for (int i = 0; i < cnt; ++i)
			{
				Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
			}

			CreateGeometry("GizmoRotationX", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
		}
		// Y Axis - Green
		{
			TArray<FVertexSimple> vertices = CreateTorusVertices();
			FMatrix torusRotation = FMatrix::RotationXMatrix(90.0f);
			FMatrix torusScale = FMatrix::ScaleMatrix(2.0, 2.0f, 2.0f);
			FMatrix torusMatrix = torusScale * torusRotation;

			for (FVertexSimple& v : vertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * torusMatrix[0][0] + py * torusMatrix[1][0] + pz * torusMatrix[2][0] + 1.0f * torusMatrix[3][0];
				v.y = px * torusMatrix[0][1] + py * torusMatrix[1][1] + pz * torusMatrix[2][1] + 1.0f * torusMatrix[3][1];
				v.z = px * torusMatrix[0][2] + py * torusMatrix[1][2] + pz * torusMatrix[2][2] + 1.0f * torusMatrix[3][2];

				v.r = 0.0f;
				v.g = 1.0f;
				v.b = 0.0f;
			}
			int cnt = vertices.size();
			TArray<FVector> Vertices(cnt);
			for (int i = 0; i < cnt; ++i)
			{
				Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
			}
			CreateGeometry("GizmoRotationY", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
		}
		// Z Axis - Blue
		{
			TArray<FVertexSimple> vertices = CreateTorusVertices();
			FMatrix torusScale = FMatrix::ScaleMatrix(2.0, 2.0f, 2.0f);
			FMatrix torusMatrix = torusScale;

			for (FVertexSimple& v : vertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * torusMatrix[0][0] + py * torusMatrix[1][0] + pz * torusMatrix[2][0] + 1.0f * torusMatrix[3][0];
				v.y = px * torusMatrix[0][1] + py * torusMatrix[1][1] + pz * torusMatrix[2][1] + 1.0f * torusMatrix[3][1];
				v.z = px * torusMatrix[0][2] + py * torusMatrix[1][2] + pz * torusMatrix[2][2] + 1.0f * torusMatrix[3][2];

				v.r = 0.0f;
				v.g = 0.0f;
				v.b = 1.0f;
			}
			int cnt = vertices.size();
			TArray<FVector> Vertices(cnt);
			for (int i = 0; i < cnt; ++i)
			{
				Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
			}
			CreateGeometry("GizmoRotationZ", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
		}
	}
	static void AddGizmoScaleMesh(ID3D11Device* Device)
	{
		// X Axis - Red
		{
			TArray<FVertexSimple> vertices;

			TArray<FVertexSimple> cylinderVertices = CreateCylinderVertices();
			FMatrix cylinderTransform = FMatrix::TranslationMatrix(0.5f, 0.0f, 0.0f);
			FMatrix cylinderRotation = FMatrix::RotationZMatrix(90.0f);
			FMatrix cylinderScale = FMatrix::ScaleMatrix(0.05f, 1.0f, 0.05f);
			FMatrix cylinderMatrix = cylinderScale * cylinderRotation * cylinderTransform;

			for (FVertexSimple& v : cylinderVertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * cylinderMatrix[0][0] + py * cylinderMatrix[1][0] + pz * cylinderMatrix[2][0] + 1.0f * cylinderMatrix[3][0];
				v.y = px * cylinderMatrix[0][1] + py * cylinderMatrix[1][1] + pz * cylinderMatrix[2][1] + 1.0f * cylinderMatrix[3][1];
				v.z = px * cylinderMatrix[0][2] + py * cylinderMatrix[1][2] + pz * cylinderMatrix[2][2] + 1.0f * cylinderMatrix[3][2];

				v.r = 1.0f;
				v.g = 0.0f;
				v.b = 0.0f;
			}
			vertices.insert(vertices.end(), cylinderVertices.begin(), cylinderVertices.end());

			TArray<FVertexSimple> cubeVertices = CreateCubeVertices();
			FMatrix cubeTransform = FMatrix::TranslationMatrix(1.0f, 0.0f, 0.0f);
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

				v.r = 1.0f;
				v.g = 0.0f;
				v.b = 0.0f;
			}
			vertices.insert(vertices.end(), cubeVertices.begin(), cubeVertices.end());

			size_t cnt = vertices.size();
			TArray<FVector> Vertices(cnt);
			for (size_t i = 0; i < cnt; ++i)
			{
				Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
			}

			CreateGeometry("GizmoScaleX", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);

		}
		// Y Axis - Green
		{
			TArray<FVertexSimple> vertices;

			TArray<FVertexSimple> cylinderVertices = CreateCylinderVertices();
			FMatrix cylinderTransform = FMatrix::TranslationMatrix(0.0f, 0.0f, 0.5f);
			FMatrix cylinderRotation = FMatrix::RotationXMatrix(90.0f);
			FMatrix cylinderScale = FMatrix::ScaleMatrix(0.05f, 1.0f, 0.05f);
			FMatrix cylinderMatrix = cylinderScale * cylinderRotation * cylinderTransform;

			for (FVertexSimple& v : cylinderVertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * cylinderMatrix[0][0] + py * cylinderMatrix[1][0] + pz * cylinderMatrix[2][0] + 1.0f * cylinderMatrix[3][0];
				v.y = px * cylinderMatrix[0][1] + py * cylinderMatrix[1][1] + pz * cylinderMatrix[2][1] + 1.0f * cylinderMatrix[3][1];
				v.z = px * cylinderMatrix[0][2] + py * cylinderMatrix[1][2] + pz * cylinderMatrix[2][2] + 1.0f * cylinderMatrix[3][2];

				v.r = 0.0f;
				v.g = 1.0f;
				v.b = 0.0f;
			}
			vertices.insert(vertices.end(), cylinderVertices.begin(), cylinderVertices.end());

			TArray<FVertexSimple> cubeVertices = CreateCubeVertices();
			FMatrix cubeTransform = FMatrix::TranslationMatrix(0.0f, 0.0f, 1.0f);
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

				v.r = 0.0f;
				v.g = 1.0f;
				v.b = 0.0f;
			}
			vertices.insert(vertices.end(), cubeVertices.begin(), cubeVertices.end());

			size_t cnt = vertices.size();
			TArray<FVector> Vertices(cnt);
			for (size_t i = 0; i < cnt; ++i)
			{
				Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
			}

			CreateGeometry("GizmoScaleY", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
		}
		// Z Axis - Blue
		{
			TArray<FVertexSimple> vertices;

			TArray<FVertexSimple> cylinderVertices = CreateCylinderVertices();
			FMatrix cylinderTransform = FMatrix::TranslationMatrix(0.0f, 0.5f, 0.0f);
			FMatrix cylinderScale = FMatrix::ScaleMatrix(0.05f, 1.0f, 0.05f);
			FMatrix cylinderMatrix = cylinderScale * cylinderTransform;

			for (FVertexSimple& v : cylinderVertices)
			{
				float px = v.x;
				float py = v.y;
				float pz = v.z;

				v.x = px * cylinderMatrix[0][0] + py * cylinderMatrix[1][0] + pz * cylinderMatrix[2][0] + 1.0f * cylinderMatrix[3][0];
				v.y = px * cylinderMatrix[0][1] + py * cylinderMatrix[1][1] + pz * cylinderMatrix[2][1] + 1.0f * cylinderMatrix[3][1];
				v.z = px * cylinderMatrix[0][2] + py * cylinderMatrix[1][2] + pz * cylinderMatrix[2][2] + 1.0f * cylinderMatrix[3][2];

				v.r = 0.0f;
				v.g = 0.0f;
				v.b = 1.0f;
			}
			vertices.insert(vertices.end(), cylinderVertices.begin(), cylinderVertices.end());

			TArray<FVertexSimple> cubeVertices = CreateCubeVertices();
			FMatrix cubeTransform = FMatrix::TranslationMatrix(0.0f, 1.0f, 0.0f);
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

				v.r = 0.0f;
				v.g = 0.0f;
				v.b = 1.0f;
			}
			vertices.insert(vertices.end(), cubeVertices.begin(), cubeVertices.end());

			size_t cnt = vertices.size();
			TArray<FVector> Vertices(cnt);
			for (size_t i = 0; i < cnt; ++i)
			{
				Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
			}

			CreateGeometry("GizmoScaleZ", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
		}
	}

	static FMaterial& GetMaterial(WFString ResourceName)
	{
		return MaterialMap[ResourceName];
	}
	static FGeometry& GetGeometry(FString ResourceName)
	{
		return GeometryMap[ResourceName];
	}
private:
	ResourceManager() {};
	static TMap<FString, FGeometry> GeometryMap;
	static TMap<WFString, FMaterial> MaterialMap;
};