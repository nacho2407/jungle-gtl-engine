#include "PropertiesPanel.h"

#include "Content/ContentBrowserDragDrop.h"
#include "Editor/Editor.h"
#include "Editor/EditorContext.h"
#include "CoreUObject/Object.h"
#include "Core/Misc/Name.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "Engine/Component/Core/SceneComponent.h"
#include "Engine/Component/Core/UnknownComponent.h"
#include "Engine/Game/Actor.h"
#include "Engine/Game/UnknownActor.h"
#include "SceneIO/SceneAssetPath.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace
{
    constexpr ImVec4 UnknownItemColor = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);

    FString GetBaseObjectDisplayName(const UObject* Object)
    {
        if (Object == nullptr)
        {
            return {};
        }

        if (Object->Name.IsValid())
        {
            return Object->Name.ToFString();
        }

        return "Unnamed";
    }

    bool IsUnknownObject(const UObject* Object)
    {
        if (Object == nullptr)
        {
            return false;
        }

        return Object->IsA(AUnknownActor::GetClass()) ||
            Object->IsA(Engine::Component::UUnknownComponent::GetClass());
    }

    const char* GetUnknownSuffix(const UObject* Object)
    {
        if (Object == nullptr)
        {
            return nullptr;
        }

        if (Object->IsA(AUnknownActor::GetClass()))
        {
            return "(UnknownActor)";
        }

        if (Object->IsA(Engine::Component::UUnknownComponent::GetClass()))
        {
            return "(UnknownComponent)";
        }

        return nullptr;
    }

    FString GetObjectDisplayName(const UObject* Object)
    {
        FString DisplayName = GetBaseObjectDisplayName(Object);
        if (const char* UnknownSuffix = GetUnknownSuffix(Object))
        {
            DisplayName += " ";
            DisplayName += UnknownSuffix;
        }

        return DisplayName;
    }

    bool IsComponentOwnedByActor(const AActor* Actor,
                                 const Engine::Component::USceneComponent* Component)
    {
        return Actor != nullptr && Component != nullptr && Component->GetOwnerActor() == Actor;
    }

    bool ShouldShowComponentInDetailsTree(const AActor* Actor,
                                          const Engine::Component::USceneComponent* Component)
    {
        return IsComponentOwnedByActor(Actor, Component) &&
               Component != nullptr &&
               Component->ShouldShowInDetailsTree();
    }

    void DrawObjectSummaryLine(const char* Prefix, const UObject* Object)
    {
        if (Object == nullptr)
        {
            return;
        }

        ImGui::Text("%s: %s", Prefix, GetBaseObjectDisplayName(Object).c_str());
        if (!IsUnknownObject(Object))
        {
            return;
        }

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
        {
            // UI는 LH Z-Up 기준 축 회전(X,Y,Z)을 보여 주고,
            // 엔진 내부 Rotator(Pitch,Yaw,Roll) 순서로 다시 변환해 저장합니다.
            Value = EulerDegrees;
        }
        ImGui::PopID();
    }

    std::string WideToUtf8(const FWString& InText)
    {
        if (InText.empty())
        {
            return {};
        }

        const int32 RequiredSize =
            WideCharToMultiByte(CP_UTF8, 0, InText.c_str(), static_cast<int32>(InText.size()),
                                nullptr, 0, nullptr, nullptr);
        if (RequiredSize <= 0)
        {
            return {};
        }

        std::string Converted(static_cast<size_t>(RequiredSize), '\0');
        WideCharToMultiByte(CP_UTF8, 0, InText.c_str(), static_cast<int32>(InText.size()),
                            Converted.data(), RequiredSize, nullptr, nullptr);
        return Converted;
    }

    FString BuildPropertyLabel(const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        if (!Descriptor.DisplayLabel.empty())
        {
            return WideToUtf8(Descriptor.DisplayLabel);
        }

        return Descriptor.Key;
    }

    bool IsCompatibleAssetKind(
        Engine::Component::EComponentAssetPathKind ExpectedKind,
        Engine::Component::EComponentAssetPathKind IncomingKind)
    {
        if (ExpectedKind == Engine::Component::EComponentAssetPathKind::Any)
        {
            return true;
        }

        return ExpectedKind == IncomingKind;
    }

    bool TryAcceptAssetPathDrop(
        const Engine::Component::FComponentPropertyDescriptor& Descriptor,
        TMap<FString, FString>* AssetPathEditBuffers)
    {
        if (AssetPathEditBuffers == nullptr || !ImGui::BeginDragDropTarget())
        {
            return false;
        }

        bool bApplied = false;
        const ImGuiPayload* ActivePayload = ImGui::GetDragDropPayload();
        if (ActivePayload != nullptr &&
            ActivePayload->IsDataType(Editor::Content::ContentBrowserAssetPayloadType))
        {
            const auto* DragPayload =
                static_cast<const Editor::Content::FContentBrowserAssetDragDropPayload*>(
                    ActivePayload->Data);
            if (DragPayload != nullptr &&
                IsCompatibleAssetKind(Descriptor.ExpectedAssetPathKind, DragPayload->AssetKind))
            {
                const ImGuiPayload* AcceptedPayload = ImGui::AcceptDragDropPayload(
                    Editor::Content::ContentBrowserAssetPayloadType,
                    ImGuiDragDropFlags_AcceptBeforeDelivery);
                if (AcceptedPayload != nullptr && AcceptedPayload->IsDelivery())
                {
                    const FString DroppedVirtualPath = DragPayload->VirtualPath;
                    if (Descriptor.StringSetter)
                    {
                        Descriptor.StringSetter(DroppedVirtualPath);
                    }

                    (*AssetPathEditBuffers)[Descriptor.Key] = DroppedVirtualPath;
                    bApplied = true;
                }
            }
        }

        ImGui::EndDragDropTarget();
        return bApplied;
    }

    bool DrawBoolPropertyRow(const char* LabelId, const char* DisplayLabel,
                             const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        bool Value = Descriptor.BoolGetter ? Descriptor.BoolGetter() : false;

        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        const bool bChanged = ImGui::Checkbox("##Value", &Value);
        ImGui::PopID();

        if (bChanged && Descriptor.BoolSetter)
        {
            Descriptor.BoolSetter(Value);
        }

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
        const bool bChanged = ImGui::DragInt("##Value", &Value, Descriptor.DragSpeed);
        ImGui::PopID();

        if (bChanged && Descriptor.IntSetter)
        {
            Descriptor.IntSetter(Value);
        }

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
        const bool bChanged = ImGui::DragFloat("##Value", &Value, Descriptor.DragSpeed);
        ImGui::PopID();

        if (bChanged && Descriptor.FloatSetter)
        {
            Descriptor.FloatSetter(Value);
        }

        return bChanged;
    }

    bool DrawStringPropertyRow(const char* LabelId, const char* DisplayLabel,
                               const Engine::Component::FComponentPropertyDescriptor& Descriptor,
                               bool bIsAssetPath, TMap<FString, FString>* AssetPathEditBuffers)
    {
        const FString Value = Descriptor.StringGetter ? Descriptor.StringGetter() : FString{};

        FString InputValue = Value;
        if (bIsAssetPath && AssetPathEditBuffers != nullptr)
        {
            FString& CachedInput = (*AssetPathEditBuffers)[Descriptor.Key];
            if (CachedInput.empty() || CachedInput == Value)
            {
                CachedInput = Value;
            }

            InputValue = CachedInput;
        }

        std::array<char, 1024> Buffer{};
        const size_t CopyLength = std::min(Buffer.size() - 1, InputValue.size());
        if (CopyLength > 0)
        {
            memcpy(Buffer.data(), InputValue.data(), CopyLength);
        }
        Buffer[CopyLength] = '\0';

        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);
        const ImGuiInputTextFlags InputFlags =
            bIsAssetPath ? ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_None;
        const bool bChanged = ImGui::InputText("##Value", Buffer.data(), Buffer.size(), InputFlags);
        bool bDroppedAssetPath = false;
        if (bIsAssetPath)
        {
            bDroppedAssetPath = TryAcceptAssetPathDrop(Descriptor, AssetPathEditBuffers);
        }
        const bool bHovered = bIsAssetPath && ImGui::IsItemHovered();
        ImGui::PopID();

        if (bDroppedAssetPath)
        {
            return true;
        }

        if (bIsAssetPath && AssetPathEditBuffers != nullptr)
        {
            (*AssetPathEditBuffers)[Descriptor.Key] = Buffer.data();
        }

        if (bChanged && Descriptor.StringSetter)
        {
            Descriptor.StringSetter(Buffer.data());

            if (bIsAssetPath && AssetPathEditBuffers != nullptr)
            {
                (*AssetPathEditBuffers)[Descriptor.Key] = Buffer.data();
            }
        }

        if (bHovered)
        {
            const std::filesystem::path ResolvedPath =
                Engine::SceneIO::ResolveSceneAssetPathToAbsolute(
                    bIsAssetPath && AssetPathEditBuffers != nullptr
                        ? (*AssetPathEditBuffers)[Descriptor.Key]
                        : Value);
            if (!ResolvedPath.empty())
            {
                const std::u8string Utf8Path = ResolvedPath.u8string();
                const FString TooltipText(reinterpret_cast<const char*>(Utf8Path.data()),
                                          Utf8Path.size());
                ImGui::SetTooltip("%s", TooltipText.c_str());
            }
        }

        return bChanged;
    }

    bool DrawVectorPropertyRow(const char* LabelId, const char* DisplayLabel,
                               const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        FVector Value = Descriptor.VectorGetter ? Descriptor.VectorGetter() : FVector::ZeroVector;

        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);
        const bool bChanged = ImGui::DragFloat3("##Value", Value.XYZ, Descriptor.DragSpeed);
        ImGui::PopID();

        if (bChanged && Descriptor.VectorSetter)
        {
            Descriptor.VectorSetter(Value);
        }

        return bChanged;
    }

    bool DrawColorPropertyRow(const char* LabelId, const char* DisplayLabel,
                              const Engine::Component::FComponentPropertyDescriptor& Descriptor)
    {
        FColor Value = Descriptor.ColorGetter ? Descriptor.ColorGetter() : FColor::White();
        float ColorValue[4] = {Value.r, Value.g, Value.b, Value.a};

        ImGui::PushID(LabelId);
        ImGui::TextUnformatted(DisplayLabel);
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);
        const bool bChanged = ImGui::ColorEdit4("##Value", ColorValue);
        ImGui::PopID();

        if (bChanged && Descriptor.ColorSetter)
        {
            Descriptor.ColorSetter(FColor(ColorValue[0], ColorValue[1], ColorValue[2],
                                          ColorValue[3]));
        }

        return bChanged;
    }

    bool DrawComponentPropertyRow(
        const Engine::Component::FComponentPropertyDescriptor& Descriptor,
        TMap<FString, FString>* AssetPathEditBuffers)
    {
        const FString LabelText = BuildPropertyLabel(Descriptor);
        const char* LabelId = Descriptor.Key.c_str();
        const char* DisplayLabel = LabelText.c_str();

        using namespace Engine::Component;

        switch (Descriptor.Type)
        {
        case EComponentPropertyType::Bool:
            return DrawBoolPropertyRow(LabelId, DisplayLabel, Descriptor);
        case EComponentPropertyType::Int:
            return DrawIntPropertyRow(LabelId, DisplayLabel, Descriptor);
        case EComponentPropertyType::Float:
            return DrawFloatPropertyRow(LabelId, DisplayLabel, Descriptor);
        case EComponentPropertyType::String:
            return DrawStringPropertyRow(LabelId, DisplayLabel, Descriptor, false,
                                         AssetPathEditBuffers);
        case EComponentPropertyType::AssetPath:
            return DrawStringPropertyRow(LabelId, DisplayLabel, Descriptor, true,
                                         AssetPathEditBuffers);
        case EComponentPropertyType::Vector3:
            return DrawVectorPropertyRow(LabelId, DisplayLabel, Descriptor);
        case EComponentPropertyType::Color:
            return DrawColorPropertyRow(LabelId, DisplayLabel, Descriptor);
        }

        return false;
    }
} // namespace

