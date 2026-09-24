#include "ResourceManager.h"
#include "VertexSimple.h"
#include "ThirdParty/DDSTextureLoader.h"
#include <iterator>
#include "Geometry/Sphere.h"
#include "Geometry/Triangle.h"
#include "Renderer/Renderer.h"
#include "EngineCore/Logger.h"

#include "EngineCore/Logger.h"
#include "Text/TextEncoding.h"

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
	LineShader.ConstantBuffer.Create(Renderer->Device.Get(), sizeof(FObjectConstant), 1);

	D3D11_INPUT_ELEMENT_DESC texturedLayout[] = {
{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
 D3D11_INPUT_PER_VERTEX_DATA, 0},
{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
 D3D11_INPUT_PER_VERTEX_DATA, 0},
{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28,
 D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	FMaterial& SubUVShader = CreateMaterial(L"Shader/ShaderSubUV.hlsl", L"Shader/ShaderSubUV.hlsl", Renderer->Device.Get(), texturedLayout, 3);
	SubUVShader.ConstantBuffer.Create(Renderer->Device.Get(), sizeof(FObjectConstant), 1);
	SubUVShader.SecondaryConstantBuffer.Create(Renderer->Device.Get(), sizeof(FSubUVConstant), 2);
	SubUVShader.bUsesTexture = true;
	SubUVShader.bUsesSecondaryConstantBuffer = true;
	// for ShaderFont.hlsl
	D3D11_INPUT_ELEMENT_DESC fontLayout[] =
	{
		{ "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 12,
		  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",  1, DXGI_FORMAT_R32G32_FLOAT,    0, 20,
		  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	Renderer->Device->CreateSamplerState(&samplerDesc, SubUVShader.SamplerState.GetAddressOf());
	SubUVShader.TextureSRV = LoadDDSTexture(L"SubUVTexture", L"Textures/input.dds");


	FMaterial& FontShader = CreateMaterial(
		L"Shader/ShaderFont.hlsl",
		L"Shader/ShaderFont.hlsl",
		Renderer->Device.Get(),
		fontLayout,
		3
	);

	FontShader.ConstantBuffer.Create(
		Renderer->Device.Get(),
		sizeof(FFontConstant),
		1
	);

	FontShader.bUsesTexture = true;

	FMaterial& TextMeshShader = CreateMaterial(
		L"Shader/ShaderTextMesh.hlsl",
		L"Shader/ShaderTextMesh.hlsl",
		Renderer->Device.Get(),
		texturedLayout,
		3
	);

	TextMeshShader.ConstantBuffer.Create(
		Renderer->Device.Get(),
		sizeof(FObjectConstant),
		1
	);
	TextMeshShader.bUsesTexture = true;

	AddCubeMesh(Renderer->Device.Get());
	AddSphereMesh(Renderer->Device.Get());
	AddTriangleMesh(Renderer->Device.Get());
	AddPlaneMesh(Renderer->Device.Get());
	AddBillboardQuadMesh(Renderer->Device.Get());
	AddGizmoTranslationMesh(Renderer->Device.Get());
	AddGizmoRotationMesh(Renderer->Device.Get());
	AddGizmoScaleMesh(Renderer->Device.Get());
	AddAxisMesh(Renderer->Device.Get());
	AddGridMesh(Renderer->Device.Get(), 1000.f, 100.f);

	// Font asset 로딩
	const bool bFontLoaded = BillboardFont.LoadFromFile("Assets/Fonts/D2CodingSubset.ttf", 32.0f);
	if (!bFontLoaded)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Failed to load billboard font");
	}

	// 숫자 등 자주 사용하는 문자는 미리 bake
	BillboardFont.EnsureGlyph(static_cast<uint32>('U'));
	BillboardFont.EnsureGlyph(static_cast<uint32>('I'));
	BillboardFont.EnsureGlyph(static_cast<uint32>('D'));
	BillboardFont.EnsureGlyph(static_cast<uint32>(':'));
	for (char c = '0'; c <= '9'; c++)
	{
		BillboardFont.EnsureGlyph(static_cast<uint32>(c));
	}

	const bool bAtlasTextureCreated = CreateBillboardFontAtlasTexture();
	if (!bAtlasTextureCreated)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Failed to create font atlas texture");
	}
}

bool ResourceManager::EnsureBillboardGlyphs(const FString& Text)
{
	if (Text.empty())
	{
		return true;
	}

	if (Renderer == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Renderer is null");
		return false;
	}

	if (Renderer->Device == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Direct3D 11 device is null");
		return false;
	}

	if (BillboardFontTexture == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Billboard font texture is not initialized");
		return false;
	}

	FU32String Codepoints = Utf8ToCodepoints(Text);
	if (Codepoints.empty())
	{
		return true;
	}

	for (uint32 Codepoint : Codepoints)
	{
		// 개행문자 처리
		if (Codepoint == U'\n' || Codepoint == U'\r')
		{
			continue;
		}

		if (BillboardFont.HasGlyph(Codepoint))
		{
			continue;
		}

		const FGlyphInfo* Glyph = BillboardFont.EnsureGlyph(Codepoint);
		if (Glyph == nullptr)
		{
			UE_LOG("[EngineCore/ResourceManager][Error] Failed to ensure billboard glyph");
			return false;
		}

		// 공백 문자의 경우 실제 비트맵 크기를 가지지는 않으므로 아래의 UpdateBillboardFontAtlasRegion에서 실패 처리되지 않도록 skip
		if (Glyph->AtlasRect.Width <= 0.0f || Glyph->AtlasRect.Height <= 0.0f)
		{
			continue;
		}

		// Atlas에 새로 복사된 영역만 GPU texture에 부분 업데이트
		const bool bUpdated = UpdateBillboardFontAtlasRegion(Glyph->AtlasRect);
		if (!bUpdated)
		{
			UE_LOG("[EngineCore/ResourceManager][Error] Failed to update billboard font atlas region");
			return false;
		}
	}

	return true;
}

bool ResourceManager::CreateBillboardFontAtlasTexture(void)
{
	if (Renderer == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Renderer is null");
		return false;
	}

	if (Renderer->Device == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Direct3D 11 device is null");
		return false;
	}

	const FGlyphAtlas& Atlas = BillboardFont.GetAtlas();
	const uint32 AtlasWidth = Atlas.GetWidth();
	const uint32 AtlasHeight = Atlas.GetHeight();
	const TArray<uint8>& AtlasPixels = Atlas.GetPixels();

	if (AtlasWidth == 0 || AtlasHeight == 0 || AtlasPixels.empty())
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Invalid billboard font atlas data");
		return false;
	}

	BillboardFontTexture.Reset();

	FMaterial& FontShader = GetMaterial(L"Shader/ShaderFont.hlsl");
	// 지금 GetMaterial은 해당하는 셰이더가 없어도 빈 FMaterial을 만들어서 반환하는데, 나중에 안정성을 위해 추가해야할 듯

	FontShader.TextureSRV.Reset();
	FontShader.SamplerState.Reset();

	// Atlas 전체 texture 생성
	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = AtlasWidth;
	TextureDesc.Height = AtlasHeight;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_R8_UNORM; // 8-bit R 채널 하나만 사용
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.SampleDesc.Quality = 0;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	// 초기 데이터로 Atlas 전체 업로드
	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = AtlasPixels.data();
	InitData.SysMemPitch = AtlasWidth * sizeof(uint8);
	InitData.SysMemSlicePitch = 0;

	HRESULT hr = Renderer->Device->CreateTexture2D(
		&TextureDesc,
		&InitData,
		BillboardFontTexture.GetAddressOf()
	);

	if (FAILED(hr) || BillboardFontTexture == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Failed to create billboard font texture");
		return false;
	}

	// SRV 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Format = TextureDesc.Format;
	SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Texture2D.MostDetailedMip = 0;
	SrvDesc.Texture2D.MipLevels = 1;

	hr = Renderer->Device->CreateShaderResourceView(
		BillboardFontTexture.Get(),
		&SrvDesc,
		FontShader.TextureSRV.GetAddressOf()
	);

	if (FAILED(hr) || FontShader.TextureSRV == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Failed to create billboard font SRV");
		return false;
	}

	// Sampler 생성
	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = Renderer->Device->CreateSamplerState(
		&SamplerDesc,
		FontShader.SamplerState.GetAddressOf());

	if (FAILED(hr) || FontShader.SamplerState == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Failed to create billboard font sampler");
		FontShader.TextureSRV.Reset();
		return false;
	}

	// 셰이더에 연결
	FontShader.bUsesTexture = true;
	FMaterial& TextMeshShader = GetMaterial(L"Shader/ShaderTextMesh.hlsl");
	TextMeshShader.TextureSRV = FontShader.TextureSRV;
	TextMeshShader.SamplerState = FontShader.SamplerState;
	TextMeshShader.bUsesTexture = true;

	return true;
}

