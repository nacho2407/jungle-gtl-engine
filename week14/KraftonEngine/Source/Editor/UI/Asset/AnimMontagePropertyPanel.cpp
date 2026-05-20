#include "AnimMontagePropertyPanel.h"

#include "Animation/Montage/AnimMontage.h"
#include "Animation/Montage/AnimMontageInstance.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationManager.h"
#include "Component/Primitive/SkeletalMeshComponent.h"

#include <imgui.h>
#include <cstdio>
#include <cstring>

namespace
{
	void RenderSourceSection(UAnimMontage* Montage)
	{
		ImGui::TextUnformatted("Source");
		ImGui::Separator();

		UAnimSequence* CurSrc = Montage->GetSourceSequence();
		const char* SrcLabel = CurSrc ? CurSrc->GetName().c_str() : "(none)";
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::BeginCombo("##montageSrc", SrcLabel))
		{
			if (ImGui::Selectable("(none)", CurSrc == nullptr))
			{
				Montage->SetSourceSequence(nullptr);
				FAnimationManager::Get().SaveMontagePreservingMetadata(Montage);
			}
			const TArray<FAssetListItem>& Anims = FAnimationManager::Get().GetAvailableAnimationFiles();
			for (const FAssetListItem& Item : Anims)
			{
				const bool bSelected = (CurSrc && CurSrc->GetAssetPathFileName() == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					UAnimSequence* Loaded = FAnimationManager::Get().LoadAnimation(Item.FullPath);
					if (Loaded)
					{
						Montage->SetSourceSequence(Loaded);
						FAnimationManager::Get().SaveMontagePreservingMetadata(Montage);
					}
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (CurSrc)
		{
			ImGui::Text("Length: %.3f s", CurSrc->GetPlayLength());
		}
	}

	void RenderBlendSection(UAnimMontage* Montage)
	{
		ImGui::Dummy(ImVec2(0, 6));
		ImGui::TextUnformatted("Blend");
		ImGui::Separator();

		float BlendIn  = Montage->GetBlendInTime();
		float BlendOut = Montage->GetBlendOutTime();

		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::DragFloat("Blend In##montage", &BlendIn, 0.01f, 0.0f, 5.0f, "%.2f s"))
		{
			Montage->SetBlendInTime(BlendIn);
			FAnimationManager::Get().SaveMontagePreservingMetadata(Montage);
		}
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::DragFloat("Blend Out##montage", &BlendOut, 0.01f, 0.0f, 5.0f, "%.2f s"))
		{
			Montage->SetBlendOutTime(BlendOut);
			FAnimationManager::Get().SaveMontagePreservingMetadata(Montage);
		}
	}

