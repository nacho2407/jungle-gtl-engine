#include "Renderer/TextMeshBuilder.h"

#include "Core/Paths.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/Shader.h"
#include "Renderer/ShaderMap.h"
#include "Renderer/ShaderResource.h"
#include "Renderer/ShaderType.h"

#include <algorithm>
#include <cstring>

FTextMeshBuilder::~FTextMeshBuilder()
{
	Release();
}

bool FTextMeshBuilder::Initialize(FRenderer* InRenderer)
{
	Release();

	if (!InRenderer)
	{
		return false;
	}

	Device = InRenderer->GetDevice();
	DeviceContext = InRenderer->GetDeviceContext();
	if (!Device || !DeviceContext)
	{
		return false;
	}

	// 폰트 아틀라스 초기화
	const std::wstring FontPath = (FPaths::ContentDir() / "Fonts/NotoSansKR_Atlas.png").wstring();
	if (!Atlas.Initialize(Device, DeviceContext, FontPath))
	{
		return false;
	}

	// 전용 머티리얼 구성
	const std::wstring ShaderDir = FPaths::ShaderDir();
	const std::wstring VSPath = ShaderDir + L"FontVertexShader.hlsl";
	const std::wstring PSPath = ShaderDir + L"FontPixelShader.hlsl";

	auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
	auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, PSPath.c_str());

	FontMaterial = std::make_shared<FMaterial>();
	FontMaterial->SetOriginName("M_Font");
	FontMaterial->SetVertexShader(VS);
	FontMaterial->SetPixelShader(PS);

	auto MaterialTexture = std::make_shared<FMaterialTexture>();
	MaterialTexture->SetResources(Atlas.GetTextureSRV(), Atlas.GetSamplerState(), false);
	FontMaterial->SetMaterialTexture(MaterialTexture);

	// b2: TextConstantBuffer (TextColor)
	const int32 SlotIndex = FontMaterial->CreateConstantBuffer(Device, 16);
	if (SlotIndex >= 0)
	{
		FontMaterial->RegisterParameter("TextColor", SlotIndex, 0, 16);
		const FVector4 White(1.0f, 1.0f, 1.0f, 1.0f);
		FontMaterial->SetParameterData("TextColor", &White, 16);
	}

	return true;
}

void FTextMeshBuilder::Release()
{
	Atlas.Release();
	FontMaterial.reset();
	Device = nullptr;
	DeviceContext = nullptr;
}

bool FTextMeshBuilder::BuildTextMesh(const FString& Text, FRenderMesh& OutMesh, float LetterSpacing) const
{
	if (Text.empty())
	{
		return false;
	}

	const TArray<uint32> Codepoints = DecodeToCodepoints(Text);
	if (Codepoints.empty())
	{
		return false;
	}

	const float SpacingScale = (std::max)(0.0f, LetterSpacing);

	auto ResolveAdvance = [SpacingScale](uint32 Codepoint, float BaseAdvance) -> float
	{
		// Grid atlas uses fixed cell metrics; tighten latin tracking to avoid
		// excessive spacing in toolbar/button/dropdown labels.
		if (Codepoint <= 0x007E)
		{
			if (Codepoint == static_cast<uint32>(' '))
			{
				return BaseAdvance * SpacingScale;
			}
			return BaseAdvance * 0.82f * SpacingScale;
		}
		return BaseAdvance * SpacingScale;
	};

	float TotalWidth = 0.0f;
	for (uint32 Codepoint : Codepoints)
	{
		const FFontGlyph& Glyph = Atlas.GetGlyph(Codepoint);
		TotalWidth += ResolveAdvance(Codepoint, Glyph.Advance);
	}

	float PenX = -TotalWidth * 0.5f;

	OutMesh.Vertices.clear();
	OutMesh.Indices.clear();
	OutMesh.Topology = EMeshTopology::EMT_TriangleList;

	for (uint32 Codepoint : Codepoints)
	{
		const FFontGlyph& Glyph = Atlas.GetGlyph(Codepoint);

		if (Glyph.Width > 0.0f && Glyph.Height > 0.0f)
		{
			const float X0 = PenX;
			const float X1 = PenX + Glyph.Width;
			const float Y0 = 0.0f;
			const float Y1 = Glyph.Height;

			const uint32 BaseIndex = static_cast<uint32>(OutMesh.Vertices.size());

			FVertex V0;
			FVertex V1;
			FVertex V2;
			FVertex V3;

			V0.Position = FVector(0.0f, X0, Y1);
			V1.Position = FVector(0.0f, X1, Y1);
			V2.Position = FVector(0.0f, X1, Y0);
			V3.Position = FVector(0.0f, X0, Y0);

			V0.UV = FVector2(Glyph.U0, Glyph.V0);
			V1.UV = FVector2(Glyph.U1, Glyph.V0);
			V2.UV = FVector2(Glyph.U1, Glyph.V1);
			V3.UV = FVector2(Glyph.U0, Glyph.V1);

			V0.Color = V1.Color = V2.Color = V3.Color = FVector4(1, 1, 1, 1);
			V0.Normal = V1.Normal = V2.Normal = V3.Normal = FVector(0, 0, 1);

			OutMesh.Vertices.push_back(V0);
			OutMesh.Vertices.push_back(V1);
			OutMesh.Vertices.push_back(V2);
			OutMesh.Vertices.push_back(V3);

			OutMesh.Indices.push_back(BaseIndex + 0);
			OutMesh.Indices.push_back(BaseIndex + 1);
			OutMesh.Indices.push_back(BaseIndex + 2);
			OutMesh.Indices.push_back(BaseIndex + 0);
			OutMesh.Indices.push_back(BaseIndex + 2);
			OutMesh.Indices.push_back(BaseIndex + 3);
		}

		PenX += ResolveAdvance(Codepoint, Glyph.Advance);
	}

	return !OutMesh.Vertices.empty();
}

TArray<uint32> FTextMeshBuilder::DecodeToCodepoints(const FString& Text) const
{
	TArray<uint32> Result;

	if (Text.empty())
	{
		return Result;
	}

	const int WideLength = MultiByteToWideChar(CP_UTF8, 0, Text.c_str(), -1, nullptr, 0);
	if (WideLength <= 0)
	{
		return Result;
	}

	std::wstring WideText;
	WideText.resize(static_cast<size_t>(WideLength - 1));
	MultiByteToWideChar(CP_UTF8, 0, Text.c_str(), -1, WideText.data(), WideLength);

	Result.reserve(WideText.size());
	for (size_t Index = 0; Index < WideText.size(); ++Index)
	{
		const uint32 W1 = static_cast<uint32>(WideText[Index]);
		if (W1 >= 0xD800 && W1 <= 0xDBFF)
		{
			if (Index + 1 < WideText.size())
			{
				const uint32 W2 = static_cast<uint32>(WideText[Index + 1]);
				if (W2 >= 0xDC00 && W2 <= 0xDFFF)
				{
					const uint32 Codepoint = 0x10000 + (((W1 - 0xD800) << 10) | (W2 - 0xDC00));
					Result.push_back(Codepoint);
					++Index;
					continue;
				}
			}
		}
		Result.push_back(W1);
	}

	return Result;
}
