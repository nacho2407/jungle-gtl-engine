#include "Editor/UI/Asset/Material/MaterialEditorWidget.h"

#include "Engine/Runtime/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Object/Object.h"
#include "Object/GarbageCollection.h"
#include "Platform/Paths.h"
#include "Render/Pipeline/Renderer.h"
#include "Texture/Texture2D.h"
#include "UI/ContentBrowser/ContentItem.h"
#include "UI/Util/EditorFileUtils.h"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace ed = ax::NodeEditor;

namespace
{
	inline ed::NodeId ToNodeId(uint32 Id) { return static_cast<ed::NodeId>(Id); }
	inline ed::PinId  ToPinId(uint32 Id) { return static_cast<ed::PinId>(Id); }
	inline ed::LinkId ToLinkId(uint32 Id) { return static_cast<ed::LinkId>(Id); }

	inline uint32 NodeIdToU32(ed::NodeId Id) { return static_cast<uint32>(Id.Get()); }
	inline uint32 PinIdToU32(ed::PinId Id) { return static_cast<uint32>(Id.Get()); }
	inline uint32 LinkIdToU32(ed::LinkId Id) { return static_cast<uint32>(Id.Get()); }

	enum class EMaterialEditorBlendMode
	{
		Opaque,
		Translucent,
		Additive,
		Custom
	};

	const char* ToString(EMaterialEditorBlendMode Mode)
	{
		switch (Mode)
		{
		case EMaterialEditorBlendMode::Opaque: return "Opaque";
		case EMaterialEditorBlendMode::Translucent: return "Translucent";
		case EMaterialEditorBlendMode::Additive: return "Additive";
		case EMaterialEditorBlendMode::Custom: return "Custom";
		}
		return "Custom";
	}

	EMaterialEditorBlendMode GetMaterialEditorBlendMode(const UMaterial* Material)
	{
		const ERenderPass Pass = Material->GetRenderPass();
		const EBlendState Blend = Material->GetBlendState();
		const EDepthStencilState Depth = Material->GetDepthStencilState();

		if (Pass == ERenderPass::Opaque && Blend == EBlendState::Opaque)
		{
			return EMaterialEditorBlendMode::Opaque;
		}
		if (Pass == ERenderPass::AlphaBlend && Blend == EBlendState::Additive && Depth == EDepthStencilState::DepthReadOnly)
		{
			return EMaterialEditorBlendMode::Additive;
		}
		if (Pass == ERenderPass::AlphaBlend && Blend == EBlendState::AlphaBlend && Depth == EDepthStencilState::DepthReadOnly)
		{
			return EMaterialEditorBlendMode::Translucent;
		}
		return EMaterialEditorBlendMode::Custom;
	}

	void ApplyMaterialEditorBlendMode(UMaterial* Material, EMaterialEditorBlendMode Mode)
	{
		switch (Mode)
		{
		case EMaterialEditorBlendMode::Opaque:
			Material->SetRenderPass(ERenderPass::Opaque);
			Material->SetBlendState(EBlendState::Opaque);
			Material->SetDepthStencilState(EDepthStencilState::Default);
			Material->SetRasterizerState(ERasterizerState::SolidBackCull);
			break;
		case EMaterialEditorBlendMode::Translucent:
			Material->SetRenderPass(ERenderPass::AlphaBlend);
			Material->SetBlendState(EBlendState::AlphaBlend);
			Material->SetDepthStencilState(EDepthStencilState::DepthReadOnly);
			Material->SetRasterizerState(ERasterizerState::SolidNoCull);
			break;
		case EMaterialEditorBlendMode::Additive:
			Material->SetRenderPass(ERenderPass::AlphaBlend);
			Material->SetBlendState(EBlendState::Additive);
			Material->SetDepthStencilState(EDepthStencilState::DepthReadOnly);
			Material->SetRasterizerState(ERasterizerState::SolidNoCull);
			break;
		case EMaterialEditorBlendMode::Custom:
			break;
		}
	}

    void ApplyDefaultBlendModeForDomain(UMaterial* Material, EMaterialDomain Domain)
    {
        if (!Material)
        {
            return;
        }

        switch (Domain)
        {
        case EMaterialDomain::ParticleMesh:
            ApplyMaterialEditorBlendMode(Material, EMaterialEditorBlendMode::Opaque);
            break;
        case EMaterialDomain::ParticleSprite:
        default:
            break;
        }
    }

	ImVec4 NodeColor(EMaterialGraphNodeType Type)
	{
		switch (Type)
		{
		case EMaterialGraphNodeType::Output: return ImVec4(0.95f, 0.45f, 0.45f, 1.0f);
		case EMaterialGraphNodeType::TextureObject:
		case EMaterialGraphNodeType::TextureSample: return ImVec4(0.40f, 0.75f, 1.00f, 1.0f);
		case EMaterialGraphNodeType::ScalarParameter:
		case EMaterialGraphNodeType::VectorParameter:
		case EMaterialGraphNodeType::ColorParameter: return ImVec4(0.95f, 0.85f, 0.40f, 1.0f);
		case EMaterialGraphNodeType::ParticleColor:
		case EMaterialGraphNodeType::VertexColor: return ImVec4(0.60f, 0.95f, 0.65f, 1.0f);
		default: return ImVec4(0.82f, 0.82f, 0.88f, 1.0f);
		}
	}