bool ResourceManager::UpdateBillboardFontAtlasRegion(const FAtlasRect& DirtyRect)
{
	if (Renderer == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Renderer is null");
		return false;
	}

	if (Renderer->DeviceContext == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Direct3D 11 device context is null");
		return false;
	}

	if (BillboardFontTexture == nullptr)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Billboard font texture is null");
		return false;
	}

	if (DirtyRect.Width <= 0.0f || DirtyRect.Height <= 0.0f)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] DirtyRect size is invalid");
		return false;
	}

	const FGlyphAtlas& Atlas = BillboardFont.GetAtlas();
	const TArray<uint8>& Pixels = Atlas.GetPixels();
	const uint32 AtlasWidth = Atlas.GetWidth();
	const uint32 AtlasHeight = Atlas.GetHeight();

	if (Pixels.empty() || AtlasWidth == 0 || AtlasHeight == 0)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] Invalid atlas pixel data");
		return false;
	}

	// DirtyRect를 정수 좌표로 변환
	const uint32 DirtyRectX = static_cast<uint32>(DirtyRect.X);
	const uint32 DirtyRectY = static_cast<uint32>(DirtyRect.Y);
	const uint32 DirtyRectWidth = static_cast<uint32>(DirtyRect.Width);
	const uint32 DirtyRectHeight = static_cast<uint32>(DirtyRect.Height);

	if (DirtyRectX + DirtyRectWidth > AtlasWidth || DirtyRectY + DirtyRectHeight > AtlasHeight)
	{
		UE_LOG("[EngineCore/ResourceManager][Error] DirtyRect is out of atlas bounds");
		return false;
	}

	// 임시 버퍼 생성
	TArray<uint8> UploadBuffer;
	UploadBuffer.resize(DirtyRectWidth * DirtyRectHeight);

	// Atlas에서 rect 영역만 줄 단위 복사
	for (uint32 Row = 0; Row < DirtyRectHeight; Row++)
	{
		const uint32 SrcOffset = (DirtyRectY + Row) * AtlasWidth + DirtyRectX;
		const uint32 DstOffset = Row * DirtyRectWidth;

		// Width 바이트만 복붙
		std::memcpy(
			UploadBuffer.data() + DstOffset,
			Pixels.data() + SrcOffset,
			DirtyRectWidth * sizeof(uint8)
		);
	}

	// D3D11_BOX 구성
	D3D11_BOX UpdateBox = {};
	UpdateBox.left = DirtyRectX;
	UpdateBox.top = DirtyRectY;
	UpdateBox.front = 0;
	UpdateBox.right = DirtyRectX + DirtyRectWidth;
	UpdateBox.bottom = DirtyRectY + DirtyRectHeight;
	UpdateBox.back = 1;

	// GPU atlas 텍스처 업데이트
	Renderer->DeviceContext->UpdateSubresource(
		BillboardFontTexture.Get(),
		0, // Subresource
		&UpdateBox, // DstBox
		UploadBuffer.data(), // SrcData
		DirtyRectWidth * sizeof(uint8), // SrcRowPitch
		0 // SrcDepthPitch(2D texture에서는 무의미)
	);

	return true;
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

