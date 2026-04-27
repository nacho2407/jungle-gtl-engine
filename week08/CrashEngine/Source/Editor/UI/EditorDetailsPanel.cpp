// 에디터 영역의 세부 동작을 구현합니다.
#include "Editor/UI/EditorDetailsPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "GameFramework/AActor.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Component/AmbientLightComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/LightComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SceneComponent.h"
#include "Core/PropertyTypes.h"
#include "Core/ClassTypes.h"
#include "Resource/ResourceManager.h"
#include "Object/FName.h"
#include "Object/ObjectIterator.h"
#include "Materials/Material.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Platform/Paths.h"

#include <Windows.h>
#include <commdlg.h>
#include <filesystem>
#include <functional>

#include "Materials/MaterialManager.h"
#include "Render/Execute/Passes/Scene/ShadowMapPass.h"
#include "Render/Execute/Registry/RenderPassRegistry.h"
#include "Render/Scene/Proxies/Light/LightProxy.h"

#define SEPARATOR()     \
    ;                   \
    ImGui::Spacing();   \
    ImGui::Spacing();   \
    ImGui::Separator(); \
    ImGui::Spacing();   \
    ImGui::Spacing();

static FString RemoveExtension(const FString& Path)
{
    size_t DotPos = Path.find_last_of('.');
    if (DotPos == FString::npos)
    {
        return Path;
    }
    return Path.substr(0, DotPos);
}

static FString GetStemFromPath(const FString& Path)
{
    size_t SlashPos = Path.find_last_of("/\\");
    FString FileName = (SlashPos == FString::npos) ? Path : Path.substr(SlashPos + 1);
    return RemoveExtension(FileName);
}

static FString BuildComponentDisplayLabel(UActorComponent* Comp)
{
    if (!Comp)
    {
        return "None";
    }

    const FString TypeName = Comp->GetClass()->GetName();
    FString Name = Comp->GetFName().ToString();
    if (Name.empty())
    {
        Name = TypeName;
    }
    return TypeName + "(" + Name + ")";
}

static int32 GetClosestWorldAlignedPointShadowFace(const UEditorEngine* InEditorEngine)
{
    if (!InEditorEngine)
    {
        return 0;
    }

    const auto* ActiveViewport = InEditorEngine->GetActiveViewport();
    const UCameraComponent* ActiveCamera = ActiveViewport ? ActiveViewport->GetCamera() : InEditorEngine->GetCamera();
    if (!ActiveCamera)
    {
        return 0;
    }

    const FVector Forward = ActiveCamera->GetForwardVector().Normalized();
    static const FVector FaceDirections[6] = {
        FVector(1.0f, 0.0f, 0.0f),
        FVector(-1.0f, 0.0f, 0.0f),
        FVector(0.0f, 1.0f, 0.0f),
        FVector(0.0f, -1.0f, 0.0f),
        FVector(0.0f, 0.0f, 1.0f),
        FVector(0.0f, 0.0f, -1.0f),
    };

    int32 BestFace = 0;
    float BestDot = -FLT_MAX;
    for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
    {
        const float DotValue = Forward.Dot(FaceDirections[FaceIndex]);
        if (DotValue > BestDot)
        {
            BestDot = DotValue;
            BestFace = FaceIndex;
        }
    }

    return BestFace;
}

FString FEditorDetailsPanel::OpenObjFileDialog()
{
    wchar_t FilePath[MAX_PATH] = {};

    OPENFILENAMEW Ofn = {};
    Ofn.lStructSize = sizeof(Ofn);
    Ofn.hwndOwner = nullptr;
    Ofn.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
    Ofn.lpstrFile = FilePath;
    Ofn.nMaxFile = MAX_PATH;
    Ofn.lpstrTitle = L"Import OBJ Mesh";
    Ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&Ofn))
    {
        std::filesystem::path AbsPath = std::filesystem::path(FilePath).lexically_normal();
        std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir());
        std::filesystem::path RelPath = AbsPath.lexically_relative(RootPath);

        // 상대 경로 변환 실패 시 (드라이브가 다른 경우 등) 절대 경로를 그대로 반환
        if (RelPath.empty() || RelPath.wstring().starts_with(L".."))
        {
            return FPaths::ToUtf8(AbsPath.generic_wstring());
        }
        return FPaths::ToUtf8(RelPath.generic_wstring());
    }

    return FString();
}

