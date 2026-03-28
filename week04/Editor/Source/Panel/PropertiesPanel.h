#pragma once

#include "Panel.h"

class AActor;

namespace Engine::Component
{
    class USceneComponent;
    class UStaticMeshComponent;
}

class FPropertiesPanel : public IPanel
{
  public:
    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    bool           ShouldOpenByDefault() const override { return true; }
    int32          GetWindowMenuOrder() const override { return 10; }

    void Draw() override;

    void SetTarget(const FVector& Location, const FVector& Rotation, const FVector& Scale);

  private:
    AActor*                             ResolveSelectedActor() const;
    Engine::Component::USceneComponent* ResolveTargetComponent(AActor*& OutSelectedActor) const;
    void                                DrawNoSelectionState() const;
    void                                DrawMultipleSelectionState() const;
    void                                DrawUnsupportedSelectionState() const;
    void                                DrawSelectionSummary(AActor*                             SelectedActor,
                                                             Engine::Component::USceneComponent* TargetComponent) const;
    void                                DrawComponentHierarchy(AActor*                             SelectedActor,
                                                               Engine::Component::USceneComponent* TargetComponent) const;
    void DrawComponentNode(AActor* OwnerActor, Engine::Component::USceneComponent* Component,
                           Engine::Component::USceneComponent* TargetComponent) const;
    void SyncEditTransformFromTarget(Engine::Component::USceneComponent* TargetComponent);
    void DrawTransformEditor(Engine::Component::USceneComponent* TargetComponent);
    void DrawComponentPropertyEditor(Engine::Component::USceneComponent* TargetComponent);
    void ResetAssetPathEditState();

    /**
     * 컴포넌트의 이름을 변경하는 편집 모드(Renaming)를 시작합니다.
     * 
     * @param InComponent 이름을 변경할 대상 컴포넌트입니다.
     */
    void StartRenaming(Engine::Component::USceneComponent* InComponent) const;

  private:
    FVector                             EditLocation = {0.f, 0.f, 0.f};
    FVector                             EditRotation = {0.f, 0.f, 0.f};
    FVector                             EditScale = {1.f, 1.f, 1.f};
    Engine::Component::USceneComponent* CachedTargetComponent = nullptr;
    TMap<FString, FString>              AssetPathEditBuffers;

    // 이름 변경 관련 상태 (Mutable은 Draw 내부 수정을 위함)
    mutable Engine::Component::USceneComponent* RenamingComponent = nullptr;
    mutable char                                RenameBuffer[256] = {0};
    mutable bool                                bFocusRenameInput = false;
};