const wchar_t* FPropertiesPanel::GetPanelID() const
{
    return L"PropertiesPanel";
}

const wchar_t* FPropertiesPanel::GetDisplayName() const
{
    return L"Details";
}

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

    AActor* SelectedActor = nullptr;
    Engine::Component::USceneComponent* TargetComponent = ResolveTargetComponent(SelectedActor);
    if (TargetComponent == nullptr)
    {
        CachedTargetComponent = nullptr;
        ResetAssetPathEditState();
        DrawUnsupportedSelectionState();
        ImGui::End();
        return;
    }

    SyncEditTransformFromTarget(TargetComponent);
    DrawComponentHierarchy(SelectedActor, TargetComponent);
    ImGui::Separator();
    DrawSelectionSummary(SelectedActor, TargetComponent);
    ImGui::Separator();
    DrawTransformEditor(TargetComponent);
    ImGui::Separator();
    DrawComponentPropertyEditor(TargetComponent);

    ImGui::End();
}

void FPropertiesPanel::SetTarget(const FVector& Location, const FVector& Rotation, const FVector& Scale)
{
    EditLocation = Location;
    EditRotation = Rotation;
    EditScale = Scale;
}

AActor* FPropertiesPanel::ResolveSelectedActor() const
{
    if (GetContext() == nullptr || GetContext()->SelectedObject == nullptr)
    {
        return nullptr;
    }

    if (AActor* SelectedActor = Cast<AActor>(GetContext()->SelectedObject))
    {
        return SelectedActor;
    }

    if (auto* SelectedComponent =
            Cast<Engine::Component::USceneComponent>(GetContext()->SelectedObject))
    {
        return SelectedComponent->GetOwnerActor();
    }

    return nullptr;
}

