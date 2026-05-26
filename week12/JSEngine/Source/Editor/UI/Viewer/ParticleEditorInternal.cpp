#include "ParticleEditorInternal.h"

namespace ParticleEditorInternal
{
// 파티클 모듈에 대한 우클릭 컨텍스트 메뉴(삭제, 위/아래 이동)를 렌더링하고 처리합니다.
void HandleModuleContextMenu(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex)
{
	if (ImGui::BeginPopupContextItem("ModuleContext"))
	{
		if (ImGui::MenuItem("Delete Module"))
		{
			Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
			Viewer->DeleteSelectedModule();
		}
		if (ImGui::MenuItem("Move Up"))
		{
			Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
			Viewer->MoveModule(ModuleIndex, ModuleIndex - 1);
		}
		if (ImGui::MenuItem("Move Down"))
		{
			Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
			Viewer->MoveModule(ModuleIndex, ModuleIndex + 1);
		}
		ImGui::EndPopup();
	}
}

// 전달된 뷰어 인터페이스가 파티클 뷰어인지 확인하고 알맞은 타입으로 캐스팅하여 반환합니다.
FParticleEditorViewer* AsParticleViewer(FEditorViewer* Viewer)
{
	return Viewer && Viewer->GetTabKind() == EEditorTabKind::ParticleViewer ? static_cast<FParticleEditorViewer*>(Viewer) : nullptr;
}

// OS 파일 저장 다이얼로그를 띄울 때 부모가 될 올바른 윈도우 핸들(HWND)을 탐색하여 반환합니다.
HWND ResolveSaveDialogOwnerWindow(const UEditorEngine* EditorEngine)
{
	if (EditorEngine && EditorEngine->GetWindow())
	{
		return EditorEngine->GetWindow()->GetHWND();
	}

	if (const ImGuiViewport* MainViewport = ImGui::GetMainViewport())
	{
		if (MainViewport->PlatformHandleRaw)
		{
			return static_cast<HWND>(MainViewport->PlatformHandleRaw);
		}
	}

	return ::GetActiveWindow();
}

// 파티클 시스템 에셋 저장을 위한 Windows OS 기본 파일 선택 다이얼로그를 엽니다.
bool OpenParticleSaveFileDialog(HWND OwnerWindow, const FParticleEditorViewer* Viewer, FString& OutFilePath)
{
	OutFilePath.clear();

	constexpr DWORD ParticleDialogPathBufferLength = 32768;
	WCHAR FileBuffer[ParticleDialogPathBufferLength] = {};
	std::filesystem::path InitialDirectory = std::filesystem::path(FPaths::ToAbsolute(L"Asset/Particle")).lexically_normal();
	std::error_code ErrorCode;
	if (!std::filesystem::exists(InitialDirectory, ErrorCode) || !std::filesystem::is_directory(InitialDirectory, ErrorCode))
	{
		InitialDirectory = std::filesystem::path(FPaths::RootDir()).lexically_normal();
	}

	const FString CurrentFileName = Viewer ? FPaths::Normalize(Viewer->GetFileName()) : FString();
	if (!CurrentFileName.empty())
	{
		std::filesystem::path CurrentAbsolutePath = std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(CurrentFileName))).lexically_normal();
		if (!CurrentAbsolutePath.parent_path().empty())
		{
			InitialDirectory = CurrentAbsolutePath.parent_path();
		}
		wcsncpy_s(FileBuffer, CurrentAbsolutePath.wstring().c_str(), _TRUNCATE);
	}

	OPENFILENAMEW DialogDesc = {};
	DialogDesc.lStructSize = sizeof(DialogDesc);
	DialogDesc.hwndOwner = OwnerWindow;
	DialogDesc.lpstrFilter = L"Particle System (*.particle)\0*.particle\0All Files (*.*)\0*.*\0";
	DialogDesc.lpstrFile = FileBuffer;
	DialogDesc.nMaxFile = ParticleDialogPathBufferLength;
	const std::wstring InitialDirectoryText = InitialDirectory.wstring();
	DialogDesc.lpstrInitialDir = InitialDirectoryText.c_str();
	DialogDesc.lpstrDefExt = L"particle";
	DialogDesc.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetSaveFileNameW(&DialogDesc))
	{
		const DWORD DialogError = CommDlgExtendedError();
		if (DialogError != 0)
		{
			WCHAR DebugMessage[128] = {};
			swprintf_s(DebugMessage, L"Particle Save As dialog failed. CommDlgExtendedError=0x%08X\n", DialogError);
			OutputDebugStringW(DebugMessage);
		}
		return false;
	}

	std::filesystem::path PickedPath(FileBuffer);
	if (PickedPath.extension().empty())
	{
		PickedPath.replace_extension(L".particle");
	}

	const std::filesystem::path RootPath(FPaths::RootDir());
	std::error_code RelativeErrorCode;
	std::filesystem::path RelativePath = std::filesystem::relative(PickedPath, RootPath, RelativeErrorCode);
	if (!RelativeErrorCode && !RelativePath.empty())
	{
		const std::wstring RelativeText = RelativePath.generic_wstring();
		if (RelativeText != L".." && RelativeText.rfind(L"../", 0) != 0)
		{
			OutFilePath = FPaths::Normalize(FPaths::ToUtf8(RelativeText));
			return true;
		}
	}

	OutFilePath = FPaths::Normalize(FPaths::ToUtf8(PickedPath.generic_wstring()));
	return !OutFilePath.empty();
}

// 현재 선택된 객체 타입(시스템, 이미터, 모듈 등)에 맞는 UI 출력용 문자열을 반환합니다.
const char* GetSelectionLabel(EParticleEditorSelectionType Type)
{
	switch (Type)
	{
	case EParticleEditorSelectionType::ParticleSystem:
		return "Particle System";
	case EParticleEditorSelectionType::Emitter:
		return "Emitter";
	case EParticleEditorSelectionType::LODLevel:
		return "LOD Level";
	case EParticleEditorSelectionType::RequiredModule:
	case EParticleEditorSelectionType::SpawnModule:
	case EParticleEditorSelectionType::TypeDataModule:
	case EParticleEditorSelectionType::Module:
		return "Module";
	case EParticleEditorSelectionType::None:
	default:
		return "None";
	}
}

// 객체의 클래스 디스플레이 이름을 문자열로 반환하며 유효하지 않을 경우 "None"을 반환합니다.
const char* GetObjectLabel(const UObject* Object)
{
	if (Object && !UObjectManager::Get().ContainsObject(Object))
	{
		return "None";
	}

	const UClass* Class = Object ? Object->GetClass() : nullptr;
	return Class ? Class->GetDisplayName() : "None";
}

// 현재 선택된 파티클 이미터가 유효하고 안전하게 삭제 가능한 상태인지 확인합니다.
bool HasDeletableSelectedEmitter(FParticleEditorViewer* Viewer)
{
	UParticleSystem* ParticleSystem = Viewer ? Viewer->GetParticleSystem() : nullptr;
	const int32 SelectedEmitterIndex = Viewer ? Viewer->GetSelectedEmitterIndex() : -1;
	return ParticleSystem != nullptr &&
		   SelectedEmitterIndex >= 0 &&
		   SelectedEmitterIndex < static_cast<int32>(ParticleSystem->Emitters.size()) &&
		   ParticleSystem->Emitters[SelectedEmitterIndex] != nullptr;
}

// 현재 선택된 파티클 모듈이 유효하고 안전하게 삭제 가능한 상태인지 확인합니다.
bool HasDeletableSelectedModule(FParticleEditorViewer* Viewer)
{
	if (!Viewer || Viewer->GetSelectionType() != EParticleEditorSelectionType::Module)
	{
		return false;
	}

	UParticleLODLevel* LOD = Viewer->GetSelectedLODLevel();
	const int32 SelectedModuleIndex = Viewer->GetSelectedModuleIndex();
	return LOD != nullptr &&
		   SelectedModuleIndex >= 0 &&
		   SelectedModuleIndex < static_cast<int32>(LOD->Modules.size()) &&
		   LOD->Modules[SelectedModuleIndex] != nullptr;
}

bool IsAnyPopupOpen()
{
	ImGuiContext* Context = ImGui::GetCurrentContext();
	return Context && Context->OpenPopupStack.Size > 0;
}

// 에디터 내에서 발생하는 키보드 단축키(Undo, Redo, 시뮬레이션 재시작, 아이템 삭제) 이벤트를 감지하고 처리합니다.
void HandleParticleEditorShortcuts(FParticleEditorViewer* Viewer)
{
	if (!Viewer || IsAnyPopupOpen())
	{
		return;
	}

	const ImGuiIO& IO = ImGui::GetIO();
	if (IO.WantTextInput || ImGui::IsAnyItemActive())
	{
		return;
	}

	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		return;
	}

	if (IO.KeyCtrl && !IO.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
	{
		Viewer->Undo();
		return;
	}

	if ((IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) ||
		(IO.KeyCtrl && IO.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)))
	{
		Viewer->Redo();
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		Viewer->RestartSimulation();
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		Viewer->DeleteSelection();
	}
}

