#pragma once
#include "UEngineTypes.h"
#include "d3d11.h"
#include "d3dcompiler.h"
#include "FVertexSimple.h"
#include "UEngineTypes.h"
#include "FMatrix.h"

#include "FVector.h"
struct VertexResource
{
	ID3D11Buffer* VertexBuffer;
	TArray<FVector> Vertices;
	int stride = 0;
	int vertexCount = 0;
	int offset = 0;
};
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
	static void CreateVertexResource(FString ResourceName, T* vertices, UINT byteWidth, ID3D11Device* Device, int offset, const TArray<FVector>& Verticies)
	{
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };
		ID3D11Buffer* vertexBuffer;
		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

		VertexResource& Resource = VertexResourceMap[ResourceName];
		Resource.VertexBuffer = vertexBuffer;
		Resource.stride = sizeof(T);
		Resource.vertexCount = byteWidth / sizeof(T);
		Resource.offset = offset;
		Resource.Vertices = Verticies;
	}

	static void CompileVertexShaderAndLayout(WFString VertexShaderName, FString ShaderMainName, ID3D11Device* Device, D3D11_INPUT_ELEMENT_DESC layout[], UINT layoutCount)
	{
		ID3D11VertexShader* VertexShader = nullptr;
		ID3D11InputLayout* InputLayout = nullptr;
		ID3DBlob* vertexshaderCSO = nullptr;
		D3DCompileFromFile(VertexShaderName.c_str(), nullptr, nullptr, ShaderMainName.c_str(), "vs_5_0", 0,
			0, &vertexshaderCSO, nullptr);
		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(),
			vertexshaderCSO->GetBufferSize(), nullptr,
			&VertexShader);
		Device->CreateInputLayout(
			layout, layoutCount, vertexshaderCSO->GetBufferPointer(),
			vertexshaderCSO->GetBufferSize(), &InputLayout);

		VertexShaderMap[VertexShaderName] = VertexShader;
		InputLayoutMap[VertexShaderName] = InputLayout;

		vertexshaderCSO->Release();
	}

	static void CompilePixelShader(WFString PixelShaderName, FString ShaderMainName, ID3D11Device* Device)
	{
		ID3D11PixelShader* PixelShader = nullptr;
		ID3DBlob* pixelshaderCSO = nullptr;
		D3DCompileFromFile(PixelShaderName.c_str(), nullptr, nullptr, ShaderMainName.c_str(), "ps_5_0", 0,
			0, &pixelshaderCSO, nullptr);
		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(),
			pixelshaderCSO->GetBufferSize(), nullptr,
			&PixelShader);
		PixelShaderMap[PixelShaderName] = PixelShader;
		pixelshaderCSO->Release();
	}



	//Temporary Function
	static void AddCubeMesh(ID3D11Device* Device)
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
		for (int i = 0;i < Cnt;++i)
		{
			Vertices[i] = FVector(cubeVertices[i].x, cubeVertices[i].y, cubeVertices[i].z);
		}
		CreateVertexResource("Cube", cubeVertices, sizeof(cubeVertices), Device, 0, Vertices);
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
		TArray<FVertexSimple> vertices;

		// X Axis - Red
		{
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
		}
		// Y Axis - Green
		{
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
		}
		// Z Axis - Blue
		{
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
		}

		int cnt = vertices.size();
		TArray<FVector> Vertices(cnt);
		for (int i = 0; i < cnt; ++i)
		{
			Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
		}

		CreateVertexResource("GizmoTranslation", vertices.data(), sizeof(FVertexSimple) * vertices.size(), Device, 0, Vertices);
	}

	static VertexResource& GetVertexResource(FString ResourceName)
	{
		return VertexResourceMap[ResourceName];
	}
	static ID3D11InputLayout* GetInputLayout(WFString VertexShaderName)
	{
		return InputLayoutMap[VertexShaderName];
	}
	static ID3D11VertexShader* GetVertexShader(WFString VertexShaderName)
	{
		return VertexShaderMap[VertexShaderName];
	}
	static ID3D11PixelShader* GetPixelShader(WFString PixelShaderName)
	{
		return PixelShaderMap[PixelShaderName];
	}
private:
	ResourceManager() {};
	static TMap<FString, VertexResource> VertexResourceMap;
	static TMap<WFString, ID3D11InputLayout*> InputLayoutMap;
	static TMap<WFString, ID3D11PixelShader*> PixelShaderMap;
	static TMap<WFString, ID3D11VertexShader*> VertexShaderMap;
};