#include "ObjViewerDetailsWindow.h"

#include "ObjViewerEngine.h"

#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "World/World.h"
#include "imgui.h"

namespace
{
	const char* GetAxisLabel(EObjImportAxis Axis)
	{
		switch (Axis)
		{
		case EObjImportAxis::PosX: return "+X";
		case EObjImportAxis::NegX: return "-X";
		case EObjImportAxis::PosY: return "+Y";
		case EObjImportAxis::NegY: return "-Y";
		case EObjImportAxis::PosZ: return "+Z";
		case EObjImportAxis::NegZ: return "-Z";
		default: return "+X";
		}
	}

	const char* GetPresetLabel(EObjImportPreset Preset)
	{
		switch (Preset)
		{
		case EObjImportPreset::Auto: return "Auto";
		case EObjImportPreset::Custom: return "Custom";
		case EObjImportPreset::Blender: return "Blender";
		case EObjImportPreset::Maya: return "Maya";
		case EObjImportPreset::ThreeDSMax: return "3ds Max";
		case EObjImportPreset::Unreal: return "Unreal";
		case EObjImportPreset::Unity: return "Unity";
		default: return "Auto";
		}
	}

	const char* GetNormalModeLabel(EObjViewerNormalVisualizationMode Mode)
	{
		switch (Mode)
		{
		case EObjViewerNormalVisualizationMode::Vertex: return "Vertex";
		case EObjViewerNormalVisualizationMode::Face: return "Face";
		default: return "Vertex";
		}
	}

	void DrawVector3(const char* Label, const FVector& Value)
	{
		ImGui::Text("%s", Label);
		ImGui::SameLine(120.0f);
		ImGui::Text("(%.2f, %.2f, %.2f)", Value.X, Value.Y, Value.Z);
	}

	void DrawBoolValue(const char* Label, bool bValue)
	{
		ImGui::Text("%s", Label);
		ImGui::SameLine(120.0f);
		ImGui::Text("%s", bValue ? "Yes" : "No");
	}

	void DrawTextValue(const char* Label, const FString& Value)
	{
		ImGui::Text("%s", Label);
		ImGui::SameLine(120.0f);
		ImGui::TextWrapped("%s", Value.c_str());
	}

	FString FormatFileSize(uint64 FileSizeBytes)
	{
		char Buffer[64] = {};

		if (FileSizeBytes >= 1024ull * 1024ull)
		{
			snprintf(Buffer, sizeof(Buffer), "%.2f MB", static_cast<double>(FileSizeBytes) / (1024.0 * 1024.0));
		}
		else if (FileSizeBytes >= 1024ull)
		{
			snprintf(Buffer, sizeof(Buffer), "%.2f KB", static_cast<double>(FileSizeBytes) / 1024.0);
		}
		else
		{
			snprintf(Buffer, sizeof(Buffer), "%llu B", static_cast<unsigned long long>(FileSizeBytes));
		}

		return Buffer;
	}
}