	ImVec4 PinTypeColor(EMaterialGraphPinType Type)
	{
		switch (Type)
		{
		case EMaterialGraphPinType::Float:  return ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
		case EMaterialGraphPinType::Float2:
		case EMaterialGraphPinType::UV:     return ImVec4(0.50f, 0.95f, 0.50f, 1.0f);
		case EMaterialGraphPinType::Float3: return ImVec4(0.95f, 0.85f, 0.30f, 1.0f);
		case EMaterialGraphPinType::Float4: return ImVec4(0.95f, 0.55f, 0.35f, 1.0f);
		case EMaterialGraphPinType::Color:  return ImVec4(1.00f, 0.45f, 0.85f, 1.0f);
		case EMaterialGraphPinType::Texture2D: return ImVec4(0.40f, 0.75f, 1.00f, 1.0f);
		case EMaterialGraphPinType::Sampler:return ImVec4(0.60f, 0.60f, 0.95f, 1.0f);
		case EMaterialGraphPinType::Bool:   return ImVec4(0.95f, 0.30f, 0.30f, 1.0f);
		}
		return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
	}

	bool InputString(const char* Label, FString& Value, int32 BufferSize = 512)
	{
		TArray<char> Buffer(BufferSize, 0);
		std::snprintf(Buffer.data(), BufferSize, "%s", Value.c_str());
		// 라벨 영역을 남기기 위해 -120. 너무 좁으면 알아서 잘림.
		ImGui::SetNextItemWidth(-120.0f);
		if (ImGui::InputText(Label, Buffer.data(), static_cast<size_t>(BufferSize)))
		{
			Value = Buffer.data();
			return true;
		}
		return false;
	}

	void SyncMaskPinType(FMaterialGraphNode& Node)
	{
		if (Node.Type != EMaterialGraphNodeType::ComponentMask || Node.Pins.size() < 2) return;
		const int32 Count = static_cast<int32>(Node.Mask.size());
		Node.Pins[1].Type =
			Count <= 1 ? EMaterialGraphPinType::Float :
			Count == 2 ? EMaterialGraphPinType::Float2 :
			Count == 3 ? EMaterialGraphPinType::Float3 :
			EMaterialGraphPinType::Float4;
	}

	// 사용자 입력 마스크에서 RGBA 외의 글자를 모두 걸러냄.
	void NormalizeMaskInPlace(FString& Mask)
	{
		FString Out;
		for (char Ch : Mask)
		{
			const char Up = static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
			if (Up == 'R' || Up == 'G' || Up == 'B' || Up == 'A') Out += Up;
		}
		if (Out.empty()) Out = "R";
		Mask = std::move(Out);
	}

	bool IsTextureSlotColor(EMaterialTextureSlot Slot)
	{
		switch (Slot)
		{
		case EMaterialTextureSlot::Diffuse:
		case EMaterialTextureSlot::Emissive:
		case EMaterialTextureSlot::Custom0:
		case EMaterialTextureSlot::Custom1:
			return true;
		default:
			return false;
		}
	}

	// 노드 타입이 현재 머티리얼 도메인에서 사용 가능한지. UE와 동일한 컨텍스트 필터.
	bool IsNodeAllowedForDomain(EMaterialGraphNodeType Type, EMaterialDomain Domain)
	{
		switch (Type)
		{
		// 파티클 인스턴스가 있어야만 의미가 있는 입력
		case EMaterialGraphNodeType::ParticleColor:
		case EMaterialGraphNodeType::ParticleSubUV:
		case EMaterialGraphNodeType::DynamicParameter:
			return Domain == EMaterialDomain::ParticleSprite
			    || Domain == EMaterialDomain::ParticleMesh;

		// 정점에 Color 채널이 있는 도메인만 (sprite는 quad라 정점 컬러 없음)
		case EMaterialGraphNodeType::VertexColor:
			return Domain == EMaterialDomain::Surface
			    || Domain == EMaterialDomain::Decal
			    || Domain == EMaterialDomain::ParticleMesh;

		// 그 외는 모든 도메인에서 사용 가능
		default:
			return true;
		}
	}

	bool ContainsCaseInsensitive(const FString& Haystack, const char* Needle)
	{
		if (!Needle || !*Needle) return true;
		const size_t HN = Haystack.size();
		const size_t NN = std::strlen(Needle);
		if (NN > HN) return false;
		for (size_t i = 0; i + NN <= HN; ++i)
		{
			bool bMatch = true;
			for (size_t j = 0; j < NN; ++j)
			{
				if (std::tolower(static_cast<unsigned char>(Haystack[i + j])) !=
					std::tolower(static_cast<unsigned char>(Needle[j])))
				{
					bMatch = false;
					break;
				}
			}
			if (bMatch) return true;
		}
		return false;
	}