	void RenderSectionsTable(UAnimMontage* Montage)
	{
		ImGui::Dummy(ImVec2(0, 6));
		ImGui::TextUnformatted("Sections");
		ImGui::Separator();

		auto& Sections = Montage->GetMutableSections();
		const UAnimSequence* Src = Montage->GetSourceSequence();
		const float MaxTime = Src ? Src->GetPlayLength() : 0.0f;

		if (ImGui::BeginTable("##sections", 5,
			ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Start");
			ImGui::TableSetupColumn("Link");
			ImGui::TableSetupColumn("Next");
			ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 24.0f);
			ImGui::TableHeadersRow();

			int32 RemoveIdx = -1;
			bool  bChanged  = false;

			for (int32 i = 0; i < static_cast<int32>(Sections.size()); ++i)
			{
				FCompositeSection& S = Sections[i];
				ImGui::TableNextRow();
				ImGui::PushID(i);

				// Name (in-place edit)
				ImGui::TableSetColumnIndex(0);
				char NameBuf[64]; std::snprintf(NameBuf, sizeof(NameBuf), "%s", S.SectionName.ToString().c_str());
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::InputText("##name", NameBuf, sizeof(NameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					S.SectionName = FName(NameBuf);
					bChanged = true;
				}

				// Start time
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::DragFloat("##start", &S.StartTime, 0.01f, 0.0f, MaxTime, "%.2f"))
				{
					bChanged = true;
				}

				// Link time
				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::DragFloat("##link", &S.LinkTime, 0.01f, S.StartTime, MaxTime, "%.2f"))
				{
					bChanged = true;
				}

				// Next section combo
				ImGui::TableSetColumnIndex(3);
				ImGui::SetNextItemWidth(-1.0f);
				const char* NextLabel = (S.NextSectionName == FName::None) ? "(end)" : S.NextSectionName.ToString().c_str();
				if (ImGui::BeginCombo("##next", NextLabel))
				{
					if (ImGui::Selectable("(end)", S.NextSectionName == FName::None))
					{
						S.NextSectionName = FName::None;
						bChanged = true;
					}
					for (const FCompositeSection& Cand : Sections)
					{
						const bool bSel = (S.NextSectionName == Cand.SectionName);
						const FString CandName = Cand.SectionName.ToString();
						if (ImGui::Selectable(CandName.c_str(), bSel))
						{
							S.NextSectionName = Cand.SectionName;
							bChanged = true;
						}
					}
					ImGui::EndCombo();
				}

				// Remove
				ImGui::TableSetColumnIndex(4);
				if (ImGui::SmallButton("X")) RemoveIdx = i;

				ImGui::PopID();
			}

			ImGui::EndTable();

			if (RemoveIdx >= 0)
			{
				Sections.erase(Sections.begin() + RemoveIdx);
				bChanged = true;
			}
			if (ImGui::Button("+ Add Section"))
			{
				FCompositeSection NewS;
				char NameBuf[32];
				std::snprintf(NameBuf, sizeof(NameBuf), "Section_%d", static_cast<int>(Sections.size()));
				NewS.SectionName = FName(NameBuf);
				NewS.StartTime   = Sections.empty() ? 0.0f : Sections.back().LinkTime;
				NewS.LinkTime    = MaxTime;
				Sections.push_back(NewS);
				bChanged = true;
			}

			if (bChanged)
			{
				FAnimationManager::Get().SaveMontagePreservingMetadata(Montage);
			}
		}
	}

	void RenderPreviewSection(UAnimMontage* Montage, USkeletalMeshComponent* PreviewComp, UAnimInstance* PreviewInst)
	{
		ImGui::Dummy(ImVec2(0, 6));
		ImGui::TextUnformatted("Preview");
		ImGui::Separator();

		if (!PreviewInst)
		{
			ImGui::TextDisabled("AnimInstance not available — preview unavailable.");
			return;
		}

		const auto& Sections = Montage->GetSections();
		static int32 sStartSectionIdx = 0;
		if (sStartSectionIdx >= static_cast<int32>(Sections.size())) sStartSectionIdx = 0;

		ImGui::Text("Start Section");
		ImGui::SetNextItemWidth(160.0f);
		const char* StartLabel = Sections.empty() ? "(none)" : Sections[sStartSectionIdx].SectionName.ToString().c_str();
		if (ImGui::BeginCombo("##startSection", StartLabel))
		{
			for (int32 i = 0; i < static_cast<int32>(Sections.size()); ++i)
			{
				const bool bSel = (sStartSectionIdx == i);
				const FString Name = Sections[i].SectionName.ToString();
				if (ImGui::Selectable(Name.c_str(), bSel))
				{
					sStartSectionIdx = i;
				}
				if (bSel) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (ImGui::Button("Play##montagePlay"))
		{
			const FName Start = Sections.empty() ? FName::None : Sections[sStartSectionIdx].SectionName;
			PreviewInst->PlayMontage(Montage, Start);
			if (PreviewComp) PreviewComp->SetPlaying(true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop##montageStop"))
		{
			PreviewInst->StopMontage();
		}

		// Jump 버튼들
		ImGui::Dummy(ImVec2(0, 4));
		ImGui::TextUnformatted("Jump To");
		for (int32 i = 0; i < static_cast<int32>(Sections.size()); ++i)
		{
			if (i > 0) ImGui::SameLine();
			const FString Name = Sections[i].SectionName.ToString();
			ImGui::PushID(i);
			if (ImGui::SmallButton(Name.c_str()))
			{
				PreviewInst->Montage_JumpToSection(Sections[i].SectionName);
			}
			ImGui::PopID();
		}

		// 현재 상태
		ImGui::Dummy(ImVec2(0, 4));
		if (UAnimMontageInstance* MI = PreviewInst->GetMontageInstance())
		{
			if (MI->IsActive())
			{
				ImGui::Text("State: %s",
					MI->IsBlendingOut() ? "BlendOut" : "Playing");
				ImGui::Text("Section: %s  Time: %.2f  Weight: %.2f",
					MI->GetCurrentSectionName().ToString().c_str(),
					MI->GetSectionTime(),
					MI->GetBlendWeight());
			}
			else
			{
				ImGui::TextDisabled("Inactive");
			}
		}
	}
}

void FAnimMontagePropertyPanel::Render(UAnimMontage* Montage,
                                       USkeletalMeshComponent* PreviewComp,
                                       UAnimInstance* PreviewInst)
{
	if (!Montage)
	{
		ImGui::TextDisabled("No montage selected.");
		return;
	}

	ImGui::TextUnformatted("Montage");
	ImGui::Separator();
	ImGui::Text("Name: %s", Montage->GetName().c_str());

	const FString& Path = Montage->GetAssetPathFileName();
	if (!Path.empty() && Path != "None")
	{
		ImGui::TextWrapped("Path:\n%s", Path.c_str());
	}

	ImGui::Dummy(ImVec2(0, 8));

	RenderSourceSection(Montage);
	RenderBlendSection(Montage);
	RenderSectionsTable(Montage);
	RenderPreviewSection(Montage, PreviewComp, PreviewInst);
}