// 하위 패널(뷰포트, 디테일, 커브 등)의 상단에 위치하는 스타일화된 타이틀 바를 그립니다.
void DrawParticlePanelTitle(const char* Title, const char* Subtitle)
{
	const ImVec2 Start = ImGui::GetCursorScreenPos();
	const float Width = ImGui::GetContentRegionAvail().x;
	const float Height = 34.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 End(Start.x + Width, Start.y + Height);

	DrawList->AddRectFilled(Start, End, IM_COL32(27, 29, 35, 255), 3.0f);
	DrawList->AddRectFilled(Start, ImVec2(Start.x + Width, Start.y + 2.0f), IM_COL32(112, 146, 214, 255), 3.0f);
	DrawList->AddLine(ImVec2(Start.x, End.y), End, IM_COL32(66, 70, 82, 255), 1.0f);

	const ImVec2 TitleSize = ImGui::CalcTextSize(Title);
	DrawList->AddText(
		ImVec2(Start.x + 10.0f, Start.y + (Height - TitleSize.y) * 0.5f),
		ImGui::GetColorU32(ImVec4(0.93f, 0.95f, 1.0f, 1.0f)),
		Title);

	if (Subtitle && Subtitle[0] != '\0')
	{
		const ImVec2 SubtitleSize = ImGui::CalcTextSize(Subtitle);
		DrawList->AddText(
			ImVec2(End.x - SubtitleSize.x - 10.0f, Start.y + (Height - SubtitleSize.y) * 0.5f),
			ImGui::GetColorU32(ImVec4(0.56f, 0.60f, 0.68f, 1.0f)),
			Subtitle);
	}

	ImGui::Dummy(ImVec2(Width, Height + 8.0f));
}

// 디테일 패널 내부에서 프로퍼티 그룹을 시각적으로 구분짓는 텍스트와 가로 선을 그립니다.
void DrawParticleDetailsSection(const char* Title)
{
	ImGui::Spacing();
	ImGui::TextColored(ImVec4(0.72f, 0.77f, 0.88f, 1.0f), "%s", Title);
	ImGui::Separator();
}

void DrawParticleDetailsText(const char* Label, const char* Value)
{
	ImGui::TextDisabled("%s", Label);
	ImGui::SameLine(150.0f);
	ImGui::TextUnformatted(Value ? Value : "");
}

const char* GetPropertyDisplayName(const FProperty& Property)
{
	return (Property.DisplayName && Property.DisplayName[0] != '\0') ? Property.DisplayName : Property.Name;
}

// ImGui 위젯에 사용하기 위해 프로퍼티 이름과 ID가 합쳐진 고유 라벨 문자열을 생성합니다.
FString MakeParticlePropertyWidgetLabel(const FProperty& Property)
{
	const char* DisplayName = GetPropertyDisplayName(Property);
	if (!DisplayName)
	{
		return "";
	}
	if (!Property.Name || std::strcmp(DisplayName, Property.Name) == 0)
	{
		return DisplayName;
	}
	return FString(DisplayName) + "##" + Property.Name;
}

// 해당 프로퍼티가 UI에서 편집 가능한 단순 타입이 아닌, 그래프 노드 간의 내부 객체 참조 용도인지 확인합니다.
bool IsParticleGraphReferenceProperty(const FProperty& Property)
{
	if (Property.Type == EPropertyType::ObjectPtr)
	{
		return Property.ReferenceKind == EObjectReferenceKind::RuntimeObject;
	}
	return Property.Type == EPropertyType::Array &&
		   Property.InnerProperty &&
		   Property.InnerProperty->Type == EPropertyType::ObjectPtr &&
		   Property.InnerProperty->ReferenceKind == EObjectReferenceKind::RuntimeObject;
}

// 리플렉션 데이터를 순회하여 객체에서 UI 편집이 가능한 프로퍼티들의 목록을 수집합니다.
void CollectParticleEditableProperties(UObject* Object, TArray<const FProperty*>& OutProperties)
{
	if (!Object || !Object->GetClass())
	{
		return;
	}

	TArray<const FProperty*> AllProperties;
	Object->GetClass()->GetAllProperties(AllProperties);
	for (const FProperty* Property : AllProperties)
	{
		if (!Property || !Property->Name || !Property->IsEditable())
		{
			continue;
		}
		if (IsParticleGraphReferenceProperty(*Property))
		{
			continue;
		}
		OutProperties.push_back(Property);
	}
}

// 객체에서 수집된 편집 가능한 프로퍼티들을 디테일 패널에 일괄적으로 렌더링합니다.
bool RenderParticleReflectionProperties(FParticlePropertyRenderContext& Context)
{
	TArray<const FProperty*> Properties;
	CollectParticleEditableProperties(Context.Object, Properties);
	for (const FProperty* Property : Properties)
	{
		if (Property)
		{
			RenderParticlePropertyWidget(Context, *Property);
		}
	}
	return !Properties.empty();
}

// 단일 프로퍼티에 대한 위젯을 그리고, 값 변경 시 Undo 스냅샷 저장 및 시뮬레이션 재시작을 트리거합니다.
bool RenderParticlePropertyWidget(FParticlePropertyRenderContext& Context, const FProperty& Property)
{
	FParticleEditorViewer* Viewer = Context.Viewer;
	UObject* Object = Context.Object;
	if (!Viewer || !Object || !Context.bUndoCaptured || !Property.Name || !Property.IsEditable())
	{
		return false;
	}
	bool& bUndoCaptured = *Context.bUndoCaptured;

	void* ValuePtr = Property.GetValuePtr(Object);
	if (!ValuePtr)
	{
		return false;
	}

	const FString Label = MakeParticlePropertyWidgetLabel(Property);
	const bool bChanged = RenderParticlePropertyValueWidget(Context, Property, ValuePtr, Label.c_str());
	if (ImGui::IsItemActivated() && !bUndoCaptured)
	{
		Viewer->CaptureUndoSnapshot("EditParticleProperty");
		bUndoCaptured = true;
	}

	if (bChanged)
	{
		if (!bUndoCaptured)
		{
			Viewer->CaptureUndoSnapshot("EditParticleProperty");
			bUndoCaptured = true;
		}
		Object->PostEditProperty(Property.Name);
		Viewer->MarkDirty();
		Viewer->RestartSimulation();
	}

	if (ImGui::IsItemDeactivatedAfterEdit() || !ImGui::IsAnyItemActive())
	{
		bUndoCaptured = false;
	}

	return bChanged;
}

// 프로퍼티의 데이터 타입(Int, Float, Enum, String 등)을 검사하여 알맞은 ImGui 입력 위젯을 연결하여 그립니다.
bool RenderParticlePropertyValueWidget(FParticlePropertyRenderContext& Context, const FProperty& Property, void* ValuePtr, const char* Label)
{
	UEditorEngine* EditorEngine = Context.EditorEngine;
	if (!ValuePtr)
	{
		return false;
	}

	switch (Property.Type)
	{
	case EPropertyType::Bool:
		return ImGui::Checkbox(Label, static_cast<bool*>(ValuePtr));
	case EPropertyType::Int:
		return ImGui::DragInt(Label, static_cast<int32*>(ValuePtr), Property.Speed);
	case EPropertyType::Float:
		if (Property.Min != 0.0f || Property.Max != 0.0f)
		{
			return ImGui::DragFloat(Label, static_cast<float*>(ValuePtr), Property.Speed, Property.Min, Property.Max);
		}
		return ImGui::DragFloat(Label, static_cast<float*>(ValuePtr), Property.Speed);
	case EPropertyType::String:
	{
		FString* Value = static_cast<FString*>(ValuePtr);
		char Buffer[512];
		strncpy_s(Buffer, sizeof(Buffer), Value->c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			*Value = Buffer;
			return true;
		}
		return false;
	}
	case EPropertyType::Name:
	{
		FName* Value = static_cast<FName*>(ValuePtr);
		FString Current = Value->ToString();
		char Buffer[256];
		strncpy_s(Buffer, sizeof(Buffer), Current.c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			*Value = FName(Buffer);
			return true;
		}
		return false;
	}
	case EPropertyType::Enum:
	{
		if (!Property.EnumMeta || !Property.EnumMeta->Values || Property.EnumMeta->Count == 0)
		{
			return false;
		}

		int64 CurrentValue = 0;
		switch (Property.EnumMeta->Size)
		{
		case 1:
			CurrentValue = static_cast<int64>(*static_cast<uint8*>(ValuePtr));
			break;
		case 2:
			CurrentValue = static_cast<int64>(*static_cast<uint16*>(ValuePtr));
			break;
		case 4:
			CurrentValue = static_cast<int64>(*static_cast<int32*>(ValuePtr));
			break;
		case 8:
			CurrentValue = static_cast<int64>(*static_cast<int64*>(ValuePtr));
			break;
		default:
			break;
		}

		int32 CurrentIndex = 0;
		for (uint32 Index = 0; Index < Property.EnumMeta->Count; ++Index)
		{
			if (Property.EnumMeta->Values[Index].Value == CurrentValue)
			{
				CurrentIndex = static_cast<int32>(Index);
				break;
			}
		}

		const auto ComboGetter = [](void* Data, int Index) -> const char*
		{
			const UEnum* EnumMeta = static_cast<const UEnum*>(Data);
			if (!EnumMeta || Index < 0 || static_cast<uint32>(Index) >= EnumMeta->Count)
			{
				return "";
			}
			const FEnumValue& ValueMeta = EnumMeta->Values[Index];
			return (ValueMeta.DisplayName && ValueMeta.DisplayName[0] != '\0') ? ValueMeta.DisplayName : ValueMeta.Name;
		};

		if (ImGui::Combo(Label, &CurrentIndex, ComboGetter, const_cast<UEnum*>(Property.EnumMeta), static_cast<int>(Property.EnumMeta->Count)))
		{
			const int64 NewValue = Property.EnumMeta->Values[CurrentIndex].Value;
			switch (Property.EnumMeta->Size)
			{
			case 1:
				*static_cast<uint8*>(ValuePtr) = static_cast<uint8>(NewValue);
				break;
			case 2:
				*static_cast<uint16*>(ValuePtr) = static_cast<uint16>(NewValue);
				break;
			case 4:
				*static_cast<int32*>(ValuePtr) = static_cast<int32>(NewValue);
				break;
			case 8:
				*static_cast<int64*>(ValuePtr) = static_cast<int64>(NewValue);
				break;
			default:
				break;
			}
			return true;
		}
		return false;
	}
	case EPropertyType::Struct:
		return RenderParticleStructPropertyWidget(Context, Property, ValuePtr, Label);
	case EPropertyType::ObjectPtr:
		return RenderParticleObjectPtrWidget(Property, ValuePtr, Label, EditorEngine);
	case EPropertyType::SoftObjectPtr:
		return RenderParticleSoftObjectPtrWidget(Property, ValuePtr, Label, EditorEngine);
	case EPropertyType::Array:
		return RenderParticleArrayPropertyWidget(Context, Property, ValuePtr);
	default:
		break;
	}
	return false;
}