	// 콘텐츠 브라우저에서 PNG 드롭을 받아 ProjectRelative 경로로 반환. 드롭이 없으면 빈 FString.
	FString AcceptPngDragDrop()
	{
		FString Out;
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("PNGElement"))
			{
				FContentItem Item = *reinterpret_cast<const FContentItem*>(Payload->Data);
				Out = FPaths::ToUtf8(
					Item.Path.lexically_relative(FPaths::RootDir()).generic_wstring());
			}
			ImGui::EndDragDropTarget();
		}
		return Out;
	}
}

FMaterialEditorWidget::~FMaterialEditorWidget()
{
	DestroyContext();
	TexturePreviewCache.clear();
}

bool FMaterialEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UMaterial>();
}

void FMaterialEditorWidget::Open(UObject* Object)
{
	if (!CanEdit(Object)) return;

	FAssetEditorWidget::Open(Object);
	EnsureContext();
	bPositionsPushed = false;
	LastCompileError.clear();

	UMaterial* Material = static_cast<UMaterial*>(Object);
	if (!Material->GetGraph().HasOutputNode())
	{
		Material->GetGraph().InitializeDefault(Material->GetDomain());
	}
}

void FMaterialEditorWidget::Close()
{
	DestroyContext();
	TexturePreviewCache.clear();
	FAssetEditorWidget::Close();
}

void FMaterialEditorWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
    FAssetEditorWidget::AddReferencedObjects(Collector);

    for (auto& Pair : TexturePreviewCache)
    {
        Collector.AddReferencedObject(Pair.second);
    }
}


void FMaterialEditorWidget::EnsureContext()
{
	if (NodeEditorContext) return;

	ed::Config Cfg;
	Cfg.SettingsFile = nullptr;
	NodeEditorContext = ed::CreateEditor(&Cfg);
}

void FMaterialEditorWidget::DestroyContext()
{
	if (NodeEditorContext)
	{
		ed::DestroyEditor(NodeEditorContext);
		NodeEditorContext = nullptr;
	}
}

void FMaterialEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!IsOpen() || !EditedObject || !NodeEditorContext) return;

	UMaterial* Material = static_cast<UMaterial*>(EditedObject);
	char WindowTitle[160];
	std::snprintf(WindowTitle, sizeof(WindowTitle), "Material Editor - %s##%p",
		Material->GetAssetPathFileName().c_str(), static_cast<void*>(Material));

	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	bool bOpenFlag = true;
	ImGui::SetNextWindowSize(ImVec2(1180.0f, 780.0f), ImGuiCond_Once);
	if (!ImGui::Begin(WindowTitle, &bOpenFlag))
	{
		ImGui::End();
		if (!bOpenFlag) Close();
		return;
	}

	RenderToolbar(Material);
	RenderErrorPanel();
	ImGui::Separator();

	constexpr float InspectorWidth = 380.0f;
	const float Spacing = ImGui::GetStyle().ItemSpacing.x;
	const float TotalWidth = ImGui::GetContentRegionAvail().x;
	const float CanvasWidth = (TotalWidth > InspectorWidth + Spacing + 100.0f)
		? TotalWidth - InspectorWidth - Spacing
		: TotalWidth;

	uint32 SelectedNodeId = 0;
	RenderGraphCanvas(Material, CanvasWidth, SelectedNodeId);

	if (CanvasWidth < TotalWidth)
	{
		ImGui::SameLine();
		RenderInspector(Material, SelectedNodeId);
	}

	ImGui::End();
	if (!bOpenFlag) Close();
}