void FPropertiesPanel::SyncEditTransformFromTarget(
    Engine::Component::USceneComponent* TargetComponent)
{
    if (TargetComponent == nullptr)
    {
        CachedTargetComponent = nullptr;
        ResetAssetPathEditState();
        return;
    }

    const FVector CurrentLocation = TargetComponent->GetRelativeLocation();
    const FQuat CurrentRotation = TargetComponent->GetRelativeQuaternion();
    const FVector CurrentScale = TargetComponent->GetRelativeScale3D();

    const bool bTargetChanged = (CachedTargetComponent != TargetComponent);
    const bool bLocationChangedExternally = (EditLocation != CurrentLocation);
    const bool bScaleChangedExternally = (EditScale != CurrentScale);
    const bool bRotationChangedExternally =
        !CurrentRotation.Equals(FRotator::MakeFromEuler(EditRotation).Quaternion());

    // 회전 편집 중에는 패널이 표시 중인 Euler 값을 유지해야 합니다.
    // 매 프레임 quat -> euler 로 다시 변환하면 Pitch(Y축)가 +/-90 부근에서
    // 등가 회전의 다른 표현으로 튀어 특이점처럼 보이는 현상이 발생합니다.
    if (bTargetChanged || bLocationChangedExternally || bScaleChangedExternally
        || bRotationChangedExternally)
    {
        if (bTargetChanged)
        {
            ResetAssetPathEditState();
        }

        SetTarget(CurrentLocation, CurrentRotation.Euler(), CurrentScale);
        CachedTargetComponent = TargetComponent;
    }
}