// 참조된 외부 객체(예: 머티리얼)를 할당하거나 해제하기 위한 드롭다운 콤보박스 위젯을 그립니다.
bool RenderParticleObjectPtrWidget(const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine)
{
	if (!Property.ObjectPtrOps || !ValuePtr)
	{
		return false;
	}

	UObject* CurrentObject = Property.ObjectPtrOps->GetObject(ValuePtr);
	if (Property.ReferenceKind == EObjectReferenceKind::Asset &&
		Property.ObjectClass &&
		Property.ObjectClass->IsChildOf(UMaterialInterface::StaticClass()) &&
		EditorEngine)
	{
		const TArray<FString>& MaterialNames = EditorEngine->GetAssetService().GetMaterialInterfaceNames();
		UMaterialInterface* CurrentMaterial = Cast<UMaterialInterface>(CurrentObject);
		const FString CurrentLabel = CurrentMaterial
										 ? (CurrentMaterial->GetFilePath().empty() ? CurrentMaterial->GetName() : FPaths::Normalize(CurrentMaterial->GetFilePath()))
										 : FString("<None>");

		bool bChanged = false;
		PushAssetComboStyle();
		if (ImGui::BeginCombo(Label, CurrentLabel.c_str()))
		{
			if (ImGui::Selectable("<None>", CurrentMaterial == nullptr))
			{
				Property.ObjectPtrOps->SetObject(ValuePtr, nullptr);
				bChanged = true;
			}
			for (int32 MaterialIndex = 0; MaterialIndex < static_cast<int32>(MaterialNames.size()); ++MaterialIndex)
			{
				const FString& MaterialLabel = MaterialNames[MaterialIndex];
				const bool bSelected = CurrentLabel == MaterialLabel;
				if (ImGui::Selectable(MaterialLabel.c_str(), bSelected))
				{
					if (UMaterialInterface* Candidate = EditorEngine->GetAssetService().ResolveMaterialInterfaceByIndex(MaterialIndex))
					{
						Property.ObjectPtrOps->SetObject(ValuePtr, Candidate);
						bChanged = true;
					}
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		PopAssetComboStyle();
		return bChanged;
	}

	const FString ObjectLabel = CurrentObject ? CurrentObject->GetClassName() : FString("None");
	ImGui::TextDisabled("%s: %s", Label, ObjectLabel.c_str());
	return false;
}

// 텍스트 기반 에셋 경로 검색 필터를 지원하는 Soft Object 콤보박스 위젯을 렌더링합니다.
bool RenderParticleSoftObjectPtrWidget(const FProperty& Property, void* ValuePtr, const char* Label, UEditorEngine* EditorEngine)
{
	if (!Property.SoftObjectOps || !ValuePtr)
	{
		return false;
	}

	FString Current = Property.SoftObjectOps->GetPath(ValuePtr);
	TArray<FString> LocalOptions;
	const TArray<FString>* Options = nullptr;
	if (Property.ObjectClass)
	{
		if (Property.ObjectClass->IsChildOf(UStaticMesh::StaticClass()) && EditorEngine)
		{
			Options = &EditorEngine->GetAssetService().GetStaticMeshAssetPaths();
		}
		else if (Property.ObjectClass->IsChildOf(UCurveFloatAsset::StaticClass()) ||
				 Property.ObjectClass->IsChildOf(UCurveVectorAsset::StaticClass()) ||
				 Property.ObjectClass->IsChildOf(UCurveColorAsset::StaticClass()))
		{
			LocalOptions = FResourceManager::Get().GetCurvePaths();
			Options = &LocalOptions;
		}
		else if (Property.ObjectClass->IsChildOf(UParticleSystem::StaticClass()))
		{
			LocalOptions = FResourceManager::Get().GetParticleSystemPaths();
			Options = &LocalOptions;
		}
	}

	bool bChanged = false;
	if (Options && !Options->empty())
	{
		FString SelectedPath;
		if (DrawSearchableAssetPathCombo(Label, Current, *Options, SelectedPath))
		{
			Property.SoftObjectOps->SetPath(ValuePtr, SelectedPath);
			bChanged = true;
		}
	}
	else
	{
		char Buffer[512];
		strncpy_s(Buffer, sizeof(Buffer), Current.c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			Property.SoftObjectOps->SetPath(ValuePtr, Buffer);
			bChanged = true;
		}
	}
	return bChanged;
}

// 배열 타입 내의 하위 항목들을 순회하며 각각의 위젯과 함께 항목 추가 및 삭제 기능을 그립니다.
bool RenderParticleArrayPropertyWidget(FParticlePropertyRenderContext& Context, const FProperty& Property, void* ValuePtr)
{
	if (!Property.ArrayOps || !Property.InnerProperty || !ValuePtr)
	{
		return false;
	}
	if (IsParticleGraphReferenceProperty(Property))
	{
		ImGui::TextDisabled("%s is managed by the emitter graph.", GetPropertyDisplayName(Property));
		return false;
	}

	bool bChanged = false;
	int32 RemoveIndex = -1;
	DrawParticleDetailsSection(GetPropertyDisplayName(Property));
	ImGui::PushID(Property.Name);

	const int32 Count = Property.ArrayOps->Num(ValuePtr);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		ImGui::PushID(Index);
		void* ElementPtr = Property.ArrayOps->GetElementPtr(ValuePtr, Index);
		char ItemLabel[32];
		snprintf(ItemLabel, sizeof(ItemLabel), "[%d]", Index);
		ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 28.0f));
		if (RenderParticlePropertyValueWidget(Context, *Property.InnerProperty, ElementPtr, ItemLabel))
		{
			bChanged = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("X"))
		{
			RemoveIndex = Index;
		}
		ImGui::PopID();
	}

	if (RemoveIndex >= 0)
	{
		Property.ArrayOps->RemoveAt(ValuePtr, RemoveIndex);
		bChanged = true;
	}

	char AddLabel[64];
	snprintf(AddLabel, sizeof(AddLabel), "+ Add##%s", Property.Name);
	if (ImGui::Button(AddLabel, ImVec2(-1, 0.0f)))
	{
		Property.ArrayOps->AddDefaulted(ValuePtr);
		bChanged = true;
	}

	ImGui::PopID();
	return bChanged;
}

