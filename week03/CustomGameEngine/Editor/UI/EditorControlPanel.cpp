#include "EditorControlPanel.h"

#include "Component/BillboardSubUVComponent.h"
#include "Component/TextMeshComponent.h"
#include "EngineCore/Logger.h"
#include "EngineCore/ResourceManager.h"

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>

static EPrimitiveType primitiveType = EPrimitiveType::Triangle;
static int spawnCount = 0;
static char sceneName[128] = "Default";
static bool orthogonal = false;
static float fov = 45.0f;
static FVector cameraLocation = { 0.0f, 0.0f, 0.0f };
static FVector cameraRotation = { 0.0f, 0.0f, 0.0f };

static float frameCount = 0;
static float elapsedTime = 0.0f;
static float displayFPS = 0.0f;
const float fpsUpdateInterval = 0.5f; // 초 단위

EditorControlPanel::EditorControlPanel(Editor* parent, URenderer* InRenderer) : editor(parent), Renderer(InRenderer)
{
}

EditorControlPanel::~EditorControlPanel()
{
}

namespace
{
	static FVector GetRandomSpawnLocation()
	{
		return FVector(
			-10.0f + static_cast<float>(std::rand() % 21),
			-10.0f + static_cast<float>(std::rand() % 21),
			static_cast<float>(std::rand() % 11));
	}

	static float GetRandomSubUVPlaybackRate()
	{
		return 4.0f + static_cast<float>(std::rand() % 13);
	}

	static TArray<FString> GetSceneFileList(const FString& sceneDir)
	{
		TArray<FString> SceneList;

		namespace fs = std::filesystem;

		if (!fs::exists(sceneDir) || !fs::is_directory(sceneDir))
		{
			return SceneList;
		}

		for (const auto& Entry : fs::directory_iterator(sceneDir))
		{
			if (!Entry.is_regular_file())
			{
				continue;
			}

			const fs::path& Path = Entry.path();

			const FString extension = Path.extension().string();
			if (extension == ".Scene")
			{
				SceneList.push_back(Path.stem().string());
			}
		}

		std::sort(SceneList.begin(), SceneList.end());
		return SceneList;
	}

	static bool DumpGlyphAtlasToBMP(const FString& FilePath)
	{
		const FGlyphAtlas& Atlas = ResourceManager::GetInstance()->GetBillboardFont().GetAtlas();
		const uint32 Width = Atlas.GetWidth();
		const uint32 Height = Atlas.GetHeight();
		const TArray<uint8>& Pixels = Atlas.GetPixels();

		if (Width == 0 || Height == 0 || Pixels.size() < static_cast<size_t>(Width) * Height)
		{
			return false;
		}

		std::ofstream File(FilePath, std::ios::binary);
		if (File.is_open() == false)
		{
			return false;
		}

		const uint32 BytesPerPixel = 3;
		const uint32 RowStride = Width * BytesPerPixel;
		const uint32 RowPadding = (4 - (RowStride % 4)) % 4;
		const uint32 PaddedRowStride = RowStride + RowPadding;
		const uint32 PixelDataSize = PaddedRowStride * Height;
		const uint32 FileSize = 14 + 40 + PixelDataSize;

		uint8 FileHeader[14] =
		{
			'B', 'M',
			static_cast<uint8>(FileSize & 0xFF),
			static_cast<uint8>((FileSize >> 8) & 0xFF),
			static_cast<uint8>((FileSize >> 16) & 0xFF),
			static_cast<uint8>((FileSize >> 24) & 0xFF),
			0, 0, 0, 0,
			54, 0, 0, 0
		};

		uint8 InfoHeader[40] = {};
		InfoHeader[0] = 40;
		InfoHeader[4] = static_cast<uint8>(Width & 0xFF);
		InfoHeader[5] = static_cast<uint8>((Width >> 8) & 0xFF);
		InfoHeader[6] = static_cast<uint8>((Width >> 16) & 0xFF);
		InfoHeader[7] = static_cast<uint8>((Width >> 24) & 0xFF);
		InfoHeader[8] = static_cast<uint8>(Height & 0xFF);
		InfoHeader[9] = static_cast<uint8>((Height >> 8) & 0xFF);
		InfoHeader[10] = static_cast<uint8>((Height >> 16) & 0xFF);
		InfoHeader[11] = static_cast<uint8>((Height >> 24) & 0xFF);
		InfoHeader[12] = 1;
		InfoHeader[14] = 24;
		InfoHeader[20] = static_cast<uint8>(PixelDataSize & 0xFF);
		InfoHeader[21] = static_cast<uint8>((PixelDataSize >> 8) & 0xFF);
		InfoHeader[22] = static_cast<uint8>((PixelDataSize >> 16) & 0xFF);
		InfoHeader[23] = static_cast<uint8>((PixelDataSize >> 24) & 0xFF);

		File.write(reinterpret_cast<const char*>(FileHeader), sizeof(FileHeader));
		File.write(reinterpret_cast<const char*>(InfoHeader), sizeof(InfoHeader));

		TArray<uint8> RowBuffer(PaddedRowStride, 0);
		for (int32 Y = static_cast<int32>(Height) - 1; Y >= 0; --Y)
		{
			for (uint32 X = 0; X < Width; ++X)
			{
				const uint8 Gray = Pixels[static_cast<size_t>(Y) * Width + X];
				const uint32 RowIndex = X * BytesPerPixel;
				RowBuffer[RowIndex + 0] = Gray;
				RowBuffer[RowIndex + 1] = Gray;
				RowBuffer[RowIndex + 2] = Gray;
			}

			File.write(reinterpret_cast<const char*>(RowBuffer.data()), static_cast<std::streamsize>(RowBuffer.size()));
		}

		return File.good();
	}
} // namespace