TComPtr<ID3D11ShaderResourceView> ResourceManager::LoadDDSTexture(WFString ResourceName, WFString FilePath)
{
	auto iter = TextureMap.find(ResourceName);
	if (iter != TextureMap.end())
	{
		return iter->second;
	}

	TComPtr<ID3D11ShaderResourceView> textureView;
	const HRESULT hr = DirectX::CreateDDSTextureFromFile(
		Renderer->Device.Get(),
		FilePath.c_str(),
		nullptr,
		textureView.GetAddressOf());
	
	if (FAILED(hr))
	{
		const FString path(FilePath.begin(), FilePath.end());
		UE_LOG("CreateDDSTextureFromFile failed. path=%s hr=0x%08X\n", path.c_str(), static_cast<unsigned int>(hr));
		return nullptr;
	}

	TextureMap[ResourceName] = textureView;
	return textureView;
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

void ResourceManager::AddBillboardQuadMesh(ID3D11Device* Device)
{
	FVertexSimple quadVertices[] =
	{
		{ 0.0f, -0.5f, -0.5f, 1.f, 1.f, 1.f, 1.f, 0.0f, 1.0f },
		{ 0.0f, -0.5f,  0.5f, 1.f, 1.f, 1.f, 1.f, 0.0f, 0.0f },
		{ 0.0f,  0.5f,  0.5f, 1.f, 1.f, 1.f, 1.f, 1.0f, 0.0f },
		{ 0.0f,  0.5f, -0.5f, 1.f, 1.f, 1.f, 1.f, 1.0f, 1.0f },
	};
	uint32_t quadIndices[] = { 0, 1, 2, 0, 2, 3 };

	TArray<FVector> Vertices(std::size(quadVertices));
	for (size_t i = 0; i < std::size(quadVertices); ++i)
	{
		Vertices[i] = FVector(quadVertices[i].x, quadVertices[i].y, quadVertices[i].z);
	}

	CreateStaticGeometry("BillboardQuad", quadVertices, sizeof(quadVertices), Device, 0, Vertices, quadIndices, sizeof(quadIndices));
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
		// x, y, z, r, g, b, a
		{ -0.5f, -0.5f, -0.5f, 1, 0, 0, 1 }, // 0
		{ -0.5f,  0.5f, -0.5f, 0, 1, 0, 1 }, // 1
		{  0.5f,  0.5f, -0.5f, 0, 0, 1, 1 }, // 2
		{  0.5f, -0.5f, -0.5f, 1, 1, 0, 1 }, // 3
		{ -0.5f, -0.5f,  0.5f, 1, 0, 1, 1 }, // 4
		{ -0.5f,  0.5f,  0.5f, 0, 1, 1, 1 }, // 5
		{  0.5f,  0.5f,  0.5f, 1, 1, 1, 1 }, // 6
		{  0.5f, -0.5f,  0.5f, 0, 0, 0, 1 }, // 7
	};

	data.Indices =
	{
		// Front (+Z)
		4, 6, 5,
		4, 7, 6,

		// Back (-Z)
		3, 1, 2,
		3, 0, 1,

		// Left (-X)
		0, 5, 1,
		0, 4, 5,

		// Right (+X)
		7, 2, 6,
		7, 3, 2,

		// Top (+Y)
		1, 6, 2,
		1, 5, 6,

		// Bottom (-Y)
		0, 7, 4,
		0, 3, 7
	};
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

		float r = 0.15f;
		float g = 0.15f;
		float b = 0.15f;
		float a = 1.0f;

		if (i == 0)
		{
			r = 1.0f;
			g = 1.0f;
			b = 1.0f;
			a = 1.0f;
		}
		else if ((std::abs(i) % 10) == 0)
		{
			r = 0.55f;
			g = 0.55f;
			b = 0.55f;
		}

		if (i == 0)
		{
			// Keep the positive X/Y directions for the world axes so the grid center lines
			// stop at the origin instead of overlapping the axes.
			AddLine(vertices, 0.0f, -halfExtent, z, 0.0f, 0.0f, z, r, g, b, a);
			AddLine(vertices, -halfExtent, 0.0f, z, 0.0f, 0.0f, z, r, g, b, a);
			continue;
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
	TArray<FColor> Colors(cnt);
	for (size_t i = 0; i < cnt; ++i)
	{
		Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
		Colors[i] = FColor(vertices[i].r, vertices[i].g, vertices[i].b, vertices[i].a);
	}
	CreateNoBufferGeometry("Grid", Vertices);
	GetGeometry("Grid").Colors = Colors;
}

void ResourceManager::UpdateGridMesh(float halfExtent, float cellSize, float z)
{
	TArray<FVertexSimple> vertices = MakeGridMesh(Renderer->Device.Get(), halfExtent, cellSize, z);
	size_t cnt = vertices.size();
	TArray<FVector> Vertices(cnt);
	TArray<FColor> Colors(cnt);
	for (size_t i = 0; i < cnt; ++i)
	{
		Vertices[i] = FVector(vertices[i].x, vertices[i].y, vertices[i].z);
		Colors[i] = FColor(vertices[i].r, vertices[i].g, vertices[i].b, vertices[i].a);
	}
	UpdateStaticGeomtry("Grid", vertices.data(), (UINT)(sizeof(FVertexSimple) * vertices.size()), Renderer->Device.Get(), 0, Vertices);
	GetGeometry("Grid").Colors = Colors;
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
	TextureMap.clear();
}