// 구조체 내부의 프로퍼티를 재귀적으로 전개해 그리거나 힌트에 맞춰 벡터/컬러 등의 수학 타입 위젯을 렌더링합니다.
bool RenderParticleStructPropertyWidget(FParticlePropertyRenderContext& Context, const FProperty& Property, void* ValuePtr, const char* Label)
{
	if (!ValuePtr || Property.Type != EPropertyType::Struct)
	{
		return false;
	}

	const char* Hint = Property.EditorHint;
	if ((!Hint || Hint[0] == '\0') && Property.ScriptStruct)
	{
		Hint = Property.ScriptStruct->GetName();
	}

	if (Hint && std::strcmp(Hint, "FVector") == 0)
	{
		return ImGui::DragFloat3(Label, static_cast<float*>(ValuePtr), Property.Speed);
	}
	if (Hint && std::strcmp(Hint, "FVector4") == 0)
	{
		return ImGui::DragFloat4(Label, static_cast<float*>(ValuePtr), Property.Speed);
	}
	if (Hint && std::strcmp(Hint, "FColor") == 0)
	{
		return ImGui::ColorEdit4(Label, &static_cast<FColor*>(ValuePtr)->R);
	}
	if (Hint && std::strcmp(Hint, "FQuat") == 0)
	{
		FQuat* Value = static_cast<FQuat*>(ValuePtr);
		float Components[4] = { Value->X, Value->Y, Value->Z, Value->W };
		if (ImGui::DragFloat4(Label, Components, Property.Speed))
		{
			*Value = FQuat(Components[0], Components[1], Components[2], Components[3]);
			Value->Normalize();
			return true;
		}
		return false;
	}

	if (!Property.ScriptStruct)
	{
		ImGui::TextDisabled("%s <unregistered struct>", Label);
		return false;
	}

	bool bChanged = false;
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.115f, 0.125f, 0.145f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.20f, 0.24f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.22f, 0.25f, 0.31f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.88f, 0.91f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
	const ImGuiTreeNodeFlags HeaderFlags =
		ImGuiTreeNodeFlags_DefaultOpen |
		ImGuiTreeNodeFlags_Framed |
		ImGuiTreeNodeFlags_SpanAvailWidth;
	const bool bOpen = ImGui::TreeNodeEx(Label, HeaderFlags);
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(4);
	if (bOpen)
	{
		TArray<const FProperty*> ChildProperties;
		Property.ScriptStruct->GetAllProperties(ChildProperties);
		for (const FProperty* Child : ChildProperties)
		{
			if (!Child || !Child->Name || !Child->IsEditable())
			{
				continue;
			}
			void* ChildPtr = reinterpret_cast<uint8*>(ValuePtr) + Child->Offset;
			const FString ChildLabel = MakeParticlePropertyWidgetLabel(*Child);
			if (RenderParticlePropertyValueWidget(Context, *Child, ChildPtr, ChildLabel.c_str()))
			{
				bChanged = true;
			}
		}
		ImGui::TreePop();
	}
	return bChanged;
}

void SortParticleModuleClassesByDisplayName(TArray<UClass*>& Classes)
{
	std::stable_sort(
		Classes.begin(),
		Classes.end(),
		[](const UClass* Lhs, const UClass* Rhs)
		{
			return std::strcmp(Lhs->GetDisplayName(), Rhs->GetDisplayName()) < 0;
		});
}

// 리플렉션 시스템을 통해 이미터에 부착 가능한 일반 파티클 모듈 클래스들의 목록을 수집합니다.
void GetParticleModuleClasses(TArray<UClass*>& OutClasses)
{
	OutClasses.clear();
	FReflectionRegistry::Get().GetClassesDerivedFrom(UParticleModule::StaticClass(), OutClasses);
	OutClasses.erase(
		std::remove_if(
			OutClasses.begin(),
			OutClasses.end(),
			[](const UClass* Class)
			{
				return !Class ||
					   Class == UParticleModule::StaticClass() ||
					   Class->IsChildOf(UParticleModuleTypeDataBase::StaticClass()) ||
					   !Class->HasAnyClassFlags(CF_Placeable) ||
					   Class->HasAnyClassFlags(CF_Abstract);
			}),
		OutClasses.end());
	SortParticleModuleClassesByDisplayName(OutClasses);
}

// 이미터의 렌더링 방식을 결정하는 특수 모듈인 Type Data 모듈 클래스 목록을 추출합니다.
void GetParticleTypeDataModuleClasses(TArray<UClass*>& OutClasses)
{
	OutClasses.clear();
	FReflectionRegistry::Get().GetClassesDerivedFrom(UParticleModuleTypeDataBase::StaticClass(), OutClasses);
	OutClasses.erase(
		std::remove_if(
			OutClasses.begin(),
			OutClasses.end(),
			[](const UClass* Class)
			{
				return !Class ||
					   Class->HasAnyClassFlags(CF_Abstract);
			}),
		OutClasses.end());
	SortParticleModuleClassesByDisplayName(OutClasses);
}

// 새로운 모듈을 추가하기 위한 동적 컨텍스트 메뉴를 렌더링하고 모듈 생성 요청을 처리합니다.
bool DrawParticleModuleClassMenu(FParticleEditorViewer* Viewer)
{
	TArray<UClass*> ModuleClasses;
	GetParticleModuleClasses(ModuleClasses);

	TArray<UClass*> TypeDataModuleClasses;
	GetParticleTypeDataModuleClasses(TypeDataModuleClasses);

	if (ModuleClasses.empty() && TypeDataModuleClasses.empty())
	{
		ImGui::TextDisabled("No particle module classes");
		return false;
	}

	bool bAdded = false;
	for (UClass* ModuleClass : ModuleClasses)
	{
		if (!ModuleClass)
		{
			continue;
		}

		if (ImGui::MenuItem(ModuleClass->GetDisplayName()))
		{
			Viewer->AddModule(ModuleClass);
			bAdded = true;
		}
	}

	if (!ModuleClasses.empty() && !TypeDataModuleClasses.empty())
	{
		ImGui::Separator();
	}

	if (ImGui::BeginMenu("Type Data"))
	{
		if (TypeDataModuleClasses.empty())
		{
			ImGui::TextDisabled("No type data modules");
		}
		for (UClass* ModuleClass : TypeDataModuleClasses)
		{
			if (!ModuleClass)
			{
				continue;
			}

			if (ImGui::MenuItem(ModuleClass->GetDisplayName()))
			{
				Viewer->AddModule(ModuleClass);
				bAdded = true;
			}
		}
		ImGui::EndMenu();
	}
	return bAdded;
}

// 버튼 클릭 시 특정 ID를 가진 ImGui 팝업 메뉴가 열리도록 연결합니다.
bool DrawPopupButton(const char* Label, const char* PopupId)
{
	if (ImGui::Button(Label))
	{
		ImGui::OpenPopup(PopupId);
	}
	return true;
}

// 모서리가 둥글게 처리된 커스텀 디자인의 텍스트 기반 툴바 버튼을 렌더링합니다.
bool DrawRoundedToolbarButton(const char* Id, const char* Label, const char* Tooltip, const ImVec2& Size)
{
	ImGui::PushID(Id);
	const bool bPressed = ImGui::InvisibleButton("##RoundedToolbarButton", Size);
	const bool bHovered = ImGui::IsItemHovered();
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 FillColor = bHovered ? IM_COL32(58, 64, 76, 178) : IM_COL32(38, 42, 50, 138);
	const ImU32 BorderColor = bHovered ? IM_COL32(122, 136, 160, 210) : IM_COL32(88, 96, 112, 165);
	DrawList->AddRectFilled(Min, Max, FillColor, 6.0f);
	DrawList->AddRect(Min, Max, BorderColor, 6.0f);

	const ImVec2 TextSize = ImGui::CalcTextSize(Label);
	DrawList->AddText(
		ImVec2(Center.x - TextSize.x * 0.5f, Center.y - TextSize.y * 0.5f),
		ImGui::GetColorU32(bHovered ? ImGuiCol_Text : ImGuiCol_TextDisabled),
		Label);

	if (bHovered && Tooltip && Tooltip[0] != '\0')
	{
		ImGui::SetTooltip("%s", Tooltip);
	}
	ImGui::PopID();
	return bPressed;
}

// 커브 에디터 출력을 토글할 수 있는 그래프 모양 아이콘의 작은 토글 버튼을 그립니다.
bool DrawCascadeGraphButton(const char* Id, const ImVec2& Size, bool bActive)
{
	ImGui::PushID(Id);
	const bool bPressed = ImGui::InvisibleButton("##CascadeGraphButton", Size);
	const bool bHovered = ImGui::IsItemHovered();
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 BorderColor = bActive
								  ? IM_COL32(190, 236, 120, 255)
								  : (bHovered ? IM_COL32(156, 198, 95, 255) : IM_COL32(86, 132, 62, 255));
	const ImU32 FillColor = bActive
								? IM_COL32(34, 64, 30, 255)
								: IM_COL32(18, 30, 20, 255);
	DrawList->AddRectFilled(Min, Max, FillColor, 0.0f);
	DrawList->AddRect(Min, Max, BorderColor, 0.0f);

	const float Pad = 3.0f;
	DrawList->AddLine(
		ImVec2(Min.x + Pad, Max.y - Pad),
		ImVec2(Min.x + Size.x * 0.42f, Min.y + Size.y * 0.55f),
		BorderColor,
		1.0f);
	DrawList->AddLine(
		ImVec2(Min.x + Size.x * 0.42f, Min.y + Size.y * 0.55f),
		ImVec2(Max.x - Pad, Min.y + Pad),
		BorderColor,
		1.0f);

	ImGui::PopID();
	return bPressed;
}

