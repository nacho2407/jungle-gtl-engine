#include "PropertiesPanel.h"

// 기초 에셋 헤더를 최상단에 배치하여 불완전 형식 에러 방지
#include "Asset/MaterialInterface.h"
#include "Asset/Material.h"
#include "Asset/StaticMesh.h"

// 컴포넌트 헤더 배치
#include "Engine/Component/Core/MeshComponent.h"
#include "Engine/Component/Mesh/StaticMeshComponent.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "Engine/Component/Core/UnknownComponent.h"

#include "Asset/AssetManager.h"
#include "Content/ContentBrowserDragDrop.h"
#include "CoreUObject/UObjectIterator.h" // 이터레이터 헤더
#include "Editor/Editor.h"
#include "Editor/EditorContext.h"
#include "CoreUObject/Object.h"
#include "Core/Misc/Name.h"
#include "Engine/Game/Actor.h"
#include "Engine/Game/UnknownActor.h"
#include "SceneIO/SceneAssetPath.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cctype>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern ENGINE_API FUObjectArray GUObjectArray;

namespace
{
    constexpr ImVec4 UnknownItemColor = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);

    /**
     * 객체의 이름을 안전하게 가져옵니다.
     * 댕글링 포인터 방지를 위해 IsValidLowLevel()을 철저히 확인합니다.
     */
    FString GetBaseObjectDisplayName(const UObject* Object)
    {
        // 1. nullptr 및 메모리 유효성 검사 (IsValidLowLevel 활용)
        if (Object == nullptr || !Object->IsValidLowLevel())
            return "Invalid Object";

        // 2. 이름 유효성 검사
        FString NameStr = Object->Name.IsValid() ? Object->Name.ToFString() : "";
        if (NameStr.empty() || NameStr == "None")
        {
            const char* RawTypeName = Object->GetTypeName();
            if (RawTypeName == nullptr)
                return "Unknown Type";

            if ((RawTypeName[0] == 'U' || RawTypeName[0] == 'A') && isupper(RawTypeName[1]))
                return FString(RawTypeName + 1);
            return FString(RawTypeName);
        }
        return NameStr;
    }

    bool IsUnknownObject(const UObject* Object)
    {
        if (Object == nullptr || !Object->IsValidLowLevel())
            return false;
        return Object->IsA(AUnknownActor::GetClass()) ||
               Object->IsA(Engine::Component::UUnknownComponent::GetClass());
    }

    const char* GetUnknownSuffix(const UObject* Object)
    {
        if (Object == nullptr || !Object->IsValidLowLevel())
            return nullptr;
        if (Object->IsA(AUnknownActor::GetClass()))
            return "(UnknownActor)";
        if (Object->IsA(Engine::Component::UUnknownComponent::GetClass()))
            return "(UnknownComponent)";
        return nullptr;
    }

    bool IsComponentOwnedByActor(const AActor*                             Actor,
                                 const Engine::Component::USceneComponent* Component)
    {
        // 배우와 컴포넌트 모두 유효한 상태인지 확인
        if (Actor == nullptr || !Actor->IsValidLowLevel() || Component == nullptr ||
            !Component->IsValidLowLevel())
            return false;
        return Component->GetOwnerActor() == Actor;
    }

    bool ShouldShowComponentInDetailsTree(const AActor*                             Actor,
                                          const Engine::Component::USceneComponent* Component)
    {
        return IsComponentOwnedByActor(Actor, Component) && Component->ShouldShowInDetailsTree();
    }

    /** 액터 내에서 중복되지 않는 고유한 컴포넌트 이름을 생성합니다. (예: StaticMeshComponent 1) */
    FString GenerateUniqueComponentName(AActor* Actor, const FString& BaseName)
    {
        if (Actor == nullptr || !Actor->IsValidLowLevel())
            return BaseName;

        const auto& Components = Actor->GetOwnedComponents();
        int32       Suffix = 1;
        FString     FinalName = BaseName + " " + std::to_string(Suffix);

        bool bUnique = false;
        while (!bUnique)
        {
            bUnique = true;
            for (auto* Comp : Components)
            {
                if (Comp && Comp->IsValidLowLevel() && Comp->Name.ToFString() == FinalName)
                {
                    Suffix++;
                    FinalName = BaseName + " " + std::to_string(Suffix);
                    bUnique = false;
                    break;
                }
            }
        }
        return FinalName;
    }

    void DrawObjectSummaryLine(const char* Prefix, const UObject* Object)
    {
        if (Object == nullptr || !Object->IsValidLowLevel())
            return;
        ImGui::Text("%s: %s", Prefix, GetBaseObjectDisplayName(Object).c_str());
        if (!IsUnknownObject(Object))
            return;
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::TextColored(UnknownItemColor, "%s", GetUnknownSuffix(Object));
    }

    void DrawVectorRow(const char* Label, FVector& Value, float Speed = 0.1f)
    {
        ImGui::PushID(Label);
        ImGui::TextUnformatted(Label);
        ImGui::SameLine(120.0f);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat3("##Value", Value.XYZ, Speed);
        ImGui::PopID();
    }

    void DrawRotatorRow(const char* Label, FVector& Value, float Speed = 0.5f)
    {
        FVector EulerDegrees = Value;
        ImGui::PushID(Label);
        ImGui::TextUnformatted(Label);
        ImGui::SameLine(120.0f);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat3("##Value", EulerDegrees.XYZ, Speed))
            Value = EulerDegrees;
        ImGui::PopID();
    }

    FString BuildPropertyLabel(const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        if (!Descriptor.DisplayLabel.empty())
        {
            const int32 ReqSize = WideCharToMultiByte(CP_UTF8, 0, Descriptor.DisplayLabel.c_str(),
                                                      -1, nullptr, 0, nullptr, nullptr);
            std::string Converted(static_cast<size_t>(ReqSize), '\0');
            WideCharToMultiByte(CP_UTF8, 0, Descriptor.DisplayLabel.c_str(), -1, Converted.data(),
                                ReqSize, nullptr, nullptr);
            return FString(Converted.c_str());
        }
        return Descriptor.Key;
    }

    bool TryAcceptAssetPathDrop(const Engine::Component::FComponentPropertyDescriptor& Descriptor,
                                TMap<FString, FString>* AssetPathEditBuffers)
    {
        if (AssetPathEditBuffers == nullptr || !ImGui::BeginDragDropTarget())
            return false;
        bool                bApplied = false;
        const ImGuiPayload* Payload = ImGui::GetDragDropPayload();
        if (Payload != nullptr &&
            Payload->IsDataType(Editor::Content::ContentBrowserAssetPayloadType))
        {
            const auto* DragPayload =
                static_cast<const Editor::Content::FContentBrowserAssetDragDropPayload*>(
                    Payload->Data);
            if (DragPayload != nullptr &&
                (Descriptor.ExpectedAssetPathKind ==
                     Engine::Component::EComponentAssetPathKind::Any ||
                 Descriptor.ExpectedAssetPathKind == DragPayload->AssetKind))
            {
                if (const ImGuiPayload* Accepted = ImGui::AcceptDragDropPayload(
                        Editor::Content::ContentBrowserAssetPayloadType,
                        ImGuiDragDropFlags_AcceptBeforeDelivery))
                {
                    if (Accepted->IsDelivery())
                    {
                        if (Descriptor.StringSetter)
                            Descriptor.StringSetter(DragPayload->VirtualPath);
                        (*AssetPathEditBuffers)[Descriptor.Key] = DragPayload->VirtualPath;
                        bApplied = true;
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
        return bApplied;
    }

    void CollectAssetItemsByType(const FContentBrowserFolderNode&    Folder,
                                 EContentBrowserItemType             TargetType,
                                 TArray<const FContentBrowserItem*>& OutItems)
    {
        for (const auto& File : Folder.Files)
        {
            if (File.ItemType == TargetType)
                OutItems.push_back(&File);
        }
        for (const auto& SubFolder : Folder.ChildFolders)
        {
            CollectAssetItemsByType(SubFolder, TargetType, OutItems);
        }
    }

    bool DrawStaticMeshAssetCombo(const Engine::Component::FComponentPropertyDescriptor& Descriptor,
                                  const FString& CurrentPath, const FString& RawValue,
                                  TMap<FString, FString>* AssetPathEditBuffers,
                                  FEditorContext*         Context)
    {
        bool    bChanged = false;
        FString ComboLabel = CurrentPath.empty() ? "None" : CurrentPath;
        if (ImGui::BeginCombo("##Value", ComboLabel.c_str()))
        {
            // None 옵션 추가
            bool bNoneSelected = RawValue.empty() || RawValue == "None";
            if (ImGui::Selectable("None", bNoneSelected))
            {
                if (Descriptor.StringSetter)
                    Descriptor.StringSetter("");
                if (AssetPathEditBuffers != nullptr)
                    (*AssetPathEditBuffers)[Descriptor.Key] = "";
                bChanged = true;
            }

            if (Context && Context->ContentIndex)
            {
                TArray<const FContentBrowserItem*> MeshItems;
                CollectAssetItemsByType(Context->ContentIndex->GetSnapshot().RootFolder,
                                        EContentBrowserItemType::StaticMesh, MeshItems);

                for (const auto* Item : MeshItems)
                {
                    bool bSelected = (RawValue == Item->VirtualPath);
                    if (ImGui::Selectable(Item->VirtualPath.c_str(), bSelected))
                    {
                        if (Descriptor.StringSetter)
                            Descriptor.StringSetter(Item->VirtualPath);
                        if (AssetPathEditBuffers != nullptr)
                            (*AssetPathEditBuffers)[Descriptor.Key] = Item->VirtualPath;
                        bChanged = true;
                    }
                    if (bSelected)
                        ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return bChanged;
    }

    bool DrawMaterialAssetCombo(Engine::Component::UMeshComponent* MeshComp, uint32 SlotIndex, 
                                FEditorContext* Context)
    {
        if (MeshComp == nullptr || !MeshComp->IsValidLowLevel() || Context == nullptr)
            return false;

        Engine::Asset::UMaterialInterface* CurrentMat = MeshComp->GetMaterial(SlotIndex);

        // 이름 결정 로직 강화
        FString CurrentDisplayName = "None";
        if (CurrentMat && CurrentMat->IsValidLowLevel())
        {
            CurrentDisplayName = CurrentMat->GetAssetName();
            if (CurrentDisplayName.empty())
            {
                // 차선책: 경로에서 파일 이름 추출
                std::filesystem::path Path(CurrentMat->GetPath());
                CurrentDisplayName = Path.filename().string().c_str();
            }
        }

        std::string LabelId = "Material Slot " + std::to_string(SlotIndex + 1);

        bool bChanged = false;
        ImGui::TextUnformatted(LabelId.c_str());
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);

        FString PopupId = FString("##MatCombo_") + std::to_string(SlotIndex);
        if (ImGui::BeginCombo(PopupId.c_str(), CurrentDisplayName.c_str()))
        {
            // None 옵션 추가
            bool bNoneSelected = (CurrentMat == nullptr);
            if (ImGui::Selectable("None", bNoneSelected))
            {
                MeshComp->SetMaterial(SlotIndex, nullptr);
                bChanged = true;
            }

            if (Context->AssetManager)
            {
                TArray<UAsset*> MaterialAssets =
                    Context->AssetManager->GetAssetsByType(EAssetType::Material);

                uint32 Index = 0;
                for (UAsset* Asset : MaterialAssets)
                {
                    auto* Mat = Cast<Engine::Asset::UMaterialInterface>(Asset);
                    if (!Mat)
                    {
                        ++Index;
                        continue;
                    }

                    FString DisplayName = Mat->GetAssetName();
                    if (DisplayName.empty())
                    {
                        DisplayName = "Material";
                    }

                    std::string Label = DisplayName + "##Mat_" + std::to_string(Index);
                    bool bSelected = (CurrentMat == Mat);
                    if (ImGui::Selectable(Label.c_str(), bSelected))
                    {
                        MeshComp->SetMaterial(SlotIndex, Mat);
                        bChanged = true;
                    }
                    ++Index;
                }
            }
            ImGui::EndCombo();
        }

        // --- 머티리얼 세부 속성 조절 UI 추가 ---
        if (CurrentMat && CurrentMat->IsValidLowLevel())
        {
            if (auto* SubMat = Cast<Engine::Asset::UMaterial>(CurrentMat))
            {
                if (auto* Data = SubMat->GetMaterialDataMutable())
                {
                    ImGui::Indent(20.0f);

                    ImGui::TextUnformatted("UV Scroll Speed");
                    ImGui::PushID("UVScrollSpeed");
                    float SpeedX = Data->UVScrollSpeed.X;
                    float SpeedY = Data->UVScrollSpeed.Y;

                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("X:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(70.0f);
                    if (ImGui::DragFloat("##X", &SpeedX, 0.001f, -10.0f, 10.0f, "%.3f"))
                    {
                        Data->UVScrollSpeed.X = SpeedX;
                        bChanged = true;
                    }

                    ImGui::SameLine(150.0f);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Y:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(70.0f);
                    if (ImGui::DragFloat("##Y", &SpeedY, 0.001f, -10.0f, 10.0f, "%.3f"))
                    {
                        Data->UVScrollSpeed.Y = SpeedY;
                        bChanged = true;
                    }

                    ImGui::PopID();
                }
            }
        }

        return bChanged;
    }

    // bool DrawStaticMeshSubMaterialCombo(Engine::Component::UStaticMeshComponent* MeshComp,
    //                                     uint32 SlotIndex)
    // {
    //     if (MeshComp == nullptr || !MeshComp->IsValidLowLevel())
    //         return false;

    //     Engine::Asset::UStaticMesh* StaticMesh = MeshComp->GetStaticMesh();
    //     if (StaticMesh == nullptr)
    //         return false;

    //     const Engine::Asset::FMaterialSlot* CurrentSlot = StaticMesh->GetMaterialSlot(SlotIndex);
    //     if (CurrentSlot == nullptr)
    //         return false;

    //     FString CurrentName =
    //         CurrentSlot->SubMaterialName.empty() ? "None" : CurrentSlot->SubMaterialName;
    //     std::string LabelId = "Material Slot " + std::to_string(SlotIndex + 1);

    //     bool bChanged = false;
    //     ImGui::TextUnformatted(LabelId.c_str());
    //     ImGui::SameLine(140.0f);
    //     ImGui::SetNextItemWidth(-1.0f);

    //     FString PopupId = FString("##SubMatCombo_") + std::to_string(SlotIndex);
    //     if (ImGui::BeginCombo(PopupId.c_str(), CurrentName.c_str()))
    //     {
    //         bool bNoneSelected = CurrentSlot->SubMaterialName.empty();
    //         if (ImGui::Selectable("None", bNoneSelected))
    //         {
    //             StaticMesh->SetMaterialSlot(SlotIndex, CurrentSlot->Material, "");
    //             bChanged = true;
    //         }

    //         const TArray<Engine::Asset::FMaterialSlot>& Slots = StaticMesh->GetMaterialSlots();
    //         for (const auto& Slot : Slots)
    //         {
    //             if (Slot.SubMaterialName.empty())
    //                 continue;

    //             bool bSelected = (CurrentSlot->SubMaterialName == Slot.SubMaterialName);
    //             if (ImGui::Selectable(Slot.SubMaterialName.c_str(), bSelected))
    //             {
    //                 StaticMesh->SetMaterialSlot(SlotIndex, CurrentSlot->Material,
    //                                             Slot.SubMaterialName);
    //                 bChanged = true;
    //             }
    //             if (bSelected)
    //                 ImGui::SetItemDefaultFocus();
    //         }

    //         ImGui::EndCombo();
    //     }

    //     return bChanged;
    // }

    bool DrawBoolPropertyRow(const char* LabelId, const char* DisplayLabel,
                             const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        bool Value = Descriptor.BoolGetter ? Descriptor.BoolGetter() : false;
        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        bool bChanged = ImGui::Checkbox("##Value", &Value);
        ImGui::PopID();
        if (bChanged && Descriptor.BoolSetter)
            Descriptor.BoolSetter(Value);
        return bChanged;
    }

    bool DrawIntPropertyRow(const char* LabelId, const char* DisplayLabel,
                            const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        int32 Value = Descriptor.IntGetter ? Descriptor.IntGetter() : 0;
        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);
        bool bChanged = ImGui::DragInt("##Value", &Value, Descriptor.DragSpeed);
        ImGui::PopID();
        if (bChanged && Descriptor.IntSetter)
            Descriptor.IntSetter(Value);
        return bChanged;
    }

    bool DrawFloatPropertyRow(const char* LabelId, const char* DisplayLabel,
                              const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        float Value = Descriptor.FloatGetter ? Descriptor.FloatGetter() : 0.0f;
        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);
        bool bChanged = ImGui::DragFloat("##Value", &Value, Descriptor.DragSpeed);
        ImGui::PopID();
        if (bChanged && Descriptor.FloatSetter)
            Descriptor.FloatSetter(Value);
        return bChanged;
    }

    bool DrawStringPropertyRow(const char* LabelId, const char* DisplayLabel,
                               const Engine::Component::FComponentPropertyDescriptor& Descriptor,
                               bool bIsAssetPath, TMap<FString, FString>* AssetPathEditBuffers,
                               FEditorContext* Context)
    {
        const FString Value = Descriptor.StringGetter ? Descriptor.StringGetter() : FString{};
        FString       InputValue = Value;
        if (bIsAssetPath && AssetPathEditBuffers != nullptr)
        {
            FString& CachedInput = (*AssetPathEditBuffers)[Descriptor.Key];
            if (CachedInput.empty() || CachedInput == Value)
                CachedInput = Value;
            InputValue = CachedInput;
        }
        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);
        bool bChanged = false;
        bool bDropped = false;
        if (bIsAssetPath && Descriptor.ExpectedAssetPathKind ==
                                Engine::Component::EComponentAssetPathKind::StaticMeshFile)
        {
            bChanged = DrawStaticMeshAssetCombo(Descriptor, InputValue, Value, AssetPathEditBuffers,
                                                Context);
            bDropped = TryAcceptAssetPathDrop(Descriptor, AssetPathEditBuffers);
        }
        else
        {
            std::array<char, 1024> Buf{};
            const size_t           Len = std::min(Buf.size() - 1, InputValue.size());
            if (Len > 0)
                memcpy(Buf.data(), InputValue.data(), Len);
            Buf[Len] = '\0';
            const ImGuiInputTextFlags Flags =
                bIsAssetPath ? ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_None;
            if (ImGui::InputText("##Value", Buf.data(), Buf.size(), Flags))
            {
                if (Descriptor.StringSetter)
                    Descriptor.StringSetter(Buf.data());
                if (bIsAssetPath && AssetPathEditBuffers != nullptr)
                    (*AssetPathEditBuffers)[Descriptor.Key] = Buf.data();
                bChanged = true;
            }
            if (bIsAssetPath)
                bDropped = TryAcceptAssetPathDrop(Descriptor, AssetPathEditBuffers);
        }
        ImGui::PopID();
        if (bDropped)
            return true;
        return bChanged;
    }

    bool DrawVectorPropertyRow(const char* LabelId, const char* DisplayLabel,
                               const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        FVector Val = Descriptor.VectorGetter ? Descriptor.VectorGetter() : FVector::ZeroVector;
        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);
        bool bChg = ImGui::DragFloat3("##Value", Val.XYZ, Descriptor.DragSpeed);
        ImGui::PopID();
        if (bChg && Descriptor.VectorSetter)
            Descriptor.VectorSetter(Val);
        return bChg;
    }

    bool DrawColorPropertyRow(const char* LabelId, const char* DisplayLabel,
                              const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        FColor Val = Descriptor.ColorGetter ? Descriptor.ColorGetter() : FColor::White();
        float  Col[4] = {Val.r, Val.g, Val.b, Val.a};
        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);
        bool bChg = ImGui::ColorEdit4("##Value", Col);
        ImGui::PopID();
        if (bChg && Descriptor.ColorSetter)
            Descriptor.ColorSetter(FColor(Col[0], Col[1], Col[2], Col[3]));
        return bChg;
    }

    bool DrawMaterialSlotPropertyRow(const char* LabelId, const char* DisplayLabel,
                                     const Engine::Component::FComponentPropertyDescriptor& Descriptor,
                                     FEditorContext* Context)
    {
        const FString CurrentValue = Descriptor.StringGetter ? Descriptor.StringGetter() : "";
        TArray<FString> Options = Descriptor.OptionsGetter ? Descriptor.OptionsGetter()
                                                           : TArray<FString>{};

        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);

        bool bChanged = false;
        const FString ComboLabel = CurrentValue.empty() ? "None" : CurrentValue;
        if (ImGui::BeginCombo("##Value", ComboLabel.c_str()))
        {
            bool bNoneSelected = CurrentValue.empty();
            if (ImGui::Selectable("None", bNoneSelected))
            {
                if (Descriptor.StringSetter)
                    Descriptor.StringSetter("");
                bChanged = true;
            }

            for (int32 OptionIndex = 0; OptionIndex < static_cast<int32>(Options.size()); ++OptionIndex)
            {
                const FString& Name = Options[OptionIndex];
                bool bSelected = (CurrentValue == Name);
                ImGui::PushID(OptionIndex);
                if (ImGui::Selectable(Name.c_str(), bSelected))
                {
                    if (Descriptor.StringSetter)
                        Descriptor.StringSetter(Name);
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        // --- UV Scroll UI 통합 ---
        if (Context && Context->SelectedObject)
        {
            if (auto* MeshComp = Cast<Engine::Component::UMeshComponent>(Context->SelectedObject))
            {
                // Property Key (예: "StaticMeshMaterialSlot_0")에서 인덱스 추출
                int32 SlotIdx = 0;
                size_t UnderscorePos = Descriptor.Key.find_last_of('_');
                if (UnderscorePos != std::string::npos)
                {
                    SlotIdx = std::stoi(Descriptor.Key.substr(UnderscorePos + 1));
                }

                if (auto* CurrentMat = MeshComp->GetMaterial(SlotIdx))
                {
                    if (auto* Mat = Cast<Engine::Asset::UMaterial>(CurrentMat))
                    {
                        auto* Data = Mat->GetMaterialDataMutable();
                        
                        // --- 인스턴스별 개별 조절 로직 (오버라이드 우선) ---
                        FVector2 CurrentSpeed = FVector2::ZeroVector;
                        if (Data)
                        {
                            CurrentSpeed = Data->UVScrollSpeed;
                        }

                        // Indent(20.0f) 제거하여 Material 0 와 정렬을 맞춤
                        
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("UV Scroll Speed");
                        ImGui::SameLine(140.0f); // 수평 정렬 시작

                        float SpeedX = CurrentSpeed.X;
                        float SpeedY = CurrentSpeed.Y;

                        // X 부분
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("X:"); ImGui::SameLine();
                        ImGui::SetNextItemWidth(60.0f);
                        if (ImGui::DragFloat("##X", &SpeedX, 0.001f, -10.0f, 10.0f, "%.3f"))
                        {
                            if (Data)
                            {
                                Data->UVScrollSpeed = FVector2(SpeedX, SpeedY);
                                bChanged = true;
                            }
                        }

                        // Y 부분 (간격 조정: 220 -> 240)
                        ImGui::SameLine(240.0f); 
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("Y:"); ImGui::SameLine();
                        ImGui::SetNextItemWidth(60.0f);
                        if (ImGui::DragFloat("##Y", &SpeedY, 0.001f, -10.0f, 10.0f, "%.3f"))
                        {
                            if (Data)
                            {
                                Data->UVScrollSpeed = FVector2(SpeedX, SpeedY);
                                bChanged = true;
                            }
                        }
                    }
                }
            }
        }

        ImGui::PopID();
        return bChanged;
    }

    bool DrawComponentPropertyRow(const Engine::Component::FComponentPropertyDescriptor& Descriptor,
                                  TMap<FString, FString>* AssetPathEditBuffers,
                                  FEditorContext*         Context)
    {
        const FString LabelText = BuildPropertyLabel(Descriptor);
        const char*   Id = Descriptor.Key.c_str();
        const char*   Label = LabelText.c_str();
        using namespace Engine::Component;
        switch (Descriptor.Type)
        {
        case EComponentPropertyType::Bool:
            return DrawBoolPropertyRow(Id, Label, Descriptor);
        case EComponentPropertyType::Int:
            return DrawIntPropertyRow(Id, Label, Descriptor);
        case EComponentPropertyType::Float:
            return DrawFloatPropertyRow(Id, Label, Descriptor);
        case EComponentPropertyType::String:
            return DrawStringPropertyRow(Id, Label, Descriptor, false, AssetPathEditBuffers,
                                         nullptr);
        case EComponentPropertyType::AssetPath:
            return DrawStringPropertyRow(Id, Label, Descriptor, true, AssetPathEditBuffers,
                                         Context);
        case EComponentPropertyType::MaterialSlot:
            return DrawMaterialSlotPropertyRow(Id, Label, Descriptor, Context);
        case EComponentPropertyType::Vector3:
            return DrawVectorPropertyRow(Id, Label, Descriptor);
        case EComponentPropertyType::Color:
            return DrawColorPropertyRow(Id, Label, Descriptor);
        }
        return false;
    }
} // namespace

const wchar_t* FPropertiesPanel::GetPanelID() const { return L"PropertiesPanel"; }
const wchar_t* FPropertiesPanel::GetDisplayName() const { return L"Details"; }

void FPropertiesPanel::Draw()
{
    if (!ImGui::Begin("Details", nullptr))
    {
        ImGui::End();
        return;
    }

    // 전역 선택 객체 유효성 검사 강화
    if (GetContext() == nullptr || GetContext()->SelectedObject == nullptr ||
        !GetContext()->SelectedObject->IsValidLowLevel())
    {
        CachedTargetComponent = nullptr;
        RenamingComponent = nullptr; // 이름 변경 중인 객체도 초기화
        ResetAssetPathEditState();
        DrawNoSelectionState();
        ImGui::End();
        return;
    }

    if (GetContext()->SelectedActors.size() > 1)
    {
        CachedTargetComponent = nullptr;
        RenamingComponent = nullptr;
        ResetAssetPathEditState();
        DrawMultipleSelectionState();
        ImGui::End();
        return;
    }

    AActor*                             SelActor = nullptr;
    Engine::Component::USceneComponent* TargetComp = ResolveTargetComponent(SelActor);

    // 타겟 컴포넌트 유효성 검사 강화
    if (TargetComp == nullptr || !TargetComp->IsValidLowLevel())
    {
        CachedTargetComponent = nullptr;
        RenamingComponent = nullptr;
        ResetAssetPathEditState();
        DrawUnsupportedSelectionState();
        ImGui::End();
        return;
    }

    SyncEditTransformFromTarget(TargetComp);
    DrawComponentHierarchy(SelActor, TargetComp);
    ImGui::Separator();
    DrawSelectionSummary(SelActor, TargetComp);
    ImGui::Separator();
    DrawTransformEditor(TargetComp);
    ImGui::Separator();
    DrawComponentPropertyEditor(TargetComp);
    ImGui::End();
}

void FPropertiesPanel::SetTarget(const FVector& Loc, const FVector& Rot, const FVector& Sca)
{
    EditLocation = Loc;
    EditRotation = Rot;
    EditScale = Sca;
}

AActor* FPropertiesPanel::ResolveSelectedActor() const
{
    if (GetContext() == nullptr || GetContext()->SelectedObject == nullptr ||
        !GetContext()->SelectedObject->IsValidLowLevel())
        return nullptr;
    if (AActor* Actor = Cast<AActor>(GetContext()->SelectedObject))
        return Actor;
    if (auto* Comp = Cast<Engine::Component::USceneComponent>(GetContext()->SelectedObject))
        return Comp->GetOwnerActor();
    return nullptr;
}

void FPropertiesPanel::SyncEditTransformFromTarget(Engine::Component::USceneComponent* TargetComp)
{
    if (TargetComp == nullptr || !TargetComp->IsValidLowLevel())
    {
        CachedTargetComponent = nullptr;
        ResetAssetPathEditState();
        return;
    }
    const FVector Loc = TargetComp->GetRelativeLocation();
    const FQuat   Rot = TargetComp->GetRelativeQuaternion();
    const FVector Sca = TargetComp->GetRelativeScale3D();
    if (CachedTargetComponent != TargetComp || EditLocation != Loc || EditScale != Sca ||
        !Rot.Equals(FRotator::MakeFromEuler(EditRotation).Quaternion()))
    {
        if (CachedTargetComponent != TargetComp)
            ResetAssetPathEditState();
        SetTarget(Loc, Rot.Euler(), Sca);
        CachedTargetComponent = TargetComp;
    }
}

Engine::Component::USceneComponent*
FPropertiesPanel::ResolveTargetComponent(AActor*& OutActor) const
{
    OutActor = ResolveSelectedActor();
    if (GetContext() == nullptr || GetContext()->SelectedObject == nullptr ||
        !GetContext()->SelectedObject->IsValidLowLevel())
        return nullptr;
    if (auto* Comp = Cast<Engine::Component::USceneComponent>(GetContext()->SelectedObject))
        return Comp;
    if (OutActor != nullptr && OutActor->IsValidLowLevel())
        return OutActor->GetRootComponent();
    return nullptr;
}

void FPropertiesPanel::DrawNoSelectionState() const
{
    ImGui::TextUnformatted("No actor selected.");
}
void FPropertiesPanel::DrawMultipleSelectionState() const
{
    ImGui::Text("%zu actors selected.", GetContext()->SelectedActors.size());
}
void FPropertiesPanel::DrawUnsupportedSelectionState() const
{
    ImGui::TextUnformatted("Selected object has no details view.");
}

void FPropertiesPanel::DrawSelectionSummary(AActor*                             Actor,
                                            Engine::Component::USceneComponent* Comp) const
{
    DrawObjectSummaryLine("Selected", GetContext()->SelectedObject);
    if (Actor && Actor->IsValidLowLevel())
        DrawObjectSummaryLine("Actor", Actor);
    if (Comp && Comp->IsValidLowLevel())
        DrawObjectSummaryLine("Component", Comp);
}

void FPropertiesPanel::ResetAssetPathEditState() { AssetPathEditBuffers.clear(); }

void FPropertiesPanel::StartRenaming(Engine::Component::USceneComponent* InComponent) const
{
    if (InComponent == nullptr || !InComponent->IsValidLowLevel())
        return;
    RenamingComponent = InComponent;
    FString CurrentName = InComponent->Name.ToFString();
    memset(RenameBuffer, 0, sizeof(RenameBuffer));
    memcpy(RenameBuffer, CurrentName.c_str(),
           std::min(sizeof(RenameBuffer) - 1, CurrentName.size()));
    bFocusRenameInput = true;
}

void FPropertiesPanel::DrawComponentHierarchy(AActor*                             Actor,
                                              Engine::Component::USceneComponent* TargetComp) const
{
    ImGui::TextUnformatted("Components");
    ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f);

    // [+] 버튼: 언리얼의 Add Component 버튼 역할을 수행합니다.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign,
                        ImVec2(0.5f, 0.42f)); // 글자를 살짝 위로 올림
    if (ImGui::Button("+", ImVec2(24, 24)))
    {
        ImGui::OpenPopup("AddComponentPopup");
    }
    ImGui::PopStyleVar(2);

    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        if (ImGui::MenuItem("Static Mesh Component"))
        {
            if (Actor != nullptr && Actor->IsValidLowLevel())
            {
                ::Engine::Component::UStaticMeshComponent* NewComp =
                    new ::Engine::Component::UStaticMeshComponent();
                if (NewComp != nullptr)
                {
                    FString UniqueName = GenerateUniqueComponentName(Actor, "StaticMeshComponent");
                    NewComp->Name = UniqueName;
                    Actor->AddOwnedComponent(NewComp);

                    if (GetContext() && GetContext()->Editor)
                    {
                        GetContext()->Editor->SetSelectedObject(NewComp);
                        GetContext()->Editor->MarkSceneDirty();
                    }
                    StartRenaming(NewComp);
                }
            }
        }
        ImGui::EndPopup();
    }

    if (Actor == nullptr || !Actor->IsValidLowLevel())
        return;
    const TArray<Engine::Component::USceneComponent*>& Owned = Actor->GetOwnedComponents();
    if (Owned.empty())
        return;
    DrawComponentNode(Actor, Actor->GetRootComponent(), TargetComp);
}

