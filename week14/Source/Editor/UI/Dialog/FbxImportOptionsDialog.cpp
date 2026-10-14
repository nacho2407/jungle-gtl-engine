#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"

#include "Animation/AnimationManager.h"
#include "Animation/Skeleton/SkeletonManager.h"
#include "Core/Logging/Log.h"
#include "Mesh/Importer/FbxImporter.h"
#include "Mesh/MeshManager.h"
#include "Platform/Paths.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace
{
    FString GetFileNameForDisplay(const FString& Path)
    {
        std::filesystem::path FsPath(FPaths::ToWide(Path));
        return FPaths::ToUtf8(FsPath.filename().wstring());
    }

    bool HasSelectedAnimationStack(const TArray<bool>& Selection)
    {
        for (bool bSelected : Selection)
        {
            if (bSelected)
            {
                return true;
            }
        }
        return false;
    }

    void RenderAnimationStackList(
        TArray<FFbxAnimationStackInfo>& Stacks,
        TArray<bool>&                   Selection,
        const char*                     ChildId,
        float                           Height
        )
    {
        ImGui::TextUnformatted("Animation Stacks");
        ImGui::SameLine();
        if (ImGui::SmallButton("All"))
        {
            std::fill(Selection.begin(), Selection.end(), true);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("None"))
        {
            std::fill(Selection.begin(), Selection.end(), false);
        }

        ImGui::BeginChild(ChildId, ImVec2(460.0f, Height), true);
        for (int32 StackIndex = 0; StackIndex < static_cast<int32>(Stacks.size()); ++StackIndex)
        {
            FFbxAnimationStackInfo& Stack = Stacks[StackIndex];
            bool bSelected = StackIndex < static_cast<int32>(Selection.size()) ? Selection[StackIndex] : false;

            char Label[512];
            if (Stack.LayerCount >= 0)
            {
                std::snprintf(
                    Label,
                    sizeof(Label),
                    "%s  (%.2fs, %d layer%s)",
                    Stack.Name.c_str(),
                    Stack.DurationSecond,
                    Stack.LayerCount,
                    Stack.LayerCount == 1 ? "" : "s"
                );
            }
            else
            {
                std::snprintf(Label, sizeof(Label), "%s  (%.2fs)", Stack.Name.c_str(), Stack.DurationSecond);
            }

            if (ImGui::Checkbox(Label, &bSelected) && StackIndex < static_cast<int32>(Selection.size()))
            {
                Selection[StackIndex] = bSelected;
            }
        }
        ImGui::EndChild();
    }

    void AddSelectedAnimationStacks(
        const TArray<FFbxAnimationStackInfo>& Stacks,
        const TArray<bool>&                   Selection,
        TSet<int32>&                          OutIndices
        )
    {
        for (int32 StackIndex = 0; StackIndex < static_cast<int32>(Stacks.size()); ++StackIndex)
        {
            const bool bSelected = StackIndex < static_cast<int32>(Selection.size()) && Selection[StackIndex];
            if (bSelected)
            {
                OutIndices.insert(Stacks[StackIndex].StackIndex);
            }
        }
    }
}

void FFbxImportOptionsDialog::BeginSceneImport(FFbxSceneImportDialogState& State, const FString& FbxPath)
{
    State                          = FFbxSceneImportDialogState {};
    State.SourceFbxPath            = FbxPath;
    State.bOverwriteExistingAssets = true;

    FString ProbeMessage;
    State.bHasSkin = FFbxImporter::HasSkinDeformer(FbxPath, &ProbeMessage);
    if (!ProbeMessage.empty())
    {
        UE_LOG("FBX skin deformer probe: Path=%s Message=%s", FbxPath.c_str(), ProbeMessage.c_str());
    }

    State.TargetSkeletons = FSkeletonManager::Get().GetAvailableSkeletonFiles();

    FString StackQueryMessage;
    if (!FFbxImporter::ListAnimationStacks(FbxPath, State.AnimationStacks, &StackQueryMessage) && !StackQueryMessage.
        empty())
    {
        UE_LOG("FBX animation stack query failed: Path=%s Message=%s", FbxPath.c_str(), StackQueryMessage.c_str());
    }

    State.AnimationStackSelected.clear();
    State.AnimationStackSelected.resize(State.AnimationStacks.size(), true);
    State.bImportSkeleton   = State.bHasSkin;
    State.bImportSkin       = State.bHasSkin;
    State.bImportAnimations = !State.AnimationStacks.empty();

    if (!State.bHasSkin && State.AnimationStacks.empty())
    {
        State.Error = "This FBX has no skin and no animation stacks.";
    }

    State.bOpenRequested = true;
}