void FMaterialEditorWidget::RenderToolbar(UMaterial* Material)
{
	const bool bDirtyNow = IsDirty();
	if (bDirtyNow) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.55f, 0.25f, 1.0f));
	if (ImGui::Button(bDirtyNow ? "Save*" : "Save"))
	{
		CompileAndSave(Material);
	}
	if (bDirtyNow) ImGui::PopStyleColor();

	ImGui::SameLine();
	if (ImGui::Button("Compile"))
	{
		CompileOnly(Material);
	}

	ImGui::SameLine();
	EMaterialDomain Domain = Material->GetDomain();
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::BeginCombo("Domain", ToString(Domain)))
	{
		for (int32 i = 0; i <= static_cast<int32>(EMaterialDomain::PostProcess); ++i)
		{
			const EMaterialDomain Candidate = static_cast<EMaterialDomain>(i);
			const bool bSelected = (Domain == Candidate);
            if (ImGui::Selectable(ToString(Candidate), bSelected))
            {
                const EMaterialDomain PreviousDomain = Material->GetDomain();

                Material->SetDomain(Candidate);
                RebuildOutputPinsForDomain(Material);

                if (PreviousDomain != Candidate)
                {
                    ApplyDefaultBlendModeForDomain(Material, Candidate);
                }

                MarkDirty();
            }
			if (bSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	EMaterialEditorBlendMode BlendMode = GetMaterialEditorBlendMode(Material);
	ImGui::SetNextItemWidth(130.0f);
	if (ImGui::BeginCombo("Mode", ToString(BlendMode)))
	{
		const EMaterialEditorBlendMode Modes[] =
		{
			EMaterialEditorBlendMode::Opaque,
			EMaterialEditorBlendMode::Translucent,
			EMaterialEditorBlendMode::Additive
		};
		for (EMaterialEditorBlendMode Candidate : Modes)
		{
			const bool bSelected = (BlendMode == Candidate);
			if (ImGui::Selectable(ToString(Candidate), bSelected))
			{
				ApplyMaterialEditorBlendMode(Material, Candidate);
				MarkDirty();
			}
			if (bSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("Presets"))
	{
		ImGui::OpenPopup("MaterialPresetsPopup");
	}
	if (ImGui::BeginPopup("MaterialPresetsPopup"))
	{
		ImGui::TextDisabled("Replace current graph with:");
		ImGui::Separator();
        if (ImGui::MenuItem("Particle Color only (default)"))
        {
            Material->GetGraph().InitializeDefault(Material->GetDomain());
            ApplyDefaultBlendModeForDomain(Material, Material->GetDomain());

            bPositionsPushed = false;
            MarkDirty();
        }

        if (ImGui::MenuItem("Textured Particle (Tex × ParticleColor)"))
        {
            Material->GetGraph().ApplyTexturedParticlePreset(Material->GetDomain());
            ApplyDefaultBlendModeForDomain(Material, Material->GetDomain());

            bPositionsPushed = false;
            MarkDirty();
        }
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	ImGui::TextDisabled("Shader: %s", Material->GetGeneratedShaderPath().empty()
		? "(not generated)"
		: Material->GetGeneratedShaderPath().c_str());
}

void FMaterialEditorWidget::RenderErrorPanel()
{
	if (LastCompileError.empty()) return;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.25f, 0.10f, 0.10f, 0.6f));
	ImGui::BeginChild("##MaterialErrorPanel", ImVec2(0, 60), ImGuiChildFlags_Borders);
	ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Compile error:");
	ImGui::TextWrapped("%s", LastCompileError.c_str());
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void FMaterialEditorWidget::RenderNodeBody(FMaterialGraphNode& Node)
{
	switch (Node.Type)
	{
	case EMaterialGraphNodeType::TextureObject:
	{
		ImGui::TextDisabled("Slot: %s", ToString(Node.TextureSlot));
		UTexture2D* Preview = !Node.TexturePath.empty()
			? GetOrLoadTexture(Node.TexturePath, IsTextureSlotColor(Node.TextureSlot))
			: nullptr;

		const ImVec2 PreviewSize(96, 96);
		if (Preview && Preview->GetSRV())
		{
			ImGui::Image(Preview->GetSRV(), PreviewSize);
		}
		else
		{
			ImGui::Dummy(PreviewSize);
			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Max = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRect(Min, Max, IM_COL32(80, 80, 80, 255));
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(Min.x + 6, Min.y + 38),
				IM_COL32(160, 160, 160, 255),
				"(drop here)");
		}

		// 노드 안에서 콘텐츠 브라우저의 PNG 드롭을 직접 받음.
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* P = ImGui::AcceptDragDropPayload("PNGElement"))
			{
				FContentItem Item = *reinterpret_cast<const FContentItem*>(P->Data);
				const FString Rel = FPaths::ToUtf8(
					Item.Path.lexically_relative(FPaths::RootDir()).generic_wstring());
				HandleTextureDropOnNode(Node, Rel);
				MarkDirty();
			}
			ImGui::EndDragDropTarget();
		}
		break;
	}
	case EMaterialGraphNodeType::ColorParameter:
	{
		ImVec4 Col(Node.Value.X, Node.Value.Y, Node.Value.Z, Node.Value.W);
		ImGui::ColorButton("##swatch", Col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_AlphaPreview, ImVec2(96, 24));
		if (!Node.ParameterName.empty()) ImGui::TextDisabled("%s", Node.ParameterName.c_str());
		break;
	}
	case EMaterialGraphNodeType::VectorParameter:
	{
		if (!Node.ParameterName.empty()) ImGui::TextDisabled("%s", Node.ParameterName.c_str());
		ImGui::Text("(%.2f, %.2f, %.2f, %.2f)", Node.Value.X, Node.Value.Y, Node.Value.Z, Node.Value.W);
		break;
	}
	case EMaterialGraphNodeType::ScalarParameter:
	{
		if (!Node.ParameterName.empty()) ImGui::TextDisabled("%s", Node.ParameterName.c_str());
		ImGui::Text("%.3f", Node.Value.X);
		break;
	}
	case EMaterialGraphNodeType::ConstantFloat:
		ImGui::Text("%.3f", Node.Value.X);
		break;
	case EMaterialGraphNodeType::ConstantFloat2:
		ImGui::Text("(%.2f, %.2f)", Node.Value.X, Node.Value.Y);
		break;
	case EMaterialGraphNodeType::ConstantFloat3:
		ImGui::Text("(%.2f, %.2f, %.2f)", Node.Value.X, Node.Value.Y, Node.Value.Z);
		break;
	case EMaterialGraphNodeType::ConstantFloat4:
		ImGui::Text("(%.2f, %.2f, %.2f, %.2f)", Node.Value.X, Node.Value.Y, Node.Value.Z, Node.Value.W);
		break;
	case EMaterialGraphNodeType::ComponentMask:
		ImGui::Text(".%s", Node.Mask.c_str());
		break;
	case EMaterialGraphNodeType::Panner:
		ImGui::TextDisabled("Speed (%.2f, %.2f)", Node.Value.X, Node.Value.Y);
		break;
	case EMaterialGraphNodeType::TexCoord:
		ImGui::TextDisabled("UV[%d]", static_cast<int32>(Node.Value.X));
		break;
	case EMaterialGraphNodeType::ConstantBiasScale:
		ImGui::TextDisabled("B:%.2f  S:%.2f", Node.Value.X, Node.Value.Y);
		break;
	case EMaterialGraphNodeType::ParticleSubUV:
		ImGui::TextDisabled("%dx%d", static_cast<int32>(Node.Value.X), static_cast<int32>(Node.Value.Y));
		break;
	default:
		break;
	}
}

void FMaterialEditorWidget::RenderAddNodeMenu(FMaterialGraph& Graph, EMaterialDomain Domain)
{
	ImGui::TextDisabled("Add Node");
	ImGui::Separator();
	ImGui::SetNextItemWidth(220.0f);
	ImGui::InputTextWithHint("##Search", "search...", AddNodeSearchBuf, sizeof(AddNodeSearchBuf));
	const char* Query = AddNodeSearchBuf;

	auto AddItem = [&](EMaterialGraphNodeType Type)
	{
		// 도메인에 맞지 않는 노드는 팔레트에서 아예 숨김 (UE 컨벤션과 동일).
		if (!IsNodeAllowedForDomain(Type, Domain)) return;
		if (!ContainsCaseInsensitive(ToString(Type), Query)) return;
		const bool bDisabled = (Type == EMaterialGraphNodeType::Output) && Graph.HasOutputNode();
		if (bDisabled) ImGui::BeginDisabled();
		if (ImGui::MenuItem(ToString(Type)))
		{
			FMaterialGraphNode* NewNode = Graph.AddNodeOfType(Type, PendingNewNodePosition.x, PendingNewNodePosition.y, Domain);
			if (NewNode)
			{
				ed::SetNodePosition(ToNodeId(NewNode->NodeId), PendingNewNodePosition);
				MarkDirty();
			}
		}
		if (bDisabled) ImGui::EndDisabled();
	};

	const bool bHasQuery = Query[0] != 0;
	if (bHasQuery)
	{
		// 검색 모드: 카테고리 펼치지 않고 한 줄로.
		for (int32 i = 0; i <= static_cast<int32>(EMaterialGraphNodeType::ComponentMask); ++i)
		{
			AddItem(static_cast<EMaterialGraphNodeType>(i));
		}
	}
	else
	{
		if (ImGui::BeginMenu("Parameters"))
		{
			AddItem(EMaterialGraphNodeType::ScalarParameter);
			AddItem(EMaterialGraphNodeType::VectorParameter);
			AddItem(EMaterialGraphNodeType::ColorParameter);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Constants"))
		{
			AddItem(EMaterialGraphNodeType::ConstantFloat);
			AddItem(EMaterialGraphNodeType::ConstantFloat2);
			AddItem(EMaterialGraphNodeType::ConstantFloat3);
			AddItem(EMaterialGraphNodeType::ConstantFloat4);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Texture"))
		{
			AddItem(EMaterialGraphNodeType::TextureObject);
			AddItem(EMaterialGraphNodeType::TextureSample);
			AddItem(EMaterialGraphNodeType::TexCoord);
			AddItem(EMaterialGraphNodeType::Panner);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Math"))
		{
			AddItem(EMaterialGraphNodeType::Add);
			AddItem(EMaterialGraphNodeType::Subtract);
			AddItem(EMaterialGraphNodeType::Multiply);
			AddItem(EMaterialGraphNodeType::Divide);
			AddItem(EMaterialGraphNodeType::Lerp);
			AddItem(EMaterialGraphNodeType::OneMinus);
			AddItem(EMaterialGraphNodeType::Saturate);
			AddItem(EMaterialGraphNodeType::Clamp);
			AddItem(EMaterialGraphNodeType::Power);
			AddItem(EMaterialGraphNodeType::ConstantBiasScale);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Vector"))
		{
			AddItem(EMaterialGraphNodeType::Append);
			AddItem(EMaterialGraphNodeType::ComponentMask);
			AddItem(EMaterialGraphNodeType::Distance);
			AddItem(EMaterialGraphNodeType::Normalize);
			AddItem(EMaterialGraphNodeType::Dot);
			AddItem(EMaterialGraphNodeType::Cross);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Inputs"))
		{
			AddItem(EMaterialGraphNodeType::Time);
			AddItem(EMaterialGraphNodeType::ParticleColor);
			AddItem(EMaterialGraphNodeType::VertexColor);
			AddItem(EMaterialGraphNodeType::ParticleSubUV);
			AddItem(EMaterialGraphNodeType::DynamicParameter);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		AddItem(EMaterialGraphNodeType::Output);
	}
}

void FMaterialEditorWidget::RenderGraphCanvas(UMaterial* Material, float Width, uint32& OutSelectedNodeId)
{
	FMaterialGraph& Graph = Material->GetGraph();

	ImGui::BeginChild("##MaterialGraphCanvasChild", ImVec2(Width, 0), ImGuiChildFlags_None);

	ed::SetCurrentEditor(NodeEditorContext);
	ed::Begin("MaterialGraphCanvas");

	if (!bPositionsPushed)
	{
		for (const FMaterialGraphNode& Node : Graph.Nodes)
		{
			ed::SetNodePosition(ToNodeId(Node.NodeId), ImVec2(Node.PosX, Node.PosY));
		}
		bPositionsPushed = true;
	}

	for (FMaterialGraphNode& Node : Graph.Nodes)
	{
		ed::BeginNode(ToNodeId(Node.NodeId));
		ImGui::TextColored(NodeColor(Node.Type), "%s", Node.DisplayName.ToString().c_str());
		// ImGui::Separator()는 노드 내부에서 너비를 노드 콘텐츠에 맞춰주지 않아 노드 밖으로 검은 선이 늘어남.
		// 가벼운 간격만 줌.
		ImGui::Dummy(ImVec2(0.0f, 2.0f));

		// 노드 본문: 색상 스와치 / 텍스처 썸네일 / 현재 값 등.
		RenderNodeBody(Node);

		for (const FMaterialGraphPin& Pin : Node.Pins)
		{
			ed::BeginPin(ToPinId(Pin.PinId), Pin.Kind == EMaterialGraphPinKind::Input
				? ed::PinKind::Input
				: ed::PinKind::Output);

			const ImVec4 PinCol = PinTypeColor(Pin.Type);
			if (Pin.Kind == EMaterialGraphPinKind::Input)
				ImGui::TextColored(PinCol, "-> %s", Pin.DisplayName.ToString().c_str());
			else
				ImGui::TextColored(PinCol, "%s ->", Pin.DisplayName.ToString().c_str());

			ed::EndPin();
		}
		// 마지막 pin 텍스트가 노드 하단에 잘려보이는 현상 방지용 패딩.
		ImGui::Dummy(ImVec2(0.0f, 2.0f));
		ed::EndNode();
	}

	for (const FMaterialGraphLink& Link : Graph.Links)
	{
		ed::Link(ToLinkId(Link.LinkId), ToPinId(Link.FromPinId), ToPinId(Link.ToPinId));
	}

	if (ed::BeginCreate())
	{
		ed::PinId StartId, EndId;
		if (ed::QueryNewLink(&StartId, &EndId) && StartId && EndId)
		{
			uint32 FromU = 0, ToU = 0;
			if (Graph.CanLinkPins(PinIdToU32(StartId), PinIdToU32(EndId), &FromU, &ToU))
			{
				if (ed::AcceptNewItem())
				{
					Graph.AddLink(FromU, ToU);
					MarkDirty();
				}
			}
			else
			{
				ed::RejectNewItem(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 2.0f);
			}
		}
	}
	ed::EndCreate();

	if (ed::BeginDelete())
	{
		ed::LinkId DeletedLink;
		while (ed::QueryDeletedLink(&DeletedLink))
		{
			if (ed::AcceptDeletedItem())
			{
				Graph.RemoveLink(LinkIdToU32(DeletedLink));
				MarkDirty();
			}
		}

		ed::NodeId DeletedNode;
		while (ed::QueryDeletedNode(&DeletedNode))
		{
			if (ed::AcceptDeletedItem())
			{
				Graph.RemoveNode(NodeIdToU32(DeletedNode));
				MarkDirty();
			}
		}
	}
	ed::EndDelete();

	for (FMaterialGraphNode& Node : Graph.Nodes)
	{
		const ImVec2 P = ed::GetNodePosition(ToNodeId(Node.NodeId));
		Node.PosX = P.x;
		Node.PosY = P.y;
	}

	ed::NodeId ContextNodeId = 0;
	ed::PinId ContextPinId = 0;
	ed::LinkId ContextLinkId = 0;

	ed::Suspend();
	if (ed::ShowNodeContextMenu(&ContextNodeId))
	{
		ImGui::OpenPopup("MaterialGraphNodeMenu");
	}
	else if (ed::ShowPinContextMenu(&ContextPinId))
	{
		ImGui::OpenPopup("MaterialGraphPinMenu");
	}
	else if (ed::ShowLinkContextMenu(&ContextLinkId))
	{
		ImGui::OpenPopup("MaterialGraphLinkMenu");
	}
	else if (ed::ShowBackgroundContextMenu())
	{
		PendingNewNodePosition = ed::ScreenToCanvas(ImGui::GetMousePos());
		AddNodeSearchBuf[0] = 0;
		ImGui::OpenPopup("MaterialGraphBackgroundMenu");
	}

	if (ImGui::BeginPopup("MaterialGraphNodeMenu"))
	{
		if (ImGui::MenuItem("Delete"))
		{
			Graph.RemoveNode(NodeIdToU32(ContextNodeId));
			MarkDirty();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("MaterialGraphLinkMenu"))
	{
		if (ImGui::MenuItem("Delete"))
		{
			Graph.RemoveLink(LinkIdToU32(ContextLinkId));
			MarkDirty();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("MaterialGraphPinMenu"))
	{
		ImGui::TextDisabled("(no actions)");
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("MaterialGraphBackgroundMenu"))
	{
		RenderAddNodeMenu(Graph, Material->GetDomain());
		ImGui::EndPopup();
	}
	ed::Resume();

	ed::NodeId SelBuf[4];
	const int SelCount = ed::GetSelectedNodes(SelBuf, 4);
	if (SelCount > 0)
	{
		OutSelectedNodeId = NodeIdToU32(SelBuf[0]);
	}

	ed::End();
	ed::SetCurrentEditor(nullptr);
	ImGui::EndChild();
}

void FMaterialEditorWidget::RenderInspector(UMaterial* Material, uint32 SelectedNodeId)
{
	ImGui::BeginChild("##MaterialInspector", ImVec2(0, 0), ImGuiChildFlags_Borders);

	FMaterialGraphNode* Node = Material->GetGraph().FindNode(SelectedNodeId);
	if (!Node)
	{
		ImGui::TextDisabled("Select a node to edit properties.");
		ImGui::EndChild();
		return;
	}

	ImGui::TextColored(NodeColor(Node->Type), "%s", Node->DisplayName.ToString().c_str());
	ImGui::TextDisabled("id=%u  type=%s", Node->NodeId, ToString(Node->Type));
	ImGui::Separator();

	bool bChanged = false;
	switch (Node->Type)
	{
	case EMaterialGraphNodeType::TextureObject:
	{
		bChanged |= InputString("Name", Node->ParameterName);

		const bool bSRGB = IsTextureSlotColor(Node->TextureSlot);
		UTexture2D* Preview = !Node->TexturePath.empty()
			? GetOrLoadTexture(Node->TexturePath, bSRGB)
			: nullptr;

		if (Preview && Preview->GetSRV())
		{
			ImGui::Image(Preview->GetSRV(), ImVec2(128, 128));
		}
		else
		{
			ImGui::Dummy(ImVec2(128, 128));
			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Max = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRect(Min, Max, IM_COL32(120, 120, 120, 255));
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(Min.x + 12, Min.y + 56),
				IM_COL32(180, 180, 180, 255),
				"Drop / browse");
		}
		// 콘텐츠 브라우저에서 PNG 드롭.
		const FString Dropped = AcceptPngDragDrop();
		if (!Dropped.empty())
		{
			HandleTextureDropOnNode(*Node, Dropped);
			bChanged = true;
		}

		// 파일 브라우저 — 직접 텍스트 입력 대신.
		if (ImGui::Button("Browse..."))
		{
			FEditorFileDialogOptions Opts;
			Opts.Title = L"Select Texture";
			// WIC + DDS + TGA 모두 처리 가능 (loader가 분기함).
			Opts.Filter =
				L"Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.tif;*.tiff;*.gif\0"
				L"PNG (*.png)\0*.png\0"
				L"JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0"
				L"DDS (*.dds)\0*.dds\0"
				L"TGA (*.tga)\0*.tga\0"
				L"All Files (*.*)\0*.*\0";
			Opts.bReturnRelativeToProjectRoot = true;
			const FString Picked = FEditorFileUtils::OpenFileDialog(Opts);
			if (!Picked.empty())
			{
				Node->TexturePath = Picked;
				bChanged = true;
			}
		}
		ImGui::SameLine();
		if (!Node->TexturePath.empty())
		{
			if (ImGui::SmallButton("Clear"))
			{
				Node->TexturePath.clear();
				bChanged = true;
			}
			ImGui::TextWrapped("%s", Node->TexturePath.c_str());
		}
		else
		{
			ImGui::TextDisabled("(no texture)");
		}

		if (ImGui::BeginCombo("Slot", ToString(Node->TextureSlot)))
		{
			for (int32 i = 0; i < static_cast<int32>(EMaterialTextureSlot::Max); ++i)
			{
				const auto Slot = static_cast<EMaterialTextureSlot>(i);
				const bool bSel = Node->TextureSlot == Slot;
				if (ImGui::Selectable(ToString(Slot), bSel))
				{
					Node->TextureSlot = Slot;
					bChanged = true;
				}
				if (bSel) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		break;
	}
	case EMaterialGraphNodeType::ScalarParameter:
		bChanged |= InputString("Name", Node->ParameterName);
		bChanged |= ImGui::DragFloat("Default", &Node->Value.X, 0.01f);
		break;
	case EMaterialGraphNodeType::VectorParameter:
	case EMaterialGraphNodeType::ConstantFloat4:
		if (Node->Type == EMaterialGraphNodeType::VectorParameter)
			bChanged |= InputString("Name", Node->ParameterName);
		bChanged |= ImGui::DragFloat4("Value", Node->Value.Data, 0.01f);
		break;
	case EMaterialGraphNodeType::ColorParameter:
		bChanged |= InputString("Name", Node->ParameterName);
		bChanged |= ImGui::ColorEdit4("Color", Node->Value.Data);
		break;
	case EMaterialGraphNodeType::ConstantFloat:
		bChanged |= ImGui::DragFloat("Value", &Node->Value.X, 0.01f);
		break;
	case EMaterialGraphNodeType::ConstantFloat2:
		bChanged |= ImGui::DragFloat2("Value", Node->Value.Data, 0.01f);
		break;
	case EMaterialGraphNodeType::ConstantFloat3:
		bChanged |= ImGui::DragFloat3("Value", Node->Value.Data, 0.01f);
		break;
	case EMaterialGraphNodeType::Panner:
		bChanged |= ImGui::DragFloat2("Speed", Node->Value.Data, 0.01f);
		break;
	case EMaterialGraphNodeType::TexCoord:
	{
		int32 Idx = static_cast<int32>(Node->Value.X);
		if (ImGui::DragInt("UV Channel", &Idx, 0.05f, 0, 2))
		{
			Node->Value.X = static_cast<float>(Idx);
			bChanged = true;
		}
		ImGui::TextDisabled("0/1/2 — 정점 타입에 따라 UV1/UV2는 0이 들어올 수 있음");
		break;
	}
	case EMaterialGraphNodeType::ConstantBiasScale:
		bChanged |= ImGui::DragFloat("Bias",  &Node->Value.X, 0.01f);
		bChanged |= ImGui::DragFloat("Scale", &Node->Value.Y, 0.01f);
		ImGui::TextDisabled("Result = (V + Bias) * Scale");
		break;
	case EMaterialGraphNodeType::ParticleSubUV:
	{
		int32 Cols = static_cast<int32>(Node->Value.X);
		int32 Rows = static_cast<int32>(Node->Value.Y);
		if (ImGui::DragInt("Cols", &Cols, 0.1f, 1, 64)) { Node->Value.X = static_cast<float>(Cols); bChanged = true; }
		if (ImGui::DragInt("Rows", &Rows, 0.1f, 1, 64)) { Node->Value.Y = static_cast<float>(Rows); bChanged = true; }
		ImGui::TextDisabled("Atlas %dx%d = %d frames. SubImageIndex = RelativeTime.", Cols, Rows, Cols * Rows);
		break;
	}
	case EMaterialGraphNodeType::ComponentMask:
		if (InputString("Mask", Node->Mask, 16))
		{
			NormalizeMaskInPlace(Node->Mask);
			SyncMaskPinType(*Node);
			bChanged = true;
		}
		ImGui::TextDisabled("Allowed letters: R G B A");
		break;
	default:
		ImGui::TextDisabled("(no editable properties)");
		break;
	}

	if (bChanged)
	{
		MarkDirty();
	}

	ImGui::Separator();
	ImGui::TextDisabled("Inputs / Outputs");
	for (const FMaterialGraphPin& Pin : Node->Pins)
	{
		ImGui::BulletText("%s %s : %s",
			Pin.Kind == EMaterialGraphPinKind::Input ? "In " : "Out",
			Pin.DisplayName.ToString().c_str(),
			ToString(Pin.Type));
	}

	ImGui::EndChild();
}

void FMaterialEditorWidget::CompileAndSave(UMaterial* Material)
{
	LastCompileError.clear();
	if (!FMaterialManager::Get().SaveMaterialAsset(Material))
	{
		LastCompileError = "Save / compile failed. See log for details.";
	}
	else
	{
		ClearDirty();
		// Reload로 graph 객체가 교체됐을 수 있으므로 위치 다시 푸시.
		bPositionsPushed = false;
		TexturePreviewCache.clear();
	}
}

void FMaterialEditorWidget::CompileOnly(UMaterial* Material)
{
	// SaveMaterialAsset이 컴파일+저장을 함께 수행. 둘을 분리하려면 별도 API가 필요해
	// 현재 빌드에서는 동일 경로를 사용.
	CompileAndSave(Material);
}

void FMaterialEditorWidget::RebuildOutputPinsForDomain(UMaterial* Material)
{
	if (!Material) return;
	Material->GetGraph().RebuildOutputPinsForDomain(Material->GetDomain());
	bPositionsPushed = false;
	MarkDirty();
}

UTexture2D* FMaterialEditorWidget::GetOrLoadTexture(const FString& Path, bool bSRGB)
{
	if (Path.empty()) return nullptr;

	auto It = TexturePreviewCache.find(Path);
	if (It != TexturePreviewCache.end()) return It->second;

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device) return nullptr;

	UTexture2D* Tex = UTexture2D::LoadFromFile(
		Path,
		Device,
		bSRGB ? ETextureColorSpace::SRGB : ETextureColorSpace::Linear);
	TexturePreviewCache[Path] = Tex;
	return Tex;
}

void FMaterialEditorWidget::HandleTextureDropOnNode(FMaterialGraphNode& Node, const FString& ProjectRelativePath)
{
	if (Node.Type != EMaterialGraphNodeType::TextureObject) return;
	Node.TexturePath = ProjectRelativePath;
	// 다음 GetOrLoadTexture 호출에서 새로 로드되도록 캐시는 그대로 둠 — 경로가 키이므로 충돌 없음.
}