void FPropertiesPanel::DrawComponentNode(AActor* Actor, Engine::Component::USceneComponent* Comp,
                                         Engine::Component::USceneComponent* TargetComp) const
{
    if (Comp == nullptr || !Comp->IsValidLowLevel() || !IsComponentOwnedByActor(Actor, Comp) ||
        !Comp->ShouldShowInDetailsTree())
        return;

    ImGui::PushID(Comp);

    if (TargetComp == Comp)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_F2))
        {
            StartRenaming(Comp);
        }
    }

    bool bOpen = false;
    bool bHasChildren = false;
    for (auto* Child : Comp->GetAttachChildren())
    {
        if (Child && Child->IsValidLowLevel() && Child->ShouldShowInDetailsTree())
        {
            bHasChildren = true;
            break;
        }
    }

    // 이름 변경 모드 렌더링
    if (RenamingComponent == Comp)
    {
        // 렌더링 전 유효성 재확인
        if (!Comp->IsValidLowLevel())
        {
            RenamingComponent = nullptr;
            ImGui::PopID();
            return;
        }

        ImGui::AlignTextToFramePadding();
        if (bHasChildren)
        {
            ImGui::TreeNodeEx("##Dummy",
                              ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                  ImGuiTreeNodeFlags_SpanAvailWidth,
                              "");
            ImGui::SameLine();
        }
        else
        {
            ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
        }

        if (bFocusRenameInput)
        {
            ImGui::SetKeyboardFocusHere();
            bFocusRenameInput = false;
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##RenameInput", RenameBuffer, sizeof(RenameBuffer),
                             ImGuiInputTextFlags_EnterReturnsTrue |
                                 ImGuiInputTextFlags_AutoSelectAll))
        {
            Comp->Name = RenameBuffer;
            RenamingComponent = nullptr;
            if (GetContext() && GetContext()->Editor)
                GetContext()->Editor->MarkSceneDirty();
        }

        if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit())
        {
            RenamingComponent = nullptr;
        }
    }
    else
    {
        ImGuiTreeNodeFlags Flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
        if (TargetComp == Comp)
            Flags |= ImGuiTreeNodeFlags_Selected;
        if (!bHasChildren)
            Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        FString LabelStr = GetBaseObjectDisplayName(Comp);
        if (IsUnknownObject(Comp))
            ImGui::PushStyleColor(ImGuiCol_Text, UnknownItemColor);

        bOpen = ImGui::TreeNodeEx("##Node", Flags, "%s", LabelStr.c_str());

        if (IsUnknownObject(Comp))
            ImGui::PopStyleColor();

        // 우클릭 컨텍스트 메뉴
        if (ImGui::BeginPopupContextItem("CompCtx"))
        {
            if (ImGui::MenuItem("Rename", "F2"))
            {
                StartRenaming(Comp);
            }
            if (ImGui::MenuItem("Remove", "Delete", false, Comp != Actor->GetRootComponent()))
            {
                // 삭제 전 모든 캐시 초기화 (댕글링 포인터 방지 핵심 코드)
                if (RenamingComponent == Comp)
                    RenamingComponent = nullptr;
                if (CachedTargetComponent == Comp)
                    CachedTargetComponent = nullptr;

                Actor->RemoveOwnedComponent(Comp);
                if (GetContext() && GetContext()->Editor)
                {
                    GetContext()->Editor->SetSelectedObject(Actor);
                    GetContext()->Editor->MarkSceneDirty();
                }
            }
            ImGui::EndPopup();
        }

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            if (TargetComp == Comp)
            {
                if (!ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    StartRenaming(Comp);
                }
            }
            else if (GetContext() && GetContext()->Editor)
            {
                GetContext()->Editor->SetSelectedObject(Comp);
            }
        }
    }

    if (bHasChildren && bOpen)
    {
        for (auto* Child : Comp->GetAttachChildren())
            DrawComponentNode(Actor, Child, TargetComp);
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void FPropertiesPanel::DrawTransformEditor(Engine::Component::USceneComponent* Comp)
{
    if (Comp == nullptr || !Comp->IsValidLowLevel())
        return;

    ImGui::TextUnformatted("Transform");
    DrawVectorRow("Location", EditLocation);
    DrawRotatorRow("Rotation", EditRotation);
    DrawVectorRow("Scale", EditScale, 0.01f);
    if (ImGui::Button("Reset Transform"))
    {
        EditLocation = FVector::ZeroVector;
        EditRotation = FVector::ZeroVector;
        EditScale = FVector::OneVector;
    }
    bool bMod = false;
    if (Comp->GetRelativeLocation() != EditLocation)
    {
        Comp->SetRelativeLocation(EditLocation);
        bMod = true;
    }
    if (!Comp->GetRelativeQuaternion().Equals(FRotator::MakeFromEuler(EditRotation).Quaternion()))
    {
        Comp->SetRelativeRotation(FRotator::MakeFromEuler(EditRotation));
        bMod = true;
    }
    if (Comp->GetRelativeScale3D() != EditScale)
    {
        Comp->SetRelativeScale3D(EditScale);
        bMod = true;
    }
    if (bMod && GetContext() && GetContext()->Editor)
        GetContext()->Editor->MarkSceneDirty();
}

    void FPropertiesPanel::DrawComponentPropertyEditor(Engine::Component::USceneComponent* TargetComp)
    {
        if (TargetComp == nullptr || !TargetComp->IsValidLowLevel())
            return;

        bool bMod = false;

    //if (TargetComp->IsA(Engine::Component::UMeshComponent::GetClass()))
    //{
    //    Engine::Component::UMeshComponent* MeshComp =
    //        (Engine::Component::UMeshComponent*)TargetComp;
    //    if (MeshComp->GetNumMaterials() > 0)
    //    {
    //        ImGui::TextUnformatted("Materials");
    //        for (uint32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
    //        {
    //            ImGui::PushID(i);
    //            // if (auto* StaticMeshComp =
    //            //         Cast<Engine::Component::UStaticMeshComponent>(MeshComp))
    //            // {
    //            //     if (DrawStaticMeshSubMaterialCombo(StaticMeshComp, i))
    //            //         bMod = true;
    //            // }
    //            // else
    //            // {
    //            // }
    //            if (DrawMaterialAssetCombo(MeshComp, i, GetContext()))
    //                bMod = true;
    //            ImGui::PopID();
    //        }
    //        ImGui::Separator();
    //    }
    //}

    Engine::Component::FComponentPropertyBuilder Builder;
    TargetComp->DescribeProperties(Builder);
    ImGui::TextUnformatted("Properties");
    for (const auto& Desc : Builder.GetProperties())
    {
        if (!Desc.bExposeInDetails)
            continue;
        if (DrawComponentPropertyRow(Desc, &AssetPathEditBuffers, GetContext()))
        {
            bMod = true;
            if (Desc.Type == Engine::Component::EComponentPropertyType::AssetPath && GetContext() &&
                GetContext()->AssetManager)
                TargetComp->ResolveAssetReferences(GetContext()->AssetManager);
        }
    }
    if (bMod && GetContext() && GetContext()->Editor)
        GetContext()->Editor->MarkSceneDirty();
}
