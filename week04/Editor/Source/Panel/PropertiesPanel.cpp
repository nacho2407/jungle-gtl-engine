#include "PropertiesPanel.h"

// 기초 에셋 헤더를 최상단에 배치하여 불완전 형식 에러 방지
#include "Asset/MaterialInterface.h"
#include "Asset/StaticMesh.h"

// 컴포넌트 헤더 배치
#include "Engine/Component/Core/MeshComponent.h"
#include "Engine/Component/Mesh/StaticMeshComponent.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "Engine/Component/Core/UnknownComponent.h"

#include "Content/ContentBrowserDragDrop.h"
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

extern ENGINE_API TArray<UObject*> GUObjectArray;

namespace
{
    constexpr ImVec4 UnknownItemColor = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);

    FString GetBaseObjectDisplayName(const UObject* Object)
    {
        if (Object == nullptr)
            return {};
        FString NameStr = Object->Name.IsValid() ? Object->Name.ToFString() : "";
        if (NameStr.empty() || NameStr == "None")
        {
            const char* RawTypeName = Object->GetTypeName();
            if ((RawTypeName[0] == 'U' || RawTypeName[0] == 'A') && isupper(RawTypeName[1]))
                return FString(RawTypeName + 1);
            return FString(RawTypeName);
        }
        return NameStr;
    }

    bool IsUnknownObject(const UObject* Object)
    {
        if (Object == nullptr)
            return false;
        return Object->IsA(AUnknownActor::GetClass()) ||
               Object->IsA(Engine::Component::UUnknownComponent::GetClass());
    }

    const char* GetUnknownSuffix(const UObject* Object)
    {
        if (Object == nullptr)
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
        return Actor != nullptr && Component != nullptr && Component->GetOwnerActor() == Actor;
    }

    bool ShouldShowComponentInDetailsTree(const AActor*                             Actor,
                                          const Engine::Component::USceneComponent* Component)
    {
        return IsComponentOwnedByActor(Actor, Component) && Component != nullptr &&
               Component->ShouldShowInDetailsTree();
    }

    /**
     * 액터 내에서 중복되지 않는 고유한 컴포넌트 이름을 생성합니다.
     * 기본적으로 "BaseName 1", "BaseName 2"와 같은 형식으로 숫자를 증가시키며 확인합니다.
     * 
     * @param Actor 컴포넌트를 소유한 액터입니다.
     * @param BaseName 기본이 되는 이름 문자열입니다.
     * @return 중복되지 않는 새로운 고유 이름입니다.
     */
    FString GenerateUniqueComponentName(AActor* Actor, const FString& BaseName)
    {
        if (Actor == nullptr)
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
                if (Comp && Comp->Name.ToFString() == FinalName)
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
        if (Object == nullptr)
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

    bool DrawStaticMeshAssetCombo(const Engine::Component::FComponentPropertyDescriptor& Descriptor,
                                  const FString& CurrentPath, const FString& RawValue,
                                  TMap<FString, FString>* AssetPathEditBuffers)
    {
        bool    bChanged = false;
        FString ComboLabel = CurrentPath.empty() ? "None" : CurrentPath;
        if (ImGui::BeginCombo("##Value", ComboLabel.c_str()))
        {
            for (UObject* Obj : UObject::GetGlobalUObjectArray())
            {
                if (Obj == nullptr)
                    continue;
                if (UStaticMesh* MeshAsset = Cast<UStaticMesh>(Obj))
                {
                    const FString Path = MeshAsset->GetAssetPathFileName();
                    FString       Label = MeshAsset->GetAssetName();
                    if (MeshAsset->bIsBaked)
                        Label += " [Baked]";
                    bool bSelected = (RawValue == Path);
                    if (ImGui::Selectable(Label.c_str(), bSelected))
                    {
                        if (Descriptor.StringSetter)
                            Descriptor.StringSetter(Path);
                        if (AssetPathEditBuffers != nullptr)
                            (*AssetPathEditBuffers)[Descriptor.Key] = Path;
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

    /** 머티리얼 선택 드롭다운 UI (인식 오류 방지를 위해 명시적 타입 사용) */
    bool DrawMaterialAssetCombo(Engine::Component::UMeshComponent* MeshComp, uint32 SlotIndex)
    {
        if (MeshComp == nullptr) return false;

        Engine::Asset::UMaterialInterface* CurrentMat = MeshComp->GetMaterial(SlotIndex);
        FString CurrentPath = CurrentMat ? CurrentMat->GetAssetName() : "None";
        std::string LabelId = "Material Slot " + std::to_string(SlotIndex + 1);
        
        bool bChanged = false;
        ImGui::TextUnformatted(LabelId.c_str());
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);

        FString PopupId = FString("##MatCombo_") + std::to_string(SlotIndex);
        if (ImGui::BeginCombo(PopupId.c_str(), CurrentPath.c_str()))
        {
            for (TUObjectIterator<Engine::Asset::UMaterialInterface> It; It; ++It)
            {
                Engine::Asset::UMaterialInterface* MatAsset = *It;
                if (MatAsset == nullptr) continue;

                bool bSelected = (CurrentMat == MatAsset);
                if (ImGui::Selectable(MatAsset->GetAssetName().c_str(), bSelected))
                {
                    MeshComp->SetMaterial(SlotIndex, MatAsset);
                    bChanged = true;
                }
            }
            ImGui::EndCombo();
        }
        return bChanged;
    }

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
                               bool bIsAssetPath, TMap<FString, FString>* AssetPathEditBuffers)
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
            bChanged =
                DrawStaticMeshAssetCombo(Descriptor, InputValue, Value, AssetPathEditBuffers);
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

    bool DrawComponentPropertyRow(const Engine::Component::FComponentPropertyDescriptor& Descriptor,
                                  TMap<FString, FString>* AssetPathEditBuffers)
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
            return DrawStringPropertyRow(Id, Label, Descriptor, false, AssetPathEditBuffers);
        case EComponentPropertyType::AssetPath:
            return DrawStringPropertyRow(Id, Label, Descriptor, true, AssetPathEditBuffers);
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
    if (GetContext() == nullptr || GetContext()->SelectedObject == nullptr)
    {
        CachedTargetComponent = nullptr;
        ResetAssetPathEditState();
        DrawNoSelectionState();
        ImGui::End();
        return;
    }
    if (GetContext()->SelectedActors.size() > 1)
    {
        CachedTargetComponent = nullptr;
        ResetAssetPathEditState();
        DrawMultipleSelectionState();
        ImGui::End();
        return;
    }
    AActor*                             SelActor = nullptr;
    Engine::Component::USceneComponent* TargetComp = ResolveTargetComponent(SelActor);
    if (TargetComp == nullptr)
    {
        CachedTargetComponent = nullptr;
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
    if (GetContext() == nullptr || GetContext()->SelectedObject == nullptr)
        return nullptr;
    if (AActor* Actor = Cast<AActor>(GetContext()->SelectedObject))
        return Actor;
    if (auto* Comp = Cast<Engine::Component::USceneComponent>(GetContext()->SelectedObject))
        return Comp->GetOwnerActor();
    return nullptr;
}

void FPropertiesPanel::SyncEditTransformFromTarget(Engine::Component::USceneComponent* TargetComp)
{
    if (TargetComp == nullptr)
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
    if (GetContext() == nullptr || GetContext()->SelectedObject == nullptr)
        return nullptr;
    if (auto* Comp = Cast<Engine::Component::USceneComponent>(GetContext()->SelectedObject))
        return Comp;
    if (OutActor != nullptr)
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
    if (Actor)
        DrawObjectSummaryLine("Actor", Actor);
    if (Comp)
        DrawObjectSummaryLine("Component", Comp);
}

void FPropertiesPanel::ResetAssetPathEditState() { AssetPathEditBuffers.clear(); }

void FPropertiesPanel::StartRenaming(Engine::Component::USceneComponent* InComponent) const
{
    if (InComponent == nullptr)
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
        // Static Mesh Component 추가 (발제 핵심 기능)
        if (ImGui::MenuItem("Static Mesh Component"))
        {
            if (Actor != nullptr)
            {
                ::Engine::Component::UStaticMeshComponent* NewComp =
                    new ::Engine::Component::UStaticMeshComponent();
                if (NewComp != nullptr)
                {
                    // 고유 이름 생성 (예: StaticMeshComponent 1)
                    FString UniqueName = GenerateUniqueComponentName(Actor, "StaticMeshComponent");
                    NewComp->Name = UniqueName;

                    // AActor::AddOwnedComponent 내부에서 자동으로 Root에 부착 처리됨
                    Actor->AddOwnedComponent(NewComp);

                    // 생성 후 즉시 선택 및 씬 변경 표시
                    if (GetContext() && GetContext()->Editor)
                    {
                        GetContext()->Editor->SetSelectedObject(NewComp);
                        GetContext()->Editor->MarkSceneDirty();
                    }

                    // 생성 즉시 이름 변경 모드 진입
                    StartRenaming(NewComp);
                }
            }
        }

        // 추후 다른 기능성 컴포넌트(Light, Camera 등) 추가 지점

        ImGui::EndPopup();
    }

    if (Actor == nullptr)
        return;
    const TArray<Engine::Component::USceneComponent*>& Owned = Actor->GetOwnedComponents();
    if (Owned.empty())
        return;
    DrawComponentNode(Actor, Actor->GetRootComponent(), TargetComp);
}

void FPropertiesPanel::DrawComponentNode(AActor* Actor, Engine::Component::USceneComponent* Comp,
                                         Engine::Component::USceneComponent* TargetComp) const
{
    if (!IsComponentOwnedByActor(Actor, Comp) || Comp == nullptr ||
        !Comp->ShouldShowInDetailsTree())
        return;

    ImGui::PushID(Comp);

    // 이름 변경 트리거 감지 (선택된 상태에서 F2 또는 이미 선택된 항목 다시 클릭)
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
        if (Child && Child->ShouldShowInDetailsTree())
        {
            bHasChildren = true;
            break;
        }
    }

    // 이름 변경 모드 렌더링
    if (RenamingComponent == Comp)
    {
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

        if (ImGui::IsItemDeactivated() &&
            !ImGui::IsItemDeactivatedAfterEdit()) // 포커스 잃었을 때 (수정 없이)
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
                Actor->RemoveOwnedComponent(Comp);
                if (GetContext() && GetContext()->Editor)
                {
                    GetContext()->Editor->SetSelectedObject(Actor);
                    GetContext()->Editor->MarkSceneDirty();
                }
            }
            ImGui::EndPopup();
        }

        // 클릭 이벤트 처리 (선택 및 느린 클릭 이름 변경)
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            if (TargetComp == Comp) // 이미 선택된 항목을 클릭했을 때 (느린 클릭)
            {
                // 더블 클릭이 아닐 때만 이름 변경 시도 (더블 클릭은 트리 노드 열기/닫기용)
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
    if (TargetComp == nullptr)
        return;

    bool bMod = false;

    // 렌더링 가능한 메시 컴포넌트인지 확인하고, 슬롯이 존재할 때만 Materials 섹션 표시
    if (TargetComp->IsA(Engine::Component::UMeshComponent::GetClass()))
    {
        Engine::Component::UMeshComponent* MeshComp =
            (Engine::Component::UMeshComponent*)TargetComp;
        if (MeshComp->GetNumMaterials() > 0)
        {
            ImGui::TextUnformatted("Materials");
            for (uint32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
            {
                ImGui::PushID(i);
                if (DrawMaterialAssetCombo(MeshComp, i))
                    bMod = true;
                ImGui::PopID();
            }
            ImGui::Separator();
        }
    }

    // 일반 속성 편집기
    Engine::Component::FComponentPropertyBuilder Builder;
    TargetComp->DescribeProperties(Builder);
    ImGui::TextUnformatted("Properties");
    for (const auto& Desc : Builder.GetProperties())
    {
        if (!Desc.bExposeInDetails)
            continue;
        if (DrawComponentPropertyRow(Desc, &AssetPathEditBuffers))
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