// 커브 에디터 캔버스의 현재 화면 비율과 목표 픽셀에 기반하여 가장 보기 좋은 그리드 간격 수치를 계산합니다.
float ChooseParticleCurveGridStep(float PixelsPerUnit, float TargetPixels)
{
	const float SafePixelsPerUnit = std::max(0.0001f, PixelsPerUnit);
	const float RawStep = std::max(0.0001f, TargetPixels / SafePixelsPerUnit);
	const float Magnitude = std::pow(10.0f, std::floor(std::log10(RawStep)));
	const float Normalized = RawStep / Magnitude;

	float NiceNormalized = 10.0f;
	if (Normalized <= 1.0f)
	{
		NiceNormalized = 1.0f;
	}
	else if (Normalized <= 2.0f)
	{
		NiceNormalized = 2.0f;
	}
	else if (Normalized <= 5.0f)
	{
		NiceNormalized = 5.0f;
	}

	return NiceNormalized * Magnitude;
}

// 커브 에디터 전용 툴바에 사용할 아이콘과 텍스트 레이블이 포함된 정사각형 버튼을 그립니다.
bool DrawParticleCurveToolbarButton(const char* Id, ID3D11ShaderResourceView* Icon, const char* Label, bool bActive, bool bEnabled)
{
	ImGui::PushID(Id);
	if (!bEnabled)
	{
		ImGui::BeginDisabled();
	}

	constexpr float ButtonWidth = 56.0f;
	constexpr float ButtonHeight = 58.0f;
	constexpr float IconSize = 38.0f;
	const float SmallFontSize = ImGui::GetFontSize() * 0.78f;
	const ImVec2 LabelSize = Label
							   ? ImGui::GetFont()->CalcTextSizeA(SmallFontSize, 1000.0f, 0.0f, Label)
							   : ImVec2(0.0f, 0.0f);
	const ImVec2 ButtonSize(ButtonWidth, ButtonHeight);
	const ImVec2 IconMinSize(IconSize, IconSize);
	const bool bPressed = ImGui::InvisibleButton("##ParticleCurveToolbarButton", ButtonSize);
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	if (bActive || bHovered)
	{
		DrawList->AddRectFilled(
			Min,
			Max,
			bActive ? IM_COL32(58, 53, 23, 255) : IM_COL32(63, 63, 63, 255),
			1.0f);
		DrawList->AddRect(Min, Max, bActive ? ParticleSelectionOutlineColor : IM_COL32(92, 92, 92, 255), 1.0f, 0, bActive ? 2.0f : 1.0f);
	}

	const ImVec2 IconMin(Min.x + (ButtonSize.x - IconMinSize.x) * 0.5f, Min.y + 1.0f);
	const ImVec2 IconMax(IconMin.x + IconMinSize.x, IconMin.y + IconMinSize.y);
	if (Icon)
	{
		DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon), IconMin, IconMax);
	}
	else if (Label && Label[0] != '\0')
	{
		const char Fallback[2] = { Label[0], '\0' };
		const ImVec2 FallbackSize = ImGui::CalcTextSize(Fallback);
		DrawList->AddText(
			ImVec2(IconMin.x + (IconMinSize.x - FallbackSize.x) * 0.5f, IconMin.y + (IconMinSize.y - FallbackSize.y) * 0.5f),
			IM_COL32(220, 224, 232, 255),
			Fallback);
	}

	if (Label)
	{
		const float LabelX = Min.x + std::max(0.0f, (ButtonSize.x - LabelSize.x) * 0.5f);
		const float LabelY = Min.y + 41.0f;
		DrawList->AddText(
			ImGui::GetFont(),
			SmallFontSize,
			ImVec2(LabelX, LabelY),
			ImGui::GetColorU32(ImVec4(0.90f, 0.90f, 0.92f, bEnabled ? 1.0f : 0.42f)),
			Label);
	}

	if (bHovered && Label)
	{
		ImGui::SetTooltip("%s", Label);
	}

	if (!bEnabled)
	{
		ImGui::EndDisabled();
	}
	ImGui::PopID();
	return bEnabled && bPressed;
}

// 커브 에디터 전용 툴바의 항목들을 시각적으로 구분짓는 수직 분리선을 렌더링합니다.
void DrawParticleCurveToolbarSeparator(const char* Id)
{
	ImGui::PushID(Id);
	const ImVec2 Pos = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##ParticleCurveToolbarSeparator", ImVec2(5.0f, 58.0f));
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddLine(ImVec2(Pos.x + 2.0f, Pos.y + 5.0f), ImVec2(Pos.x + 2.0f, Pos.y + 53.0f), IM_COL32(70, 70, 70, 255), 1.0f);
	ImGui::PopID();
}