Engine::Component::USceneComponent* FPropertiesPanel::ResolveTargetComponent(
    AActor*& OutSelectedActor) const
{
    OutSelectedActor = ResolveSelectedActor();
    if (GetContext() == nullptr || GetContext()->SelectedObject == nullptr)
    {
        return nullptr;
    }

    if (auto* SelectedComponent =
            Cast<Engine::Component::USceneComponent>(GetContext()->SelectedObject))
    {
        return SelectedComponent;
    }

    if (OutSelectedActor != nullptr)
    {
        return OutSelectedActor->GetRootComponent();
    }

    return nullptr;
}

void FPropertiesPanel::DrawNoSelectionState() const
{
    ImGui::TextUnformatted("No actor selected.");
    ImGui::Spacing();
    ImGui::TextWrapped("Select an actor or scene component to edit its transform.");
}

void FPropertiesPanel::DrawMultipleSelectionState() const
{
    const size_t SelectedCount =
        GetContext() != nullptr ? GetContext()->SelectedActors.size() : 0;

    ImGui::Text("%zu actors selected.", SelectedCount);
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Details currently supports a single selected actor or component.");
}

void FPropertiesPanel::DrawUnsupportedSelectionState() const
{
    ImGui::TextUnformatted("Selected object has no details view.");
    ImGui::Spacing();
    ImGui::TextWrapped("Only Actor and SceneComponent selections are supported.");
}

