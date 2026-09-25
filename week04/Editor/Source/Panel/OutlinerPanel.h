#pragma once

#include "Panel.h"

class AActor;

class FOutlinerPanel : public IPanel
{
  public:
    /** 스폰 가능한 액터 타입 (언리얼의 Place Actor 메뉴와 유사) */
    enum class ESpawnActorType : uint32
    {
        StaticMesh,   // 정적 메시 액터 (기본)
        Sprite,       // 스프라이트 액터
        Effect,       // 이펙트 액터
        Text,         // 텍스트 액터
        AtlasSprite,
        Flipbook
    };

  public:
    const wchar_t* GetPanelID() const override;
    const wchar_t* GetDisplayName() const override;
    bool           ShouldOpenByDefault() const override { return true; }
    int32          GetWindowMenuOrder() const override { return 0; }

    void Draw() override;
    void StartRenaming(AActor* InActor);

  private:
    void DrawToolbar();
    void DrawEmptyState() const;
    void DrawActorRow(AActor* Actor);
    void SpawnActors() const;

  private:
    ESpawnActorType SpawnActorType = ESpawnActorType::StaticMesh;
    int32           SpawnCount = 1;

    // 이름 변경 기능 (PropertiesPanel과 동일한 로직)
    AActor* RenamingActor = nullptr;
    char    RenameBuffer[256] = {0};
    bool    bFocusRenameInput = false;
};