// 대상 에셋 경로가 입력된 텍스트 필터 문자열과 부분 일치(대소문자 무시)하는지 검사합니다.
bool PassesAssetSearchFilter(const FString& Path, const char* Filter)
{
	if (!Filter || Filter[0] == '\0')
	{
		return true;
	}

	FString LowerPath = Path;
	FString LowerFilter = Filter;
	std::transform(
		LowerPath.begin(),
		LowerPath.end(),
		LowerPath.begin(),
		[](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
	std::transform(
		LowerFilter.begin(),
		LowerFilter.end(),
		LowerFilter.begin(),
		[](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
	return LowerPath.find(LowerFilter) != FString::npos;
}

void PushAssetComboStyle()
{
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.115f, 0.135f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.16f, 0.18f, 0.22f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.19f, 0.215f, 0.26f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.145f, 0.17f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.22f, 0.26f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.25f, 0.32f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.29f, 0.37f, 1.0f));
}

void PopAssetComboStyle()
{
	ImGui::PopStyleColor(7);
}

// 내부 검색 바를 포함하여, 대량의 에셋 목록 중 원하는 경로를 쉽게 필터링해 선택할 수 있는 콤보박스를 그립니다.
bool DrawSearchableAssetPathCombo(const char* Label, const FString& Current, const TArray<FString>& Options, FString& OutSelectedPath)
{
	bool bChanged = false;
	static char SearchBuffer[128] = {};

	ImGui::PushID(Label);
	PushAssetComboStyle();
	const float ComboWidth = std::max(160.0f, ImGui::CalcItemWidth());
	ImGui::SetNextItemWidth(ComboWidth);
	ImGui::SetNextWindowSizeConstraints(ImVec2(ComboWidth, 0.0f), ImVec2(ComboWidth, 340.0f));
	if (ImGui::BeginCombo(Label, Current.empty() ? "<None>" : Current.c_str()))
	{
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##AssetSearch", "Search...", SearchBuffer, sizeof(SearchBuffer));
		ImGui::Separator();

		if (ImGui::Selectable("<None>", Current.empty()))
		{
			OutSelectedPath.clear();
			bChanged = true;
		}

		for (const FString& Path : Options)
		{
			if (!PassesAssetSearchFilter(Path, SearchBuffer))
			{
				continue;
			}

			const bool bSelected = Current == Path;
			if (ImGui::Selectable(Path.c_str(), bSelected))
			{
				OutSelectedPath = Path;
				bChanged = true;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	PopAssetComboStyle();
	ImGui::PopID();

	return bChanged;
}

// 툴바 내에 현재 선택된 LOD 레벨을 표시하고 숫자를 입력하여 LOD를 전환할 수 있는 인풋 필드를 그립니다.
bool DrawCurrentLODToolbarInput(FParticleEditorViewer* Viewer, ID3D11ShaderResourceView* Icon, const ImVec2& IconSize, const ImVec2& Size)
{
	ImGui::PushID("CurrentLODToolbarInput");

	static FParticleEditorViewer* BufferedViewer = nullptr;
	static int32 BufferedLOD = -1;
	static bool bEditing = false;
	static char LODBuffer[16] = {};

	const int32 CurrentLODIndex = std::max(0, Viewer ? Viewer->GetSelectedLODIndex() : 0);
	if (!bEditing || BufferedViewer != Viewer || BufferedLOD != CurrentLODIndex)
	{
		BufferedViewer = Viewer;
		BufferedLOD = CurrentLODIndex;
		snprintf(LODBuffer, sizeof(LODBuffer), "%d", CurrentLODIndex);
	}

	const ImVec2 Start = ImGui::GetCursorScreenPos();
	const ImVec2 End(Start.x + Size.x, Start.y + Size.y);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const bool bHovered = ImGui::IsMouseHoveringRect(Start, End);
	const ImU32 BgColor = ImGui::GetColorU32(bHovered ? ImVec4(0.14f, 0.16f, 0.19f, 1.0f) : ImVec4(0.09f, 0.10f, 0.12f, 1.0f));
	DrawList->AddRectFilled(Start, End, BgColor, 3.0f);

	if (Icon)
	{
		const float Padding = std::max(3.0f, IconSize.x * 0.16f);
		DrawList->AddImage(
			reinterpret_cast<ImTextureID>(Icon),
			ImVec2(Start.x + Padding, Start.y + Padding),
			ImVec2(Start.x + IconSize.x - Padding, Start.y + IconSize.y - Padding));
	}

	const char* Prefix = "LOD:";
	const ImVec2 PrefixSize = ImGui::CalcTextSize(Prefix);
	const float PrefixX = Start.x + IconSize.x + 6.0f;
	DrawList->AddText(
		ImVec2(PrefixX, Start.y + (Size.y - PrefixSize.y) * 0.5f),
		ImGui::GetColorU32(ImVec4(0.94f, 0.95f, 0.98f, 1.0f)),
		Prefix);

	const float InputWidth = 36.0f;
	const float InputHeight = std::max(18.0f, Size.y - 6.0f);
	const ImVec2 InputPos(PrefixX + PrefixSize.x + 4.0f, Start.y + (Size.y - InputHeight) * 0.5f);
	ImGui::SetCursorScreenPos(InputPos);
	ImGui::SetNextItemWidth(InputWidth);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.05f, 0.07f, 0.88f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.08f, 0.10f, 0.13f, 0.95f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.12f, 0.15f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.98f, 1.0f, 1.0f));
	const bool bEnterPressed = ImGui::InputText(
		"##CurrentLODValue",
		LODBuffer,
		sizeof(LODBuffer),
		ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackCharFilter,
		FilterUnsignedIntegerInput);
	const bool bActivated = ImGui::IsItemActivated();
	const bool bActive = ImGui::IsItemActive();
	const bool bDeactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(2);

	if (bActivated)
	{
		bEditing = true;
	}

	bool bCommitted = false;
	if ((bEnterPressed || bDeactivatedAfterEdit) && Viewer)
	{
		const int32 LODCount = GetSelectedEmitterLODCount(Viewer);
		if (LODCount > 0 && IsUnsignedIntegerText(LODBuffer))
		{
			const int32 RequestedLOD = static_cast<int32>(std::strtol(LODBuffer, nullptr, 10));
			const int32 ClampedLOD = std::clamp(RequestedLOD, 0, LODCount - 1);
			Viewer->SelectLOD(ClampedLOD);
			snprintf(LODBuffer, sizeof(LODBuffer), "%d", ClampedLOD);
			BufferedLOD = ClampedLOD;
			bCommitted = true;
		}
		else
		{
			snprintf(LODBuffer, sizeof(LODBuffer), "%d", CurrentLODIndex);
			BufferedLOD = CurrentLODIndex;
		}
		bEditing = false;
	}
	else if (!bActive && bEditing)
	{
		snprintf(LODBuffer, sizeof(LODBuffer), "%d", CurrentLODIndex);
		BufferedLOD = CurrentLODIndex;
		bEditing = false;
	}

	if (bHovered)
	{
		ImGui::SetTooltip("Current LOD");
	}

	ImGui::SetCursorScreenPos(ImVec2(Start.x + Size.x, Start.y));
	ImGui::Dummy(ImVec2(0.0f, Size.y));
	ImGui::PopID();
	return bCommitted;
}

// 뷰어 정보와 인덱스를 활용하여 파티클 시스템 내의 타겟 LOD 레벨 객체를 안전하게 찾아 반환합니다.
UParticleLODLevel* ResolveParticleLOD(FParticleEditorViewer* Viewer, int32 EmitterIndex, int32 LODIndex)
{
	UParticleSystem* ParticleSystem = Viewer ? Viewer->GetParticleSystem() : nullptr;
	if (!ParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return nullptr;
	}

	UParticleEmitter* Emitter = ParticleSystem->Emitters[EmitterIndex];
	if (!Emitter || LODIndex < 0 || LODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return nullptr;
	}

	return Emitter->LODLevels[LODIndex];
}

// 인덱스 및 모듈 타입을 기반으로 타겟 파티클 모듈 객체(Required, Spawn, 일반 모듈 등)를 안전하게 반환합니다.
UParticleModule* ResolveParticleModule(
	FParticleEditorViewer* Viewer,
	EParticleEditorSelectionType Type,
	int32 EmitterIndex,
	int32 LODIndex,
	int32 ModuleIndex)
{
	UParticleLODLevel* LOD = ResolveParticleLOD(Viewer, EmitterIndex, LODIndex);
	if (!LOD)
	{
		return nullptr;
	}

	switch (Type)
	{
	case EParticleEditorSelectionType::RequiredModule:
		return LOD->RequiredModule;
	case EParticleEditorSelectionType::SpawnModule:
		return LOD->SpawnModule;
	case EParticleEditorSelectionType::TypeDataModule:
		return LOD->TypeDataModule;
	case EParticleEditorSelectionType::Module:
		return ModuleIndex >= 0 && ModuleIndex < static_cast<int32>(LOD->Modules.size())
				   ? LOD->Modules[ModuleIndex]
				   : nullptr;
	default:
		return nullptr;
	}
}

// 지정된 인덱스와 모듈 종류에 맞추어 에디터의 현재 선택 상태(Selection)를 동적으로 업데이트합니다.
void SelectParticleModuleTarget(
	FParticleEditorViewer* Viewer,
	EParticleEditorSelectionType Type,
	int32 EmitterIndex,
	int32 LODIndex,
	int32 ModuleIndex)
{
	if (!Viewer)
	{
		return;
	}

	Viewer->SelectEmitter(EmitterIndex);
	Viewer->SelectLOD(LODIndex);
	switch (Type)
	{
	case EParticleEditorSelectionType::RequiredModule:
		Viewer->SelectRequiredModule();
		break;
	case EParticleEditorSelectionType::SpawnModule:
		Viewer->SelectSpawnModule();
		break;
	case EParticleEditorSelectionType::TypeDataModule:
		Viewer->SelectTypeDataModule();
		break;
	case EParticleEditorSelectionType::Module:
		Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
		break;
	default:
		break;
	}
}

bool ContainsIndex(const TArray<int32>& Indices, int32 Index)
{
	return std::find(Indices.begin(), Indices.end(), Index) != Indices.end();
}

void ToggleIndex(TArray<int32>& Indices, int32 Index)
{
	auto It = std::find(Indices.begin(), Indices.end(), Index);
	if (It != Indices.end())
	{
		Indices.erase(It);
		return;
	}

	Indices.push_back(Index);
	std::sort(Indices.begin(), Indices.end());
}

// 뷰어에서 현재 단일 선택되어 있는 이미터를 드래그/복사 대상이 되는 다중 선택 배열의 초기값으로 시드(Seed)합니다.
void SeedEmitterMultiSelectionFromViewer(FParticleEditorViewer* Viewer, TArray<int32>& EmitterIndices)
{
	if (!Viewer || !EmitterIndices.empty())
	{
		return;
	}

	const int32 SelectedEmitterIndex = Viewer->GetSelectedEmitterIndex();
	if (SelectedEmitterIndex >= 0)
	{
		EmitterIndices.push_back(SelectedEmitterIndex);
	}
}

// 단일 선택된 상태에서 Ctrl/Shift 다중 선택이 시작될 때 현재 선택된 단일 모듈을 배열에 포함하여 컨텍스트를 시드합니다.
void SeedModuleMultiSelectionFromViewer(
	FParticleEditorViewer* Viewer,
	TArray<int32>& ModuleIndices,
	int32& MultiEmitterIndex,
	int32& MultiLODIndex,
	int32 EmitterIndex,
	int32 LODIndex)
{
	if (!Viewer || !ModuleIndices.empty())
	{
		return;
	}

	if (Viewer->GetSelectionType() != EParticleEditorSelectionType::Module ||
		Viewer->GetSelectedEmitterIndex() != EmitterIndex ||
		Viewer->GetSelectedLODIndex() != LODIndex ||
		Viewer->GetSelectedModuleIndex() < 0)
	{
		return;
	}

	MultiEmitterIndex = EmitterIndex;
	MultiLODIndex = LODIndex;
	ModuleIndices.push_back(Viewer->GetSelectedModuleIndex());
}

void ClearModuleMultiSelection(TArray<int32>& ModuleIndices, int32& EmitterIndex, int32& LODIndex)
{
	ModuleIndices.clear();
	EmitterIndex = -1;
	LODIndex = -1;
}

// 다중 선택 시 모듈이 속한 부모 이미터나 LOD가 다를 경우 다중 선택 상태를 초기화하고 기준 컨텍스트를 재설정합니다.
void SetModuleMultiSelectionContext(TArray<int32>& ModuleIndices, int32& MultiEmitterIndex, int32& MultiLODIndex, int32 EmitterIndex, int32 LODIndex)
{
	if (MultiEmitterIndex != EmitterIndex || MultiLODIndex != LODIndex)
	{
		ModuleIndices.clear();
		MultiEmitterIndex = EmitterIndex;
		MultiLODIndex = LODIndex;
	}
}

// 드래그 앤 드롭을 처리하기 위해 현재 선택된 단일/다중 모듈의 인덱스 정보를 담은 Payload 구조체를 생성합니다.
void BuildModulePayload(
	FParticleModuleDragPayload& Payload,
	const FParticleModuleAddress& Address,
	const FParticleModuleMultiSelectionView& MultiSelection)
{
	Payload = {};
	Payload.EmitterIndex = Address.EmitterIndex;
	Payload.LODIndex = Address.LODIndex;
	Payload.ModuleIndex = Address.ModuleIndex;

	const TArray<int32>* MultiSelectedModuleIndices = MultiSelection.ModuleIndices;
	const int32 MultiSelectedModuleEmitterIndex = MultiSelection.EmitterIndex ? *MultiSelection.EmitterIndex : -1;
	const int32 MultiSelectedModuleLODIndex = MultiSelection.LODIndex ? *MultiSelection.LODIndex : -1;
	const bool bUseMultiSelection =
		MultiSelectedModuleIndices &&
		MultiSelectedModuleEmitterIndex == Address.EmitterIndex &&
		MultiSelectedModuleLODIndex == Address.LODIndex &&
		ContainsIndex(*MultiSelectedModuleIndices, Address.ModuleIndex);
	if (bUseMultiSelection)
	{
		for (int32 Index : *MultiSelectedModuleIndices)
		{
			if (Payload.Count >= MaxParticleDragSelectionCount)
			{
				break;
			}
			Payload.ModuleIndices[Payload.Count++] = Index;
		}
	}

	if (Payload.Count == 0)
	{
		Payload.ModuleIndices[Payload.Count++] = Address.ModuleIndex;
	}
}

// 다중 또는 단일로 선택된 이미터들을 다른 위치로 드래그 앤 드롭하기 위해 Payload 구조체를 생성합니다.
void BuildEmitterPayload(
	FParticleEmitterDragPayload& Payload,
	int32 EmitterIndex,
	const TArray<int32>& MultiSelectedEmitterIndices)
{
	Payload = {};
	Payload.EmitterIndex = EmitterIndex;

	if (ContainsIndex(MultiSelectedEmitterIndices, EmitterIndex))
	{
		for (int32 Index : MultiSelectedEmitterIndices)
		{
			if (Payload.Count >= MaxParticleDragSelectionCount)
			{
				break;
			}
			Payload.EmitterIndices[Payload.Count++] = Index;
		}
	}

	if (Payload.Count == 0)
	{
		Payload.EmitterIndices[Payload.Count++] = EmitterIndex;
	}
}

// 전달받은 드래그 앤 드롭 페이로드 구조체 내에서 중복을 제거한 선택 모듈의 인덱스 배열을 추출합니다.
TArray<int32> GetPayloadModuleIndices(const FParticleModuleDragPayload& Payload)
{
	TArray<int32> Indices;
	const int32 Count = std::clamp(Payload.Count, 0, MaxParticleDragSelectionCount);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!ContainsIndex(Indices, Payload.ModuleIndices[Index]))
		{
			Indices.push_back(Payload.ModuleIndices[Index]);
		}
	}
	if (Indices.empty() && Payload.ModuleIndex >= 0)
	{
		Indices.push_back(Payload.ModuleIndex);
	}
	std::sort(Indices.begin(), Indices.end());
	return Indices;
}

// 전달받은 드래그 앤 드롭 페이로드에서 다중 선택된 이미터들의 고유 인덱스 배열을 정렬하여 반환합니다.
TArray<int32> GetPayloadEmitterIndices(const FParticleEmitterDragPayload& Payload)
{
	TArray<int32> Indices;
	const int32 Count = std::clamp(Payload.Count, 0, MaxParticleDragSelectionCount);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!ContainsIndex(Indices, Payload.EmitterIndices[Index]))
		{
			Indices.push_back(Payload.EmitterIndices[Index]);
		}
	}
	if (Indices.empty() && Payload.EmitterIndex >= 0)
	{
		Indices.push_back(Payload.EmitterIndex);
	}
	std::sort(Indices.begin(), Indices.end());
	return Indices;
}