void FPropertiesPanel::DrawSelectionSummary(
    AActor* SelectedActor, Engine::Component::USceneComponent* TargetComponent) const
{
    UObject* SelectedObject = GetContext()->SelectedObject;

    DrawObjectSummaryLine("Selected", SelectedObject);
    if (SelectedActor != nullptr)
    {
        DrawObjectSummaryLine("Actor", SelectedActor);
    }

    if (TargetComponent != nullptr)
    {
        DrawObjectSummaryLine("Component", TargetComponent);
    }
}

void FPropertiesPanel::DrawComponentHierarchy(
    AActor* SelectedActor, Engine::Component::USceneComponent* TargetComponent) const
{
    ImGui::TextUnformatted("Components");

    if (SelectedActor == nullptr)
    {
        ImGui::Spacing();
        ImGui::TextWrapped("Selected object is not owned by an actor.");
        return;
    }

    const TArray<Engine::Component::USceneComponent*>& OwnedComponents =
        SelectedActor->GetOwnedComponents();
    if (OwnedComponents.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("Selected actor has no components.");
        return;
    }

    Engine::Component::USceneComponent* RootComponent = SelectedActor->GetRootComponent();
    if (ShouldShowComponentInDetailsTree(SelectedActor, RootComponent))
    {
        DrawComponentNode(SelectedActor, RootComponent, TargetComponent);
    }

    for (Engine::Component::USceneComponent* Component : OwnedComponents)
    {
        if (Component == nullptr || Component == RootComponent)
        {
            continue;
        }

        if (!ShouldShowComponentInDetailsTree(SelectedActor, Component) ||
            Component->GetAttachParent() != nullptr)
        {
            continue;
        }

        DrawComponentNode(SelectedActor, Component, TargetComponent);
    }
}