EFbxImportDialogResult FFbxImportOptionsDialog::RenderSceneImportPopup(
    const char*                 PopupId,
    FFbxSceneImportDialogState& State,
    FFbxSceneImportRequest&     OutRequest
    )
{
    if (State.bOpenRequested)
    {
        ImGui::OpenPopup(PopupId);
        State.bOpenRequested = false;
    }

    if (!ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return EFbxImportDialogResult::None;
    }

    ImGui::PushID(PopupId);

    if (State.bCloseRequested)
    {
        State.bCloseRequested = false;
        State.Error.clear();
        ImGui::CloseCurrentPopup();
        ImGui::PopID();
        ImGui::EndPopup();
        return EFbxImportDialogResult::None;
    }

    ImGui::TextUnformatted("Import FBX");
    ImGui::TextDisabled("%s", GetFileNameForDisplay(State.SourceFbxPath).c_str());
    ImGui::Separator();

    if (!State.bHasSkin)
    {
        ImGui::TextWrapped(
            "No skinned mesh was detected. Skeleton/skin import is disabled unless this FBX contains animation stacks for an existing skeleton."
        );
    }

    if (!State.bHasSkin)
    {
        ImGui::BeginDisabled();
    }
    ImGui::Checkbox("Import skeleton", &State.bImportSkeleton);
    ImGui::Checkbox("Import skin / skeletal mesh", &State.bImportSkin);
    if (!State.bHasSkin)
    {
        State.bImportSkeleton = false;
        State.bImportSkin     = false;
        ImGui::EndDisabled();
    }

    const bool bHasAnimationStacks = !State.AnimationStacks.empty();
    if (!bHasAnimationStacks)
    {
        ImGui::BeginDisabled();
    }
    ImGui::Checkbox("Import animations", &State.bImportAnimations);
    if (!bHasAnimationStacks)
    {
        State.bImportAnimations = false;
        ImGui::EndDisabled();
    }

    ImGui::Checkbox("Overwrite existing assets", &State.bOverwriteExistingAssets);
    ImGui::Checkbox("Allow target skeleton extra bones", &State.bAllowTargetExtraBones);

    const bool bNeedsExistingSkeleton = !State.bImportSkeleton && (State.bImportSkin || State.bImportAnimations);
    if (bNeedsExistingSkeleton)
    {
        ImGui::Spacing();
        ImGui::TextUnformatted("Target Skeleton");

        const char* Preview = "None";
        if (State.TargetSkeletonIndex >= 0 && State.TargetSkeletonIndex < static_cast<int32>(State.TargetSkeletons.
            size()))
        {
            Preview = State.TargetSkeletons[State.TargetSkeletonIndex].DisplayName.c_str();
        }

        if (ImGui::BeginCombo("##FbxTargetSkeleton", Preview))
        {
            for (int32 Index = 0; Index < static_cast<int32>(State.TargetSkeletons.size()); ++Index)
            {
                const bool bSelected = State.TargetSkeletonIndex == Index;
                if (ImGui::Selectable(State.TargetSkeletons[Index].DisplayName.c_str(), bSelected))
                {
                    State.TargetSkeletonIndex = Index;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    if (bHasAnimationStacks)
    {
        ImGui::Spacing();
        RenderAnimationStackList(
            State.AnimationStacks,
            State.AnimationStackSelected,
            "##FbxAnimationStackList",
            140.0f
        );
    }

    if (!State.Error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", State.Error.c_str());
    }

    ImGui::Separator();
    EFbxImportDialogResult Result = EFbxImportDialogResult::None;

    if (ImGui::Button("Import"))
    {
        State.Error.clear();

        if (!State.bImportSkeleton && !State.bImportSkin && !State.bImportAnimations)
        {
            State.Error = "Select at least one import part.";
        }
        else if (State.bImportAnimations && !HasSelectedAnimationStack(State.AnimationStackSelected))
        {
            State.Error = "Select at least one animation stack.";
        }
        else if (bNeedsExistingSkeleton && (State.TargetSkeletonIndex < 0 || State.TargetSkeletonIndex >= static_cast<
            int32>(State.TargetSkeletons.size())))
        {
            State.Error = "Select a target skeleton for skin-only or animation-only import.";
        }
        else
        {
            OutRequest                          = FFbxSceneImportRequest {};
            OutRequest.SourceFbxPath            = State.SourceFbxPath;
            OutRequest.bImportSkeleton          = State.bImportSkeleton;
            OutRequest.bImportSkin              = State.bImportSkin;
            OutRequest.bImportAnimations        = State.bImportAnimations;
            OutRequest.bOverwriteExistingAssets = State.bOverwriteExistingAssets;
            OutRequest.bAllowTargetExtraBones   = State.bAllowTargetExtraBones;

            if (State.TargetSkeletonIndex >= 0 && State.TargetSkeletonIndex < static_cast<int32>(State.TargetSkeletons.
                size()))
            {
                OutRequest.TargetSkeletonPath = State.TargetSkeletons[State.TargetSkeletonIndex].FullPath;
            }

            if (OutRequest.bImportAnimations)
            {
                AddSelectedAnimationStacks(
                    State.AnimationStacks,
                    State.AnimationStackSelected,
                    OutRequest.SelectedAnimationStackIndices
                );
            }

            Result = EFbxImportDialogResult::Submitted;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        State.Error.clear();
        ImGui::CloseCurrentPopup();
        Result = EFbxImportDialogResult::Cancelled;
    }

    ImGui::PopID();
    ImGui::EndPopup();
    return Result;
}

void FFbxImportOptionsDialog::BeginAnimationImport(FFbxAnimationImportDialogState& State, const FString& FbxPath)
{
    State                          = FFbxAnimationImportDialogState {};
    State.SourceFbxPath            = FbxPath;
    State.bOverwriteExistingAssets = false;

    FString QueryMessage;
    if (!FFbxImporter::ListAnimationStacks(FbxPath, State.AnimationStacks, &QueryMessage))
    {
        State.Error = QueryMessage.empty() ? "Failed to read FBX animation stacks." : QueryMessage;
    }

    State.AnimationStackSelected.clear();
    State.AnimationStackSelected.resize(State.AnimationStacks.size(), true);
    State.bOpenRequested = true;
}

EFbxImportDialogResult FFbxImportOptionsDialog::RenderAnimationImportPopup(
    const char*                     PopupId,
    FFbxAnimationImportDialogState& State,
    const FString&                  TargetSkeletonPath,
    FAnimationImportRequest&        OutRequest
    )
{
    if (State.bOpenRequested)
    {
        ImGui::OpenPopup(PopupId);
        State.bOpenRequested = false;
    }

    if (!ImGui::BeginPopupModal(PopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return EFbxImportDialogResult::None;
    }

    ImGui::PushID(PopupId);

    if (State.bCloseRequested)
    {
        State.bCloseRequested = false;
        State.Error.clear();
        ImGui::CloseCurrentPopup();
        ImGui::PopID();
        ImGui::EndPopup();
        return EFbxImportDialogResult::None;
    }

    ImGui::TextUnformatted("Import Animation FBX");
    ImGui::TextDisabled("%s", GetFileNameForDisplay(State.SourceFbxPath).c_str());
    ImGui::Separator();

    if (State.AnimationStacks.empty())
    {
        ImGui::TextWrapped("No FBX AnimStack was found in this file.");
    }
    else
    {
        ImGui::Checkbox("Overwrite existing animation assets", &State.bOverwriteExistingAssets);
        RenderAnimationStackList(
            State.AnimationStacks,
            State.AnimationStackSelected,
            "##FbxAnimationOnlyStackList",
            160.0f
        );
    }

    if (!State.Error.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", State.Error.c_str());
    }

    ImGui::Separator();
    EFbxImportDialogResult Result = EFbxImportDialogResult::None;

    if (ImGui::Button("Import"))
    {
        State.Error.clear();
        if (State.AnimationStacks.empty())
        {
            State.Error = "No animation stack is available to import.";
        }
        else if (!HasSelectedAnimationStack(State.AnimationStackSelected))
        {
            State.Error = "Select at least one animation stack.";
        }
        else
        {
            OutRequest                          = FAnimationImportRequest {};
            OutRequest.SourceFbxPath            = State.SourceFbxPath;
            OutRequest.TargetSkeletonPath       = TargetSkeletonPath;
            OutRequest.bOverwriteExistingAssets = State.bOverwriteExistingAssets;
            AddSelectedAnimationStacks(
                State.AnimationStacks,
                State.AnimationStackSelected,
                OutRequest.SelectedAnimationStackIndices
            );
            Result = EFbxImportDialogResult::Submitted;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        State.Error.clear();
        ImGui::CloseCurrentPopup();
        Result = EFbxImportDialogResult::Cancelled;
    }

    ImGui::PopID();
    ImGui::EndPopup();
    return Result;
}