void FEditorDetailsPanel::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);

    ImGui::Begin("Details");

    FSelectionManager& Selection = EditorEngine->GetSelectionManager();
    AActor* PrimaryActor = Selection.GetPrimarySelection();
    if (!PrimaryActor)
    {
        SelectedComponent = nullptr;
        LastSelectedActor = nullptr;
        bActorSelected = true;
        ImGui::TextDisabled("No actor selected.");
        ImGui::End();
        return;
    }

    // Actor 선택이 바뀌면 초기화
    if (PrimaryActor != LastSelectedActor)
    {
        SelectedComponent = nullptr;
        LastSelectedActor = PrimaryActor;
        bActorSelected = true;
    }

    const TArray<AActor*>& SelectedActors = Selection.GetSelectedActors();
    const int32 SelectionCount = static_cast<int32>(SelectedActors.size());

    // ========== 고정 영역: Actor Info ==========
    ImGui::Text("Class: %s", PrimaryActor->GetClass()->GetName());

    FString PrimaryName = PrimaryActor->GetFName().ToString();
    if (PrimaryName.empty())
        PrimaryName = PrimaryActor->GetClass()->GetName();

    if (bActorSelected)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
    if (SelectionCount > 1)
    {
        ImGui::Text("Name: %s (+%d)", PrimaryName.c_str(), SelectionCount - 1);
    }
    else
    {
        ImGui::Text("Name: %s", PrimaryName.c_str());
    }
    if (bActorSelected)
        ImGui::PopStyleColor();
    if (ImGui::IsItemClicked())
    {
        bActorSelected = true;
        SelectedComponent = nullptr;
    }

    // ========== 고정 영역: Component Tree ==========
    SEPARATOR();
    RenderComponentTree(PrimaryActor);

    // ========== 스크롤 영역: Details ==========
    SEPARATOR();
    ImGui::Text("Details");
    ImGui::Separator();

    float ScrollHeight = ImGui::GetContentRegionAvail().y;
    if (ScrollHeight < 50.0f)
        ScrollHeight = 50.0f;

    ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        RenderDetails(PrimaryActor, SelectedActors);
    }
    ImGui::EndChild();

    ImGui::End();
}

void FEditorDetailsPanel::RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
    if (bActorSelected)
    {
        RenderActorProperties(PrimaryActor, SelectedActors);
    }
    else if (SelectedComponent)
    {
        RenderComponentProperties(PrimaryActor);
    }
    else
    {
        ImGui::TextDisabled("Select an actor or component to view details.");
    }
}

void FEditorDetailsPanel::RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
    ImGui::Text("Actor: %s", PrimaryActor->GetClass()->GetName());
    ImGui::Text("Name: %s", PrimaryActor->GetFName().ToString().c_str());

    if (PrimaryActor->GetRootComponent())
    {
        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::Spacing();

        FVector Pos = PrimaryActor->GetActorLocation();
        float PosArray[3] = { Pos.X, Pos.Y, Pos.Z };

        USceneComponent* RootComp = PrimaryActor->GetRootComponent();

        FVector Scale = PrimaryActor->GetActorScale();
        float ScaleArray[3] = { Scale.X, Scale.Y, Scale.Z };

        if (ImGui::DragFloat3("Location", PosArray, 0.1f))
        {
            FVector Delta = FVector(PosArray[0], PosArray[1], PosArray[2]) - Pos;
            for (AActor* Actor : SelectedActors)
            {
                if (Actor)
                    Actor->AddActorWorldOffset(Delta);
            }
            EditorEngine->GetGizmo()->UpdateGizmoTransform();
        }
        {
            // Rotation: CachedEditRotator를 X=Roll(X축), Y=Pitch(Y축), Z=Yaw(Z축)로 노출
            FRotator& CachedRot = RootComp->GetCachedEditRotator();
            FRotator PrevRot = CachedRot;
            float RotXYZ[3] = { CachedRot.Roll, CachedRot.Pitch, CachedRot.Yaw };

            if (ImGui::DragFloat3("Rotation", RotXYZ, 0.1f))
            {
                CachedRot.Roll = RotXYZ[0];
                CachedRot.Pitch = RotXYZ[1];
                CachedRot.Yaw = RotXYZ[2];

                if (SelectedActors.size() > 1)
                {
                    FRotator Delta = CachedRot - PrevRot;
                    for (AActor* Actor : SelectedActors)
                    {
                        if (!Actor || Actor == PrimaryActor)
                            continue;
                        USceneComponent* Root = Actor->GetRootComponent();
                        if (Root)
                        {
                            FRotator Other = Root->GetCachedEditRotator();
                            Root->SetRelativeRotation(Other + Delta);
                        }
                    }
                }
                RootComp->ApplyCachedEditRotator();
                EditorEngine->GetGizmo()->UpdateGizmoTransform();
            }
        }
        if (ImGui::DragFloat3("Scale", ScaleArray, 0.1f))
        {
            FVector Delta = FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]) - Scale;
            for (AActor* Actor : SelectedActors)
            {
                if (Actor)
                    Actor->SetActorScale(Actor->GetActorScale() + Delta);
            }
        }
    }

    ImGui::Separator();
    bool bVisible = PrimaryActor->IsVisible();
    if (ImGui::Checkbox("Visible", &bVisible))
    {
        PrimaryActor->SetVisible(bVisible);
    }
}