// 드롭된 모듈 페이로드를 읽어 타겟 이미터로 복사(Ctrl 누름)하거나 이동시킵니다.
void ApplyModulePayloadToEmitter(FParticleEditorViewer* Viewer, const FParticleModuleDragPayload& Payload, int32 TargetEmitterIndex)
{
	const TArray<int32> ModuleIndices = GetPayloadModuleIndices(Payload);
	if (ImGui::GetIO().KeyCtrl)
	{
		Viewer->CopyModulesToEmitter(Payload.EmitterIndex, Payload.LODIndex, ModuleIndices, TargetEmitterIndex);
	}
	else
	{
		Viewer->MoveModulesToEmitter(Payload.EmitterIndex, Payload.LODIndex, ModuleIndices, TargetEmitterIndex);
	}
}

int32 GetSelectedEmitterLODCount(FParticleEditorViewer* Viewer)
{
	UParticleEmitter* Emitter = Viewer ? Viewer->GetSelectedEmitter() : nullptr;
	return Emitter ? static_cast<int32>(Emitter->LODLevels.size()) : 0;
}

bool IsUnsignedIntegerText(const char* Text)
{
	if (!Text || Text[0] == '\0')
	{
		return false;
	}

	for (const char* It = Text; *It != '\0'; ++It)
	{
		if (*It < '0' || *It > '9')
		{
			return false;
		}
	}

	return true;
}

int FilterUnsignedIntegerInput(ImGuiInputTextCallbackData* Data)
{
	if (!Data)
	{
		return 1;
	}

	return Data->EventChar >= '0' && Data->EventChar <= '9' ? 0 : 1;
}

// 이미터 패널 헤더에 노출되는 파티클 시뮬레이션 느낌의 작은 썸네일 프리뷰 영역을 렌더링합니다.
void DrawEmitterPreview(const ImVec2& Size, int32 EmitterIndex, bool bSelected)
{
	const ImVec2 Start = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##EmitterPreview", Size);

	const ImVec2 End(Start.x + Size.x, Start.y + Size.y);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(Start, End, IM_COL32(18, 20, 24, 255), 0.0f);
	DrawList->AddRect(Start, End, bSelected ? IM_COL32(240, 219, 79, 255) : IM_COL32(76, 78, 86, 255), 0.0f);

	const ImU32 Warm = IM_COL32(255, 126, 82, 210);
	const ImU32 Cool = IM_COL32(104, 190, 255, 180);
	const ImU32 Soft = IM_COL32(255, 220, 130, 170);
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const float XRatio = 0.18f + 0.68f * static_cast<float>(((EmitterIndex * 17 + Index * 31) % 100)) / 100.0f;
		const float YRatio = 0.18f + 0.62f * static_cast<float>(((EmitterIndex * 29 + Index * 23) % 100)) / 100.0f;
		const float Radius = 2.0f + static_cast<float>((Index + EmitterIndex) % 4);
		const ImU32 Color = Index % 3 == 0 ? Warm : (Index % 3 == 1 ? Cool : Soft);
		const ImVec2 Center(Start.x + Size.x * XRatio, Start.y + Size.y * YRatio);
		DrawList->AddCircleFilled(Center, Radius, Color, 12);
	}
	DrawList->AddLine(
		ImVec2(Start.x + Size.x * 0.16f, End.y - Size.y * 0.18f),
		ImVec2(End.x - Size.x * 0.14f, Start.y + Size.y * 0.24f),
		IM_COL32(255, 255, 255, 32),
		1.0f);
}