void FPropertiesPanel::DrawComponentNode(
    AActor* OwnerActor, Engine::Component::USceneComponent* Component,
    Engine::Component::USceneComponent* TargetComponent) const
{
    if (!ShouldShowComponentInDetailsTree(OwnerActor, Component))
    {
        return;
    }

    bool bHasVisibleChildren = false;
    for (Engine::Component::USceneComponent* ChildComponent : Component->GetAttachChildren())
    {
        if (ShouldShowComponentInDetailsTree(OwnerActor, ChildComponent))
        {
            bHasVisibleChildren = true;
            break;
        }
    }

    ImGuiTreeNodeFlags TreeFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_DefaultOpen;
    if (TargetComponent == Component)
    {
        TreeFlags |= ImGuiTreeNodeFlags_Selected;
    }

    if (!bHasVisibleChildren)
    {
        TreeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    const FString DisplayName = GetBaseObjectDisplayName(Component);

    ImGui::PushID(Component);
    const bool bNodeOpen = ImGui::TreeNodeEx("##ComponentNode", TreeFlags, "%s", DisplayName.c_str());
    const bool bNodeClicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();

    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextDisabled("[%s]", Component->GetTypeName());

    if (IsUnknownObject(Component))
    {
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::TextColored(UnknownItemColor, "%s", GetUnknownSuffix(Component));
    }

    if (bNodeClicked && GetContext() != nullptr && GetContext()->Editor != nullptr)
    {
        GetContext()->Editor->SetSelectedObject(Component);
    }

    if (bHasVisibleChildren && bNodeOpen)
    {
        for (Engine::Component::USceneComponent* ChildComponent : Component->GetAttachChildren())
        {
            if (ShouldShowComponentInDetailsTree(OwnerActor, ChildComponent))
            {
                DrawComponentNode(OwnerActor, ChildComponent, TargetComponent);
            }
        }

        ImGui::TreePop();
    }
    ImGui::PopID();
}

void FPropertiesPanel::DrawTransformEditor(
    Engine::Component::USceneComponent* TargetComponent)
{
    ImGui::TextUnformatted("Transform");
    DrawVectorRow("Location", EditLocation, 0.1f);
    DrawRotatorRow("Rotation", EditRotation, 0.5f);
    DrawVectorRow("Scale", EditScale, 0.01f);

    bool bSceneModified = false;

    if (ImGui::Button("Reset Transform"))
    {
        EditLocation = FVector::ZeroVector;
        EditRotation = FVector::ZeroVector;
        EditScale = FVector::OneVector;
    }

    if (TargetComponent->GetRelativeLocation() != EditLocation)
    {
        TargetComponent->SetRelativeLocation(EditLocation);
        bSceneModified = true;
    }

    if (!TargetComponent->GetRelativeQuaternion().Equals(
            FRotator::MakeFromEuler(EditRotation).Quaternion()))
    {
        TargetComponent->SetRelativeRotation(FRotator::MakeFromEuler(EditRotation));
        bSceneModified = true;
    }

    if (TargetComponent->GetRelativeScale3D() != EditScale)
    {
        TargetComponent->SetRelativeScale3D(EditScale);
        bSceneModified = true;
    }

    if (bSceneModified && GetContext() != nullptr && GetContext()->Editor != nullptr)
    {
        GetContext()->Editor->MarkSceneDirty();
    }
}

void FPropertiesPanel::DrawComponentPropertyEditor(
    Engine::Component::USceneComponent* TargetComponent)
{
    if (TargetComponent == nullptr)
    {
        return;
    }

    Engine::Component::FComponentPropertyBuilder Builder;
    TargetComponent->DescribeProperties(Builder);

    bool bHasVisibleProperty = false;
    bool bSceneModified = false;

    ImGui::TextUnformatted("Properties");
    for (const Engine::Component::FComponentPropertyDescriptor& Descriptor :
         Builder.GetProperties())
    {
        if (!Descriptor.bExposeInDetails)
        {
            continue;
        }

        bHasVisibleProperty = true;
        if (DrawComponentPropertyRow(Descriptor, &AssetPathEditBuffers))
        {
            bSceneModified = true;

            if (Descriptor.Type == Engine::Component::EComponentPropertyType::AssetPath &&
                GetContext() != nullptr && GetContext()->AssetManager != nullptr)
            {
                TargetComponent->ResolveAssetReferences(GetContext()->AssetManager);
            }
        }
    }

    if (!bHasVisibleProperty)
    {
        ImGui::TextDisabled("No component-specific properties.");
    }

    if (bSceneModified && GetContext() != nullptr && GetContext()->Editor != nullptr)
    {
        GetContext()->Editor->MarkSceneDirty();
    }
}

void FPropertiesPanel::ResetAssetPathEditState()
{
    AssetPathEditBuffers.clear();
}