void FEditorDetailsPanel::RenderComponentTree(AActor* Actor)
{
    ImGui::Text("Component List");

    // Get All Component Classes
    TArray<UClass*>& AllClasses = UClass::GetAllClasses();

    TArray<UClass*> ComponentClasses;
    for (UClass* Cls : AllClasses)
    {
        if (Cls->IsA(UActorComponent::StaticClass()) && !Cls->HasAnyClassFlags(CF_Abstract))
            ComponentClasses.push_back(Cls);
    }

    static int SelectedIndex = 0;
    if (ComponentClasses.empty())
    {
        SelectedIndex = 0;
    }
    else if (SelectedIndex >= static_cast<int>(ComponentClasses.size()))
    {
        SelectedIndex = static_cast<int>(ComponentClasses.size()) - 1;
    }
    const char* Preview = ComponentClasses.empty() ? "None" : ComponentClasses[SelectedIndex]->GetName();

    USceneComponent* Root = Actor->GetRootComponent();

    if (!ComponentClasses.empty() && ImGui::Button("Add"))
    {
        UActorComponent* Comp = Actor->AddComponentByClass(ComponentClasses[SelectedIndex]);
        if (!Comp)
        {
            return;
        }

        if (ComponentClasses[SelectedIndex]->IsA(USceneComponent::StaticClass()))
        {
            if (SelectedComponent != nullptr && SelectedComponent->GetClass()->IsA(USceneComponent::StaticClass()))
                Cast<USceneComponent>(Comp)->AttachToComponent(Cast<USceneComponent>(SelectedComponent));
            else
                Cast<USceneComponent>(Comp)->AttachToComponent(Root);
        }
    }

    ImGui::SameLine();
    // ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("Type", Preview))
    {
        for (int i = 0; i < (int)ComponentClasses.size(); ++i)
        {
            bool bSelected = (SelectedIndex == i);
            if (ImGui::Selectable(ComponentClasses[i]->GetName(), bSelected))
                SelectedIndex = i;
            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    if (Root)
    {
        RenderSceneComponentNode(Root);
    }

    // Non-scene ActorComponents
    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (!Comp)
            continue;
        if (Comp->IsA<USceneComponent>())
            continue;

        FString Label = BuildComponentDisplayLabel(Comp);

        ImGui::Indent(12.0f);
        ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (!bActorSelected && SelectedComponent == Comp)
            Flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::TreeNodeEx(Comp, Flags, "%s", Label.c_str());
        if (ImGui::IsItemClicked())
        {
            SelectedComponent = Comp;
            bActorSelected = false;
        }
        ImGui::Unindent(12.0f);
    }
}

void FEditorDetailsPanel::RenderSceneComponentNode(USceneComponent* Comp)
{
    if (!Comp)
        return;

    FString Label = BuildComponentDisplayLabel(Comp);

    const auto& Children = Comp->GetChildren();
    bool bHasChildren = !Children.empty();

    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (!bHasChildren)
        Flags |= ImGuiTreeNodeFlags_Leaf;
    if (!bActorSelected && SelectedComponent == Comp)
        Flags |= ImGuiTreeNodeFlags_Selected;

    bool bIsRoot = (Comp->GetParent() == nullptr);
    bool bOpen = ImGui::TreeNodeEx(
        Comp, Flags, "%s%s",
        bIsRoot ? "[Root] " : "",
        Label.c_str());

    if (ImGui::IsItemClicked())
    {
        SelectedComponent = Comp;
        bActorSelected = false;
    }

    if (bOpen)
    {
        for (USceneComponent* Child : Children)
        {
            RenderSceneComponentNode(Child);
        }
        ImGui::TreePop();
    }
}

void FEditorDetailsPanel::RenderComponentProperties(AActor* Actor)
{
    (void)Actor;
    const FString ComponentLabel = BuildComponentDisplayLabel(SelectedComponent);
    ImGui::Text("Component: %s", ComponentLabel.c_str());
    ImGui::Separator();

    TArray<FPropertyDescriptor> Props;
    SelectedComponent->GetEditableProperties(Props);

    auto IsTransformProp = [](const FString& Name)
    {
        return Name == "Location" || Name == "Rotation" || Name == "Scale";
    };
    auto IsStaticMeshProp = [](const FString& Name, EPropertyType Type)
    {
        return Type == EPropertyType::StaticMeshRef || Name == "Static Mesh" || Name == "StaticMesh";
    };
    auto IsMaterialProp = [](const FString& Name, EPropertyType Type)
    {
        return Type == EPropertyType::MaterialSlot || Name.rfind("Element ", 0) == 0;
    };
    auto IsVisibilityProp = [](const FString& Name)
    {
        return Name == "Visible" || Name == "Visible In Editor" || Name == "Visible In Game" || Name == "Is Editor Helper";
    };
    auto IsShadowProp = [](const FString& Name)
    {
        return Name == "bCastShadows"
            || Name == "Bias"
            || Name == "Slope Bias"
            || Name == "Normal Bias"
            || Name == "Cascade Count"
            || Name == "CSM Max Distance"
            || Name == "Cascade Distribution";
    };

    bool bIsRoot = false;
    if (SelectedComponent->IsA<USceneComponent>())
    {
        USceneComponent* SceneComp = static_cast<USceneComponent*>(SelectedComponent);
        bIsRoot = (SceneComp->GetParent() == nullptr);
    }

    if (SelectedComponent->IsA<USceneComponent>() && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (int32 i = 0; i < (int32)Props.size(); ++i)
        {
            if (IsTransformProp(Props[i].Name))
            {
                RenderDetailsPanel(Props, i);
            }
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
    }

    const bool bIsStaticMeshComponent = SelectedComponent->IsA<UStaticMeshComponent>();
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(SelectedComponent);
    if (bIsStaticMeshComponent && ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (int32 i = 0; i < (int32)Props.size(); ++i)
        {
            if (IsStaticMeshProp(Props[i].Name, Props[i].Type))
            {
                bool bChanged = RenderDetailsPanel(Props, i);
                if (bChanged && Props[i].Type == EPropertyType::StaticMeshRef)
                {
                    if (SelectedComponent->IsA<USceneComponent>())
                    {
                        static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
                    }
                    return;
                }
            }
        }
    }

    bool bHasMaterialProp = false;
    for (const FPropertyDescriptor& Prop : Props)
    {
        if (IsMaterialProp(Prop.Name, Prop.Type))
        {
            bHasMaterialProp = true;
            break;
        }
    }

    if (bHasMaterialProp && ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (int32 i = 0; i < (int32)Props.size(); ++i)
        {
            if (IsMaterialProp(Props[i].Name, Props[i].Type))
            {
                RenderDetailsPanel(Props, i);
            }
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
    }

    if (PrimitiveComponent && ImGui::CollapsingHeader("Visibility", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool bVisible = PrimitiveComponent->IsVisible();
        if (ImGui::Checkbox("Visible", &bVisible))
        {
            PrimitiveComponent->SetVisibility(bVisible);
            PrimitiveComponent->PostEditProperty("Visible");
        }

        ImGui::Indent();
        bool bVisibleInEditor = PrimitiveComponent->IsVisibleInEditor();
        bool bVisibleInGame = PrimitiveComponent->IsVisibleInGame();

        if (!PrimitiveComponent->IsVisible())
        {
            bVisibleInEditor = false;
            bVisibleInGame = false;
        }

        ImGui::BeginDisabled(!PrimitiveComponent->IsVisible());
        if (ImGui::Checkbox("Visible In Editor", &bVisibleInEditor))
        {
            PrimitiveComponent->SetVisibleInEditor(bVisibleInEditor);
            PrimitiveComponent->PostEditProperty("Visible In Editor");
        }
        if (ImGui::Checkbox("Visible In Game", &bVisibleInGame))
        {
            PrimitiveComponent->SetVisibleInGame(bVisibleInGame);
            PrimitiveComponent->PostEditProperty("Visible In Game");
        }
        ImGui::EndDisabled();
        ImGui::Unindent();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
    }

    if (ImGui::CollapsingHeader("Details", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ULightComponent* LightComponent = Cast<ULightComponent>(SelectedComponent);
        for (int32 i = 0; i < (int32)Props.size(); ++i)
        {
            if (IsTransformProp(Props[i].Name) || IsStaticMeshProp(Props[i].Name, Props[i].Type) || IsMaterialProp(Props[i].Name, Props[i].Type) || IsVisibilityProp(Props[i].Name))
            {
                continue;
            }
            if (LightComponent && IsShadowProp(Props[i].Name))
            {
                continue;
            }
            RenderDetailsPanel(Props, i);
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
    }

    if (ULightComponent* LightComponent = Cast<ULightComponent>(SelectedComponent))
    {
        RenderLightShadowSettings(LightComponent);
    }

    if (SelectedComponent->IsA<USceneComponent>())
    {
        static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
    }
}

void FEditorDetailsPanel::RenderLightShadowSettings(ULightComponent* LightComponent)
{
    if (!LightComponent || LightComponent->IsA<UAmbientLightComponent>())
    {
        return;
    }

    if (!ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    TArray<FPropertyDescriptor> ShadowProps;
    LightComponent->GetEditableProperties(ShadowProps);

    auto RenderShadowPropertyByName = [&](const char* PropertyName)
    {
        for (int32 PropIndex = 0; PropIndex < static_cast<int32>(ShadowProps.size()); ++PropIndex)
        {
            if (ShadowProps[PropIndex].Name == PropertyName)
            {
                bool bChanged = false;
                if (ShadowProps[PropIndex].Name == "bCastShadows" && ShadowProps[PropIndex].Type == EPropertyType::Bool)
                {
                    bool* ValuePtr = static_cast<bool*>(ShadowProps[PropIndex].ValuePtr);
                    bChanged = ImGui::Checkbox("Cast Shadows", ValuePtr);
                }
                else
                {
                    bChanged = RenderDetailsPanel(ShadowProps, PropIndex);
                }

                if (bChanged)
                {
                    LightComponent->PostEditProperty(PropertyName);
                }
                return true;
            }
        }
        return false;
    };

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    RenderShadowPropertyByName("bCastShadows");
    if (!LightComponent->DoesCastShadows())
    {
        return;
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::SeparatorText("Common");
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    FLightProxy* LightProxy = LightComponent->GetLightProxy();
    if (!LightProxy)
    {
        ImGui::TextDisabled("Light proxy is not ready.");
        return;
    }

    if (!GEngine)
    {
        ImGui::TextDisabled("Renderer is unavailable.");
        return;
    }

    FRenderPass* Pass = GEngine->GetRenderer().GetPassRegistry().FindPass(ERenderPassNodeType::ShadowMapPass);
    FShadowMapPass* ShadowPass = static_cast<FShadowMapPass*>(Pass);
    if (!ShadowPass)
    {
        ImGui::TextDisabled("Shadow pass is unavailable.");
        return;
    }

    static const int32 ShadowMapSizes[] = { 512, 1024, 2048, 4096 };
    static const char* ShadowMapSizeLabels = "512\0""1024\0""2048\0""4096\0\0";
    int32 SelectedShadowMapSizeIndex = 0;
    for (int32 SizeIndex = 0; SizeIndex < IM_ARRAYSIZE(ShadowMapSizes); ++SizeIndex)
    {
        if (ShadowMapSizes[SizeIndex] == static_cast<int32>(ShadowPass->GetShadowMapSize()))
        {
            SelectedShadowMapSizeIndex = SizeIndex;
            break;
        }
    }

    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::Combo("Resolution", &SelectedShadowMapSizeIndex, ShadowMapSizeLabels))
    {
        ShadowPass->SetShadowMapSize(static_cast<uint32>(ShadowMapSizes[SelectedShadowMapSizeIndex]));
    }

    RenderShadowPropertyByName("Bias");
    RenderShadowPropertyByName("Slope Bias");
    RenderShadowPropertyByName("Normal Bias");

    if (UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(LightComponent))
    {
        (void)DirectionalLight;
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::SeparatorText("Cascade Settings");
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        RenderShadowPropertyByName("Cascade Count");
        RenderShadowPropertyByName("CSM Max Distance");
        RenderShadowPropertyByName("Cascade Distribution");
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::SeparatorText("Shadow Map Preview");
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (LightProxy->ShadowMapIndex < 0)
    {
        ImGui::TextDisabled("No shadow map assigned this frame.");
        return;
    }

    uint32 PreviewFace = 0;
    if (LightProxy->LightProxyInfo.LightType == static_cast<uint32>(ELightType::Point))
    {
        static const char* FaceLabels[6] = { "World +X", "World -X", "World +Y", "World -Y", "World +Z", "World -Z" };
        static const char* FaceButtonLabels[6] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
        const int32 ClosestFace = GetClosestWorldAlignedPointShadowFace(EditorEngine);

        ImGui::TextDisabled("Point light shadow faces are world-aligned.");
        ImGui::TextDisabled("Closest to current view: %s", FaceLabels[ClosestFace]);
        ImGui::SameLine();
        if (ImGui::Button("Use Closest Face"))
        {
            SelectedPointLightShadowFace = ClosestFace;
        }

        static const int32 FaceButtonOrder[6] = { 0, 1, 2, 3, 4, 5 };
        const float AvailableWidth = ImGui::GetContentRegionAvail().x;
        const float ButtonSpacing = ImGui::GetStyle().ItemSpacing.x;
        const float FaceButtonWidth = std::max(64.0f, (AvailableWidth - ButtonSpacing) / 2.0f);

        ImGui::Text("Face");
        for (int32 ButtonIndex = 0; ButtonIndex < 6; ++ButtonIndex)
        {
            if ((ButtonIndex % 2) != 0)
            {
                ImGui::SameLine();
            }

            const int32 FaceIndex = FaceButtonOrder[ButtonIndex];
            const bool bSelectedFace = (SelectedPointLightShadowFace == FaceIndex);
            if (bSelectedFace)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }

            if (ImGui::Button(FaceButtonLabels[FaceIndex], ImVec2(FaceButtonWidth, 0.0f)))
            {
                SelectedPointLightShadowFace = FaceIndex;
            }

            if (bSelectedFace)
            {
                ImGui::PopStyleColor(2);
            }
        }

        PreviewFace = static_cast<uint32>(std::clamp(SelectedPointLightShadowFace, 0, 5));
    }

    ID3D11DeviceContext* Context = GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();
    ID3D11ShaderResourceView* PreviewSRV =
        ShadowPass->GetShadowPreviewSRV(static_cast<uint32>(LightProxy->ShadowMapIndex), PreviewFace, Context);
    if (!PreviewSRV)
    {
        ImGui::TextDisabled("Shadow preview is unavailable.");
        return;
    }

    const float MaxWidth = ImGui::GetContentRegionAvail().x;
    const float PreviewSize = (MaxWidth > 0.0f) ? std::min(MaxWidth, 256.0f) : 256.0f;

    const ImVec2 PreviewMin = ImGui::GetCursorScreenPos();
    const ImVec2 PreviewMax = ImVec2(PreviewMin.x + PreviewSize, PreviewMin.y + PreviewSize);
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    DrawList->AddImageQuad(
        reinterpret_cast<ImTextureID>(PreviewSRV),
        PreviewMin,
        ImVec2(PreviewMax.x, PreviewMin.y),
        PreviewMax,
        ImVec2(PreviewMin.x, PreviewMax.y),
        ImVec2(0.0f, 1.0f),
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 0.0f),
        ImVec2(1.0f, 1.0f));
    ImGui::Dummy(ImVec2(PreviewSize, PreviewSize));

    ImGui::TextDisabled("Shadow Map Index: %d", LightProxy->ShadowMapIndex);
}

bool FEditorDetailsPanel::RenderDetailsPanel(TArray<FPropertyDescriptor>& Props, int32& Index)
{
    ImGui::PushID(Index);
    FPropertyDescriptor& Prop = Props[Index];
    bool bChanged = false;

    switch (Prop.Type)
    {
    case EPropertyType::Bool:
    {
        bool* Val = static_cast<bool*>(Prop.ValuePtr);
        bChanged = ImGui::Checkbox(Prop.Name.c_str(), Val);
        break;
    }
    case EPropertyType::ByteBool:
    {
        uint8* Val = static_cast<uint8*>(Prop.ValuePtr);
        bool bVal = (*Val != 0);
        if (ImGui::Checkbox(Prop.Name.c_str(), &bVal))
        {
            *Val = bVal ? 1 : 0;
            bChanged = true;
        }
        break;
    }
    case EPropertyType::Int:
    {
        int32* Val = static_cast<int32*>(Prop.ValuePtr);
        bChanged = ImGui::DragInt(Prop.Name.c_str(), Val);
        break;
    }
    case EPropertyType::Float:
    {
        float* Val = static_cast<float*>(Prop.ValuePtr);
        if (Prop.Min != 0.0f || Prop.Max != 0.0f)
            bChanged = ImGui::DragFloat(Prop.Name.c_str(), Val, Prop.Speed, Prop.Min, Prop.Max);
        else
            bChanged = ImGui::DragFloat(Prop.Name.c_str(), Val, Prop.Speed);
        break;
    }
    case EPropertyType::Vec3:
    {
        float* Val = static_cast<float*>(Prop.ValuePtr);
        bChanged = ImGui::DragFloat3(Prop.Name.c_str(), Val, Prop.Speed);
        break;
    }
    case EPropertyType::Rotator:
    {
        // FRotator 메모리 레이아웃 [Pitch,Yaw,Roll] → UI X=Roll(X축), Y=Pitch(Y축), Z=Yaw(Z축)
        FRotator* Rot = static_cast<FRotator*>(Prop.ValuePtr);
        float RotXYZ[3] = { Rot->Roll, Rot->Pitch, Rot->Yaw };
        bChanged = ImGui::DragFloat3(Prop.Name.c_str(), RotXYZ, Prop.Speed);
        if (bChanged)
        {
            Rot->Roll = RotXYZ[0];
            Rot->Pitch = RotXYZ[1];
            Rot->Yaw = RotXYZ[2];
            if (SelectedComponent && SelectedComponent->IsA<USceneComponent>())
            {
                static_cast<USceneComponent*>(SelectedComponent)->ApplyCachedEditRotator();
            }
        }
        break;
    }
    case EPropertyType::Vec4:
    {
        float* Val = static_cast<float*>(Prop.ValuePtr);
        bChanged = ImGui::DragFloat4(Prop.Name.c_str(), Val, Prop.Speed);
        break;
    }
    case EPropertyType::Color4:
    {
        float* Val = static_cast<float*>(Prop.ValuePtr);
        bChanged = ImGui::ColorEdit4(Prop.Name.c_str(), Val);
        break;
    }
    case EPropertyType::String:
    {
        FString* Val = static_cast<FString*>(Prop.ValuePtr);
        char Buf[256];
        strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
        if (ImGui::InputText(Prop.Name.c_str(), Buf, sizeof(Buf)))
        {
            *Val = Buf;
            bChanged = true;
        }
        break;
    }
    case EPropertyType::StaticMeshRef:
    {
        FString* Val = static_cast<FString*>(Prop.ValuePtr);

        FString Preview = Val->empty() ? "None" : GetStemFromPath(*Val);
        if (*Val == "None")
            Preview = "None";

        ImGui::Text("%s", Prop.Name.c_str());
        ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1.0f);

        float ButtonWidth = ImGui::CalcTextSize("Import OBJ").x + ImGui::GetStyle().FramePadding.x * 2.0f;

        if (ImGui::BeginCombo("##Mesh", Preview.c_str()))
        {
            FObjManager::ScanMeshAssets();
            FObjManager::ScanObjSourceFiles();

            bool bSelectedNone = (*Val == "None");
            if (ImGui::Selectable("None", bSelectedNone))
            {
                *Val = "None";
                bChanged = true;
            }
            if (bSelectedNone)
                ImGui::SetItemDefaultFocus();

            ImGui::TextDisabled("OBJ Source");
            const TArray<FMeshAssetListItem>& ObjFiles = FObjManager::GetAvailableObjFiles();
            for (const FMeshAssetListItem& Item : ObjFiles)
            {
                const FString Label = Item.DisplayName + "##obj_" + Item.FullPath;
                bool bSelected = (*Val == Item.FullPath);
                if (ImGui::Selectable(Label.c_str(), bSelected))
                {
                    *Val = Item.FullPath;
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::Separator();
            ImGui::TextDisabled("Cached Mesh");
            const TArray<FMeshAssetListItem>& MeshFiles = FObjManager::GetAvailableMeshFiles();
            for (const FMeshAssetListItem& Item : MeshFiles)
            {
                const FString Label = Item.DisplayName + "##bin_" + Item.FullPath;
                bool bSelected = (*Val == Item.FullPath);
                if (ImGui::Selectable(Label.c_str(), bSelected))
                {
                    *Val = Item.FullPath;
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // .obj 임포트 버튼은 다음 줄 우측 정렬
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
        if (ImGui::Button("Import OBJ"))
        {
            FString ObjPath = OpenObjFileDialog();
            if (!ObjPath.empty())
            {
                ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
                UStaticMesh* Loaded = FObjManager::LoadObjStaticMesh(ObjPath, Device);
                if (Loaded)
                {
                    *Val = ObjPath;
                    bChanged = true;
                }
            }
        }
        break;
    }
    case EPropertyType::ComponentRef:
    {
        FString* Val = static_cast<FString*>(Prop.ValuePtr);

        // 현재 선택 라벨 계산
        FString Preview = "None";
        AActor* RefActor = SelectedComponent ? SelectedComponent->GetOwner() : nullptr;
        if (RefActor && RefActor->GetRootComponent())
        {
            if (Val->empty())
            {
                Preview = "(auto)";
            }
            else if (*Val == "Root")
            {
                Preview = FString("Root - ") + RefActor->GetRootComponent()->GetClass()->GetName();
            }
            else
            {
                // 경로 탐색으로 라벨 구성
                USceneComponent* Resolved = RefActor->GetRootComponent();
                size_t Start = 0;
                bool bValid = true;
                while (Start < Val->size() && bValid)
                {
                    size_t End = Val->find('/', Start);
                    size_t Len = (End == FString::npos) ? (Val->size() - Start) : (End - Start);
                    int32 Idx = std::stoi(Val->substr(Start, Len));
                    const TArray<USceneComponent*>& Ch = Resolved->GetChildren();
                    if (Idx >= 0 && Idx < static_cast<int32>(Ch.size()))
                        Resolved = Ch[Idx];
                    else
                        bValid = false;
                    Start = (End == FString::npos) ? Val->size() : (End + 1);
                }
                Preview = bValid ? (*Val + " - " + Resolved->GetClass()->GetName()) : "(invalid)";
            }
        }

        ImGui::Text("%s", Prop.Name.c_str());
        ImGui::SameLine(140);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo(("##compref_" + Prop.Name).c_str(), Preview.c_str()))
        {
            // "(auto)" 선택지
            bool bAutoSel = Val->empty();
            if (ImGui::Selectable("(auto)", bAutoSel))
            {
                Val->clear();
                bChanged = true;
            }
            if (bAutoSel)
                ImGui::SetItemDefaultFocus();

            if (RefActor && RefActor->GetRootComponent())
            {
                // DFS로 SceneComponent 열거
                struct FEntry
                {
                    FString Path;
                    FString Label;
                };
                TArray<FEntry> Entries;
                std::function<void(USceneComponent*, const FString&, int32)> Collect =
                    [&](USceneComponent* Comp, const FString& CurPath, int32 Depth)
                {
                    if (!Comp)
                        return;
                    FString Indent(static_cast<size_t>(Depth * 2), ' ');
                    Entries.push_back({ CurPath, Indent + Comp->GetClass()->GetName() });
                    const TArray<USceneComponent*>& Ch = Comp->GetChildren();
                    for (int32 ci = 0; ci < static_cast<int32>(Ch.size()); ++ci)
                    {
                        FString ChildPath = (CurPath == "Root")
                                                ? std::to_string(ci)
                                                : (CurPath + "/" + std::to_string(ci));
                        Collect(Ch[ci], ChildPath, Depth + 1);
                    }
                };
                Collect(RefActor->GetRootComponent(), "Root", 0);

                for (const FEntry& Entry : Entries)
                {
                    bool bSel = (*Val == Entry.Path);
                    FString SelectLabel = Entry.Label + "##" + Entry.Path;
                    if (ImGui::Selectable(SelectLabel.c_str(), bSel))
                    {
                        *Val = Entry.Path;
                        bChanged = true;
                    }
                    if (bSel)
                        ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        break;
    }
    case EPropertyType::MaterialSlot:
    {
        FMaterialSlot* Slot = static_cast<FMaterialSlot*>(Prop.ValuePtr);
        const bool bHasElementPrefix = (strncmp(Prop.Name.c_str(), "Element ", 8) == 0);
        const int32 ElemIdx = bHasElementPrefix ? atoi(&Prop.Name[8]) : -1;

        FString SlotName = "None";
        if (ElemIdx != -1 && SelectedComponent && SelectedComponent->IsA<UStaticMeshComponent>())
        {
            UStaticMeshComponent* SMC = static_cast<UStaticMeshComponent*>(SelectedComponent);
            if (SMC->GetStaticMesh() && ElemIdx < (int32)SMC->GetStaticMesh()->GetStaticMaterials().size())
                SlotName = SMC->GetStaticMesh()->GetStaticMaterials()[ElemIdx].MaterialSlotName;
        }

        const bool bIsArrayElementSlot = (ElemIdx != -1);

        // 좌측: Element 인덱스 + 슬롯 이름, 또는 단일 Material 라벨
        ImGui::BeginGroup();
        if (bIsArrayElementSlot)
        {
            ImGui::Text("Element %d", ElemIdx);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::TextUnformatted(SlotName.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", SlotName.c_str());
        }
        else
        {
            ImGui::TextUnformatted(Prop.Name.c_str());
        }
        ImGui::EndGroup();

        ImGui::SameLine(120);

        // 우측: Material 콤보
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(-1);

        FString Preview = (Slot->Path.empty() || Slot->Path == "None") ? "None" : GetStemFromPath(Slot->Path);
        FString ComboId = bIsArrayElementSlot ? "##Mat" : "##SingleMaterial";
        if (ImGui::BeginCombo(ComboId.c_str(), Preview.c_str()))
        {
            FMaterialManager::Get().ScanMaterialAssets();

            // "None" 선택지 기본 제공
            bool bSelectedNone = (Slot->Path == "None" || Slot->Path.empty());
            if (ImGui::Selectable("None", bSelectedNone))
            {
                Slot->Path = "None";
                bChanged = true;
            }
            if (bSelectedNone)
                ImGui::SetItemDefaultFocus();

            // TObjectIterator 대신 FMaterialManager 파일 목록 스캔 데이터 사용
            const TArray<FMaterialAssetListItem>& MatFiles = FMaterialManager::Get().GetAvailableRuntimeMaterialFiles();
            for (const FMaterialAssetListItem& Item : MatFiles)
            {
                bool bSelected = (Slot->Path == Item.FullPath);
                if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                {
                    Slot->Path = Item.FullPath; // 데이터는 전체 경로로 저장
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::EndGroup();
        break;
    }
    case EPropertyType::Name:
    {
        FName* Val = static_cast<FName*>(Prop.ValuePtr);
        FString Current = Val->ToString();

        // 리소스 키와 매칭되는 프로퍼티면 콤보 박스로 렌더링
        TArray<FString> Names;
        if (strcmp(Prop.Name.c_str(), "Font") == 0)
            Names = FResourceManager::Get().GetFontNames();
        else if (strcmp(Prop.Name.c_str(), "Particle") == 0)
            Names = FResourceManager::Get().GetParticleNames();
        else if (strcmp(Prop.Name.c_str(), "Texture") == 0)
            Names = FResourceManager::Get().GetTextureNames();

        if (!Names.empty())
        {
            if (ImGui::BeginCombo(Prop.Name.c_str(), Current.c_str()))
            {
                for (const auto& Name : Names)
                {
                    bool bSelected = (Current == Name);
                    if (ImGui::Selectable(Name.c_str(), bSelected))
                    {
                        *Val = FName(Name);
                        bChanged = true;
                    }
                    if (bSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            char Buf[256];
            strncpy_s(Buf, sizeof(Buf), Current.c_str(), _TRUNCATE);
            if (ImGui::InputText(Prop.Name.c_str(), Buf, sizeof(Buf)))
            {
                *Val = FName(Buf);
                bChanged = true;
            }
        }
        break;
    }
    }

    if (bChanged && SelectedComponent)
    {
        SelectedComponent->PostEditProperty(Prop.Name.c_str());
    }

    ImGui::PopID();
    return bChanged;
}