// 이미터 노드 그래프 내에서 선택, 활성화 토글, 커브 뷰어 진입 등의 인터랙션이 가능한 단일 모듈 항목 행을 그립니다.
void DrawSelectableModuleRow(const FParticleModuleRowContext& Context, const FParticleModuleRowDesc& Row)
{
	FParticleEditorViewer* Viewer = Context.Viewer;
	const char* Label = Row.Label ? Row.Label : "";
	const FParticleModuleAddress& Address = Row.Address;
	const EParticleEditorSelectionType Type = Address.Type;
	const int32 EmitterIndex = Address.EmitterIndex;
	const int32 LODIndex = Address.LODIndex;
	const int32 ModuleIndex = Address.ModuleIndex;
	const ImU32 BackgroundColor = Row.BackgroundColor;

	if (!Viewer || !Context.CurveSource.Type || !Context.CurveSource.EmitterIndex ||
		!Context.CurveSource.LODIndex || !Context.CurveSource.ModuleIndex ||
		!Context.MultiSelection.ModuleIndices || !Context.MultiSelection.EmitterIndex ||
		!Context.MultiSelection.LODIndex)
	{
		return;
	}

	EParticleEditorSelectionType& CurveSourceType = *Context.CurveSource.Type;
	int32& CurveSourceEmitterIndex = *Context.CurveSource.EmitterIndex;
	int32& CurveSourceLODIndex = *Context.CurveSource.LODIndex;
	int32& CurveSourceModuleIndex = *Context.CurveSource.ModuleIndex;
	TArray<int32>& MultiSelectedModuleIndices = *Context.MultiSelection.ModuleIndices;
	int32& MultiSelectedModuleEmitterIndex = *Context.MultiSelection.EmitterIndex;
	int32& MultiSelectedModuleLODIndex = *Context.MultiSelection.LODIndex;
	const bool bMultiSelected =
		Type == EParticleEditorSelectionType::Module &&
		MultiSelectedModuleEmitterIndex == EmitterIndex &&
		MultiSelectedModuleLODIndex == LODIndex &&
		ContainsIndex(MultiSelectedModuleIndices, ModuleIndex);
	const bool bSelected = bMultiSelected ||
		Viewer->GetSelectionType() == Type &&
		Viewer->GetSelectedEmitterIndex() == EmitterIndex &&
		Viewer->GetSelectedLODIndex() == LODIndex &&
		(Type != EParticleEditorSelectionType::Module || Viewer->GetSelectedModuleIndex() == ModuleIndex);

	ImGui::PushID(static_cast<int>(Type));
	ImGui::PushID(ModuleIndex);

	const ImVec2 RowStart = ImGui::GetCursorScreenPos();
	const float RowHeight = ImGui::GetTextLineHeight() + 6.0f;
	const float TextLeftPadding = 12.0f;
	const float ControlSize = 16.0f;
	const float ControlGap = 5.0f;
	const float RowWidth = EmitterNodeWidth;
	const float SelectableWidth = std::max(1.0f, RowWidth - (ControlSize * 2.0f + ControlGap + 8.0f));
	const ImVec2 RowEnd(RowStart.x + RowWidth, RowStart.y + RowHeight);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	if ((BackgroundColor & IM_COL32_A_MASK) != 0)
	{
		DrawList->AddRectFilled(
			RowStart,
			RowEnd,
			BackgroundColor,
			0.0f);
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
	const bool bPressed = ImGui::InvisibleButton("##SelectableModuleRow", ImVec2(SelectableWidth, RowHeight));
	ImGui::PopStyleVar();
	const bool bRowHovered = ImGui::IsItemHovered();

	if (Type == EParticleEditorSelectionType::Module)
	{
		if (ImGui::BeginDragDropSource())
		{
			FParticleModuleDragPayload Payload;
			BuildModulePayload(Payload, Address, Context.MultiSelection);
			ImGui::SetDragDropPayload(ParticleModuleDragPayload, &Payload, sizeof(Payload));
			if (Payload.Count > 1)
			{
				ImGui::Text("%d Modules", Payload.Count);
			}
			else
			{
				ImGui::Text("Module: %s", Label);
			}
			ImGui::EndDragDropSource();
		}
	}
	HandleModuleDragDropTarget(Viewer, Address);

	if (bSelected || bRowHovered)
	{
		const ImU32 StateColor = ImGui::GetColorU32(
			bSelected
				? ImVec4(0.22f, 0.33f, 0.55f, 0.78f)
				: ImVec4(0.22f, 0.24f, 0.30f, 0.42f));
		DrawList->AddRectFilled(RowStart, RowEnd, StateColor, 0.0f);
	}

	const ImVec2 TextSize = ImGui::CalcTextSize(Label);
	DrawList->AddText(
		ImVec2(RowStart.x + TextLeftPadding, RowStart.y + (RowHeight - TextSize.y) * 0.5f),
		ImGui::GetColorU32(ImGuiCol_Text),
		Label);

	UParticleModule* Module = ResolveParticleModule(Viewer, Type, EmitterIndex, LODIndex, ModuleIndex);
	if (Module)
	{
		const float RightControlInset = ImGui::GetStyle().ScrollbarSize + 4.0f;
		const float GraphX = RowEnd.x - RightControlInset - ControlSize;
		const float CheckX = GraphX - ControlGap - ControlSize;
		const float ControlY = RowStart.y + (RowHeight - ControlSize) * 0.5f;

		ImGui::SetCursorScreenPos(ImVec2(CheckX, ControlY));
		bool bModuleEnabled = Module->bEnabled;
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		if (ImGui::Checkbox("##ModuleEnabled", &bModuleEnabled))
		{
			Viewer->CaptureUndoSnapshot("EditModuleEnabled");
			Module->bEnabled = bModuleEnabled;
			SelectParticleModuleTarget(Viewer, Type, EmitterIndex, LODIndex, ModuleIndex);
			Viewer->MarkDirty();
			Viewer->RestartSimulation();
		}
		ImGui::PopStyleVar(2);

		const bool bCurveActive =
			CurveSourceType == Type &&
			CurveSourceEmitterIndex == EmitterIndex &&
			CurveSourceLODIndex == LODIndex &&
			CurveSourceModuleIndex == ModuleIndex;
		ImGui::SetCursorScreenPos(ImVec2(GraphX, ControlY));
		if (DrawCascadeGraphButton("##ModuleCurve", ImVec2(ControlSize, ControlSize), bCurveActive))
		{
			CurveSourceType = Type;
			CurveSourceEmitterIndex = EmitterIndex;
			CurveSourceLODIndex = LODIndex;
			CurveSourceModuleIndex = ModuleIndex;
			SelectParticleModuleTarget(Viewer, Type, EmitterIndex, LODIndex, ModuleIndex);
		}
	}

	if (bPressed)
	{
		if (Type == EParticleEditorSelectionType::Module && ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift)
		{
			SeedModuleMultiSelectionFromViewer(
				Viewer,
				MultiSelectedModuleIndices,
				MultiSelectedModuleEmitterIndex,
				MultiSelectedModuleLODIndex,
				EmitterIndex,
				LODIndex);
			SetModuleMultiSelectionContext(
				MultiSelectedModuleIndices,
				MultiSelectedModuleEmitterIndex,
				MultiSelectedModuleLODIndex,
				EmitterIndex,
				LODIndex);
			ToggleIndex(MultiSelectedModuleIndices, ModuleIndex);
			Viewer->SelectEmitterModule(EmitterIndex, LODIndex, ModuleIndex);
		}
		else
		{
			ClearModuleMultiSelection(MultiSelectedModuleIndices, MultiSelectedModuleEmitterIndex, MultiSelectedModuleLODIndex);
			SelectParticleModuleTarget(Viewer, Type, EmitterIndex, LODIndex, ModuleIndex);
		}
	}

	if (Type == EParticleEditorSelectionType::Module)
	{
		HandleModuleContextMenu(Viewer, EmitterIndex, LODIndex, ModuleIndex);
	}

	ImGui::SetCursorScreenPos(ImVec2(RowStart.x, RowEnd.y));
	ImGui::PopID();
	ImGui::PopID();
}

// 모듈 행이 드래그 앤 드롭의 타겟이 되었을 때, 다른 모듈의 순서 변경이나 이미터 간 복사/이동을 처리합니다.
void HandleModuleDragDropTarget(FParticleEditorViewer* Viewer, const FParticleModuleAddress& Address)
{
	const EParticleEditorSelectionType Type = Address.Type;
	const int32 EmitterIndex = Address.EmitterIndex;
	const int32 LODIndex = Address.LODIndex;
	const int32 ModuleIndex = Address.ModuleIndex;
	if (!ImGui::BeginDragDropTarget())
	{
		return;
	}

	if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleModuleDragPayload))
	{
		const FParticleModuleDragPayload* DragPayload = static_cast<const FParticleModuleDragPayload*>(Payload->Data);
		if (DragPayload)
		{
			const TArray<int32> ModuleIndices = GetPayloadModuleIndices(*DragPayload);
			const bool bSameModuleList =
				Type == EParticleEditorSelectionType::Module &&
				DragPayload->EmitterIndex == EmitterIndex &&
				DragPayload->LODIndex == LODIndex;
			if (bSameModuleList && !ImGui::GetIO().KeyCtrl)
			{
				Viewer->MoveModules(EmitterIndex, LODIndex, ModuleIndices, ModuleIndex);
			}
			else
			{
				Viewer->SelectEmitter(DragPayload->EmitterIndex);
				Viewer->SelectLOD(DragPayload->LODIndex);
				if (ImGui::GetIO().KeyCtrl)
				{
					Viewer->CopyModulesToEmitter(DragPayload->EmitterIndex, DragPayload->LODIndex, ModuleIndices, EmitterIndex);
				}
				else
				{
					Viewer->MoveModulesToEmitter(DragPayload->EmitterIndex, DragPayload->LODIndex, ModuleIndices, EmitterIndex);
				}
			}
		}
	}

	ImGui::EndDragDropTarget();
}

// 뷰포트의 렌더링 뷰 모드(Lit, Unlit, Wireframe 등)를 선택할 수 있는 메뉴 아이템들을 그립니다.
void DrawViewModeMenuItems(FParticleEditorViewer* Viewer)
{
	FEditorMainPanelViewportToolbarHelpers::ForEachViewMode(
		[Viewer](EViewMode Mode)
		{
			if (ImGui::MenuItem(
					FEditorMainPanelViewportToolbarHelpers::GetViewModeName(Mode),
					nullptr,
					Viewer->GetViewMode() == Mode))
			{
				Viewer->SetViewMode(Mode);
			}
		});
}
} // namespace