void UpdateFPS(float deltaTime)
{
	frameCount++;
	elapsedTime += deltaTime;

	if (elapsedTime >= fpsUpdateInterval)
	{
		displayFPS = frameCount / elapsedTime;

		frameCount = 0;
		elapsedTime = 0.0f;
	}
}

void EditorControlPanel::Draw(UCameraComponent* camera, UGridComp* Grid)
{
	static bool bSeeded = false;
	if (!bSeeded)
	{
		std::srand(static_cast<unsigned int>(std::time(nullptr)));
		bSeeded = true;
	}

	UpdateFPS(TimerManager::GetInstance().GetGlobalDeltaTime());

	ImGui::Begin("Jungle Control Panel");
	ImGui::Text("Hello Jungle World!");
	ImGui::Text("FPS %.0f (%d ms)",
		displayFPS,
		static_cast<int>(TimerManager::GetInstance().GetGlobalDeltaTime() * 1000));
	ImGui::Text("DrawCall Count: %d", Renderer->DrawCallCount);
	ImGui::Separator();

	if (ImGui::Combo("Primitive", (int*)&primitiveType, "Sphere\0Cube\0Triangle\0Plane\0SubUV\0TextMesh\0"))
	{
		// TODO: Change primitive type to spawn.
	}
	if (ImGui::Button("Spawn"))
	{
		for (int i = 0; i < spawnCount; ++i)
		{
			auto p = PrimitiveFactory::AddPrimitive(primitiveType);
			if (p != nullptr)
			{
				p->SetRelativeLocation(GetRandomSpawnLocation());
				if (primitiveType == EPrimitiveType::SubUV)
				{
					UBillboardSubUVComp* subUV = Cast<UBillboardSubUVComp>(p);
					if (subUV != nullptr)
					{
						subUV->SetPlayback(GetRandomSubUVPlaybackRate(), true);
					}
				}
				else if (primitiveType == EPrimitiveType::TextMesh)
				{
					UTextMeshComponent* TextMesh = Cast<UTextMeshComponent>(p);
					if (TextMesh != nullptr)
					{
						TextMesh->SetText("Hello, world!");
					}
				}
			}
		}
	}
	ImGui::SameLine();
	ImGui::InputInt("#", &spawnCount);
	if (spawnCount < 0)
	{
		spawnCount = 0;
	}
	ImGui::SameLine();
	ImGui::Separator();

	static TArray<FString> SceneList;
	static int32 SelectedSceneIndex = -1;
	static bool bCreateNewSceneAfterSave = false;
	SceneManager* sceneMgr = GetWorld().GetSceneManager();

	if (sceneMgr == nullptr)
	{
		ImGui::Text("Scene manager is not initialized.");
		ImGui::End();
		return;
	}

	if (ImGui::Button("New Scene"))
	{
		sceneMgr->NewScene();
	}
	ImGui::SameLine();
	if (ImGui::Button("Save Scene"))
	{
		ImGui::OpenPopup("Save Scene Popup");
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Scene"))
	{
		SceneList = GetSceneFileList("Scenes/");
		SelectedSceneIndex = -1;
		ImGui::OpenPopup("Load Scene Popup");
	}
	ImGui::SetNextWindowSize(ImVec2(360, 140), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal("Save Scene Popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("Scene Name", sceneName, IM_ARRAYSIZE(sceneName));

		if (ImGui::Button("Save"))
		{
			editor->ClearSelection();
			sceneMgr->SaveScene(sceneName);

			ImGui::CloseCurrentPopup();

			if (bCreateNewSceneAfterSave)
			{
				bCreateNewSceneAfterSave = false;
				sceneMgr->NewScene();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			bCreateNewSceneAfterSave = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	// Load Scene 팝업
	ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal("Load Scene Popup", nullptr))
	{
		if (ImGui::BeginChild("SceneList", ImVec2(0, 220), true))
		{
			for (int32 i = 0; i < (int32)SceneList.size(); ++i)
			{
				const bool bSelected = (SelectedSceneIndex == i);
				if (ImGui::Selectable(SceneList[i].c_str(), bSelected))
				{
					SelectedSceneIndex = i;
				}
			}
			ImGui::EndChild();
		}

		const bool bCanLoad = (SelectedSceneIndex >= 0 && SelectedSceneIndex < (int32)SceneList.size());

		if (!bCanLoad)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Load"))
		{
			editor->ClearSelection();
			sceneMgr->LoadScene(SceneList[SelectedSceneIndex].c_str());
			ImGui::CloseCurrentPopup();
		}

		if (!bCanLoad)
		{
			ImGui::EndDisabled();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::Separator();

	if (camera)
	{
		ImGui::Text("CAMERA");
		ImGui::Checkbox("Orthographic", &camera->IsOrthogonal);
		ImGui::DragFloat("FOV", &camera->FOV, 1.0f, 40.f, 160.f);

		FVector camPosition = camera->GetRelativeLocation();
		FRotator camRotation = camera->GetRelativeRotation();
		ImGui::DragFloat3("Location", &camPosition.x);
		ImGui::DragFloat3("Rotation", &camRotation.Roll);
		camera->SetRelativeLocation(camPosition);
		camera->SetRelativeRotation(camRotation);

		ImGui::DragFloat3("Direction", &camera->Direction.x);
		ImGui::SliderFloat("Move Speed", &camera->MoveSpeed, 0.0f, 20.0f, "%.0f");
		ImGui::SliderFloat("Rotation Speed", &camera->RotateSensitivity, 0.0f, 20.0f, "%.0f");
	}

	ImGui::Separator();
	ImGui::Text("Grid Settings");

	bool bChanged = false;
	if (ImGui::DragFloat("Half Extent", &Grid->HalfExtent, 1.0f, 1.0f, 1000.0f)) bChanged = true;
	if (ImGui::DragFloat("Cell Size", &Grid->CellSize, 0.1f, 1.0f, 10.0f)) bChanged = true;
	if (bChanged)
	{
		ResourceManager::GetInstance()->UpdateGridMesh(Grid->HalfExtent, Grid->CellSize, 0.f);
	}

	ImGui::Separator();
	if (Renderer)
	{
		ImGui::Checkbox("Show Primitives", &Renderer->ShowFlagMap[EEngineShowFlags::SF_Primitives]);
		ImGui::Checkbox("Show Billboard Text", &Renderer->ShowFlagMap[EEngineShowFlags::SF_BillboardText]);
		ImGui::Checkbox("Show Editor Primitives", &Renderer->ShowFlagMap[EEngineShowFlags::SF_EditorPrimitives]);
		ImGui::Checkbox("Show DebugLines ", &Renderer->ShowFlagMap[EEngineShowFlags::SF_DebugLines]);
	}
	static int32 RenderingMode = 0;
	if (ImGui::Combo("RenderingMode", &RenderingMode, "Lit\0UnLit\0Wireframe\0"))
	{
		Renderer->SetRasterizerMode(RenderingMode);
	}
	//for (auto& elem : Renderer->ShowFlagMap)
	//{
	//	ImGui::Checkbox(elem.first.c_str(), &elem.second);
	//}

	ImGui::Separator();
	ImGui::Text("DEBUG");
	if (ImGui::Button("Dump Glyph Atlas BMP"))
	{
		const FString DumpPath = "GlyphAtlasDump.bmp";
		if (DumpGlyphAtlasToBMP(DumpPath))
		{
			UE_LOG("[EditorControlPanel] Saved glyph atlas dump: %s", DumpPath.c_str());
		}
		else
		{
			UE_LOG("[EditorControlPanel][Error] Failed to dump glyph atlas");
		}
	}

	ImGui::End();
}