void FObjViewerDetailsWindow::Render(FObjViewerEngine* Engine)
{
	if (Engine == nullptr)
	{
		return;
	}

	if (!ImGui::Begin("Details", nullptr, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::End();
		return;
	}

	if (!Engine->HasLoadedModel())
	{
		ImGui::TextDisabled("No model loaded.");
		ImGui::Spacing();
		ImGui::TextWrapped("Drop an OBJ file into the window or use File > Open OBJ.");
		ImGui::End();
		return;
	}

	const FObjViewerModelState& State = Engine->GetModelState();

	if (ImGui::CollapsingHeader("Source", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawTextValue("File", State.FileName);
		DrawTextValue("Path", State.SourceFilePath);
		DrawTextValue("Size", FormatFileSize(State.FileSizeBytes));
	}

	if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Vertices");
		ImGui::SameLine(120.0f);
		ImGui::Text("%d", State.VertexCount);

		ImGui::Text("Indices");
		ImGui::SameLine(120.0f);
		ImGui::Text("%d", State.IndexCount);

		ImGui::Text("Triangles");
		ImGui::SameLine(120.0f);
		ImGui::Text("%d", State.TriangleCount);

		ImGui::Text("Sections");
		ImGui::SameLine(120.0f);
		ImGui::Text("%d", State.SectionCount);

		DrawBoolValue("UV Present", State.bHasUV);
	}

	if (ImGui::CollapsingHeader("Bounds", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawVector3("Center", State.BoundsCenter);
		DrawVector3("Extent", State.BoundsExtent);
		ImGui::Text("Radius");
		ImGui::SameLine(120.0f);
		ImGui::Text("%.2f", State.BoundsRadius);
	}

	if (ImGui::CollapsingHeader("Import Summary", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawTextValue("Source", State.LastImportSummary.ImportSource);
		DrawTextValue("Preset", GetPresetLabel(State.LastImportSummary.SourcePreset));
		DrawTextValue("Forward Axis", GetAxisLabel(State.LastImportSummary.ForwardAxis));
		DrawTextValue("Up Axis", GetAxisLabel(State.LastImportSummary.UpAxis));
		DrawBoolValue("Replace Current", State.LastImportSummary.bReplaceCurrentModel);
		DrawBoolValue("Center To Origin", State.LastImportSummary.bCenterToOrigin);
		DrawBoolValue("Place On Ground", State.LastImportSummary.bPlaceOnGround);
		DrawBoolValue("Frame Camera", State.LastImportSummary.bFrameCameraAfterImport);

		ImGui::Text("Uniform Scale");
		ImGui::SameLine(120.0f);
		ImGui::Text("%.2f", State.LastImportSummary.UniformScale);
	}

	if (ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool bWireframeEnabled = Engine->IsWireframeEnabled();
		if (ImGui::Checkbox("Show Wireframe", &bWireframeEnabled))
		{
			Engine->SetWireframeEnabled(bWireframeEnabled);
		}

		FObjViewerNormalSettings& NormalSettings = Engine->GetMutableNormalSettings();
		ImGui::Checkbox("Show Normals", &NormalSettings.bVisible);
		if (ImGui::BeginCombo("Normal Mode", GetNormalModeLabel(NormalSettings.Mode)))
		{
			constexpr EObjViewerNormalVisualizationMode Modes[] =
			{
				EObjViewerNormalVisualizationMode::Vertex,
				EObjViewerNormalVisualizationMode::Face
			};

			for (EObjViewerNormalVisualizationMode Candidate : Modes)
			{
				const bool bSelected = Candidate == NormalSettings.Mode;
				if (ImGui::Selectable(GetNormalModeLabel(Candidate), bSelected))
				{
					NormalSettings.Mode = Candidate;
				}

				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
		ImGui::SliderFloat("Normal Length", &NormalSettings.LengthScale, 0.005f, 0.25f, "%.3f");

		FObjViewerGridSettings& GridSettings = Engine->GetMutableGridSettings();
		ImGui::Checkbox("Show Grid", &GridSettings.bVisible);
		ImGui::SliderFloat("Grid Size", &GridSettings.GridSize, 1.0f, 100.0f, "%.1f");
		ImGui::SliderFloat("Line Thickness", &GridSettings.LineThickness, 0.1f, 5.0f, "%.2f");
	}

	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
	{
		UWorld* ActiveWorld = Engine->GetActiveWorld();
		UCameraComponent* ActiveCamera = ActiveWorld ? ActiveWorld->GetActiveCameraComponent() : nullptr;
		FCamera* Camera = ActiveCamera ? ActiveCamera->GetCamera() : nullptr;

		if (Camera)
		{
			DrawVector3("Position", Camera->GetPosition());

			ImGui::Text("Yaw");
			ImGui::SameLine(120.0f);
			ImGui::Text("%.2f", Camera->GetYaw());

			ImGui::Text("Pitch");
			ImGui::SameLine(120.0f);
			ImGui::Text("%.2f", Camera->GetPitch());

			ImGui::Text("FOV");
			ImGui::SameLine(120.0f);
			ImGui::Text("%.2f", Camera->GetFOV());
		}

		if (ImGui::Button("Frame Camera"))
		{
			Engine->FrameLoadedModel();
		}

		ImGui::SameLine();

		if (ImGui::Button("Reset Camera"))
		{
			Engine->ResetViewerCamera();
		}
	}

	ImGui::End();
}
