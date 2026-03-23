#pragma once

#include "Chrome/EditorChrome.h"
#include "Core/CoreMinimal.h"

#include <algorithm>
#include <functional>
#include <limits>

// 커스텀 타이틀바에서 사용하는 최상위 메뉴 그룹입니다.
enum class EEditorMainMenu
{
    File,
    Edit,
    Window,
    Tool,
    Help
};

struct FEditorCommandDefinition
{
    // 내부에서만 쓰는 고정 식별자입니다. 중복 등록 시 같은 명령으로 취급합니다.
    FString CommandId;
    // 사용자에게 보여 줄 메뉴 라벨입니다.
    FWString Label;
    // 메뉴 우측에 표시할 단축키 문자열입니다. 실제 키 입력 바인딩과는 별개입니다.
    FString ShortcutLabel;
    // 메뉴를 눌렀을 때 실행할 동작입니다.
    std::function<void()> Execute;
    // 현재 프레임에서 이 명령을 실행 가능 상태로 보여 줄지 결정합니다.
    std::function<bool()> CanExecute;
    // 체크형 메뉴일 때 현재 체크 상태를 반환합니다.
    std::function<bool()> IsChecked;
    // 체크형 메뉴인지 여부입니다.
    bool bCheckable = false;
};

struct FEditorMenuEntryDefinition
{
    // 어느 최상위 메뉴 아래에 들어갈지 결정합니다.
    EEditorMainMenu MainMenu = EEditorMainMenu::File;
    // "디버그/렌더" 같은 서브메뉴 경로입니다.
    FWString SubmenuPath;
    // 실제 동작은 이 CommandId가 가리키는 명령에서 가져옵니다.
    FString CommandId;
    // true면 구분선으로 처리합니다.
    bool bSeparator = false;
    // 같은 그룹 안에서의 정렬 우선순위입니다.
    int32 Order = 0;
};

class FEditorMenuRegistry
{
  public:
    // 모든 command와 menu 배치 정의를 초기 상태로 되돌립니다.
    void Clear()
    {
        Commands.clear();
        MenuEntries.clear();
        NextEntrySerial = 0;
    }

    // 실행 가능한 메뉴 명령 하나를 등록하거나 같은 CommandId로 덮어씁니다.
    void RegisterCommand(FEditorCommandDefinition Definition)
    {
        if (Definition.CommandId.empty())
        {
            return;
        }

        Commands[Definition.CommandId] = std::move(Definition);
    }

    // command를 실제 메뉴 위치에 배치합니다.
    void RegisterMenuItem(FEditorMenuEntryDefinition Definition)
    {
        if (Definition.CommandId.empty())
        {
            return;
        }

        Definition.bSeparator = false;
        if (TryUpdateExistingEntry(Definition))
        {
            return;
        }

        MenuEntries.push_back(FRegisteredMenuEntry{std::move(Definition), NextEntrySerial++});
    }

    // 특정 메뉴 그룹/서브메뉴 경로에 구분선을 배치합니다.
    void RegisterMenuSeparator(EEditorMainMenu MainMenu, const FWString& SubmenuPath = {},
                               int32 Order = 0)
    {
        FEditorMenuEntryDefinition Definition;
        Definition.MainMenu = MainMenu;
        Definition.SubmenuPath = SubmenuPath;
        Definition.bSeparator = true;
        Definition.Order = Order;

        if (TryUpdateExistingEntry(Definition))
        {
            return;
        }

        MenuEntries.push_back(FRegisteredMenuEntry{std::move(Definition), NextEntrySerial++});
    }

    // 등록된 command/menu 정의를 현재 프레임의 ImGui chrome 메뉴 구조로 해석합니다.
    TArray<FEditorChromeMenu> BuildChromeMenus() const
    {
        TArray<FEditorChromeMenu> Menus;
        Menus.reserve(5);

        for (const EEditorMainMenu RootMenu : GetRootMenuOrder())
        {
            TArray<FBuildMenuItem> RootItems;
            for (const FRegisteredMenuEntry& Entry : MenuEntries)
            {
                if (Entry.Definition.MainMenu != RootMenu)
                {
                    continue;
                }

                const FEditorCommandDefinition* Command = nullptr;
                if (!Entry.Definition.bSeparator)
                {
                    const auto CommandIt = Commands.find(Entry.Definition.CommandId);
                    if (CommandIt == Commands.end())
                    {
                        continue;
                    }

                    Command = &CommandIt->second;
                }

                const TArray<FWString> SubmenuSegments =
                    SplitSubmenuPath(Entry.Definition.SubmenuPath);
                InsertMenuEntry(RootItems, SubmenuSegments, 0, Entry, Command);
            }

            SortBuildItems(RootItems);

            FEditorChromeMenu Menu;
            Menu.Label = GetRootMenuLabel(RootMenu);
            Menu.Items = ConvertBuildItems(RootItems);
            Menus.push_back(std::move(Menu));
        }

        return Menus;
    }

  private:
    // 메뉴 항목이 등록된 순서를 보존하기 위한 내부 레코드입니다.
    struct FRegisteredMenuEntry
    {
        FEditorMenuEntryDefinition Definition;
        size_t Serial = 0;
    };

    // submenu를 포함한 트리 형태의 중간 빌드 구조입니다.
    struct FBuildMenuItem
    {
        EEditorChromeMenuItemType Type = EEditorChromeMenuItemType::Action;
        FWString Label;
        FString ShortcutLabel;
        bool bEnabled = true;
        bool bCheckable = false;
        bool bChecked = false;
        std::function<void()> OnTriggered;
        TArray<FBuildMenuItem> Children;
        int32 Order = 0;
        size_t Serial = 0;
    };

    // 같은 위치에 같은 항목이 다시 등록되면 순서만 갱신하고 중복 추가는 막습니다.
    bool TryUpdateExistingEntry(const FEditorMenuEntryDefinition& Definition)
    {
        for (FRegisteredMenuEntry& Entry : MenuEntries)
        {
            const bool bSameKey =
                Entry.Definition.MainMenu == Definition.MainMenu &&
                Entry.Definition.SubmenuPath == Definition.SubmenuPath &&
                Entry.Definition.CommandId == Definition.CommandId &&
                Entry.Definition.bSeparator == Definition.bSeparator;

            if (!bSameKey)
            {
                continue;
            }

            Entry.Definition.Order = Definition.Order;
            return true;
        }

        return false;
    }

    // 타이틀바에 보일 최상위 메뉴 순서를 고정합니다.
    static TArray<EEditorMainMenu> GetRootMenuOrder()
    {
        return {EEditorMainMenu::File, EEditorMainMenu::Edit, EEditorMainMenu::Window,
                EEditorMainMenu::Tool, EEditorMainMenu::Help};
    }

    // 최상위 메뉴 enum을 실제 표시 라벨로 바꿉니다.
    static FWString GetRootMenuLabel(EEditorMainMenu Menu)
    {
        switch (Menu)
        {
        case EEditorMainMenu::File:
            return L"파일";
        case EEditorMainMenu::Edit:
            return L"편집";
        case EEditorMainMenu::Window:
            return L"창";
        case EEditorMainMenu::Tool:
            return L"툴";
        case EEditorMainMenu::Help:
            return L"도움말";
        default:
            return {};
        }
    }

    // "디버그/렌더" 같은 경로 문자열을 submenu 단계별 조각으로 나눕니다.
    static TArray<FWString> SplitSubmenuPath(const FWString& SubmenuPath)
    {
        TArray<FWString> Segments;
        if (SubmenuPath.empty())
        {
            return Segments;
        }

        size_t Start = 0;
        while (Start < SubmenuPath.size())
        {
            const size_t SlashIndex = SubmenuPath.find('/', Start);
            const size_t SegmentLength =
                (SlashIndex == FWString::npos) ? (SubmenuPath.size() - Start)
                                               : (SlashIndex - Start);

            if (SegmentLength > 0)
            {
                Segments.push_back(SubmenuPath.substr(Start, SegmentLength));
            }

            if (SlashIndex == FWString::npos)
            {
                break;
            }

            Start = SlashIndex + 1;
        }

        return Segments;
    }

    // 같은 이름의 submenu가 이미 있으면 재사용하고, 없으면 새 submenu 노드를 만듭니다.
    static FBuildMenuItem& FindOrAddSubmenu(TArray<FBuildMenuItem>& Items, const FWString& Label,
                                            int32 Order, size_t Serial)
    {
        for (FBuildMenuItem& Item : Items)
        {
            if (Item.Type != EEditorChromeMenuItemType::SubMenu || Item.Label != Label)
            {
                continue;
            }

            Item.Order = std::min(Item.Order, Order);
            Item.Serial = std::min(Item.Serial, Serial);
            return Item;
        }

        Items.push_back(FBuildMenuItem{});
        FBuildMenuItem& Item = Items.back();
        Item.Type = EEditorChromeMenuItemType::SubMenu;
        Item.Label = Label;
        Item.Order = Order;
        Item.Serial = Serial;
        return Item;
    }

    // submenu path를 따라가며 중간 노드를 만들고 마지막 단계에 실제 항목을 넣습니다.
    static void InsertMenuEntry(TArray<FBuildMenuItem>& Items,
                                const TArray<FWString>& SubmenuSegments, size_t SegmentIndex,
                                const FRegisteredMenuEntry& Entry,
                                const FEditorCommandDefinition* Command)
    {
        if (SegmentIndex < SubmenuSegments.size())
        {
            FBuildMenuItem& Submenu = FindOrAddSubmenu(
                Items, SubmenuSegments[SegmentIndex], Entry.Definition.Order, Entry.Serial);
            InsertMenuEntry(Submenu.Children, SubmenuSegments, SegmentIndex + 1, Entry, Command);
            return;
        }

        FBuildMenuItem Item;
        Item.Type = Entry.Definition.bSeparator ? EEditorChromeMenuItemType::Separator
                                                : EEditorChromeMenuItemType::Action;
        Item.Order = Entry.Definition.Order;
        Item.Serial = Entry.Serial;

        if (Command != nullptr)
        {
            Item.Label = Command->Label;
            Item.ShortcutLabel = Command->ShortcutLabel;
            Item.bCheckable = Command->bCheckable;
            Item.bChecked = Command->IsChecked ? Command->IsChecked() : false;
            Item.bEnabled = Command->CanExecute ? Command->CanExecute() : true;
            Item.OnTriggered = Command->Execute;
        }

        Items.push_back(std::move(Item));
    }

    // submenu를 포함한 전체 트리를 순서 값과 등록 순서 기준으로 안정 정렬합니다.
    static void SortBuildItems(TArray<FBuildMenuItem>& Items)
    {
        for (FBuildMenuItem& Item : Items)
        {
            if (Item.Type == EEditorChromeMenuItemType::SubMenu)
            {
                SortBuildItems(Item.Children);
            }
        }

        std::sort(Items.begin(), Items.end(),
                  [](const FBuildMenuItem& Left, const FBuildMenuItem& Right)
                  {
                      if (Left.Order != Right.Order)
                      {
                          return Left.Order < Right.Order;
                      }

                      if (Left.Serial != Right.Serial)
                      {
                          return Left.Serial < Right.Serial;
                      }

                      return Left.Label < Right.Label;
                  });
    }

    // chrome 렌더러가 바로 소비할 수 있는 평탄화된 메뉴 구조로 변환합니다.
    static TArray<FEditorChromeMenuItem> ConvertBuildItems(const TArray<FBuildMenuItem>& Items)
    {
        TArray<FEditorChromeMenuItem> ConvertedItems;
        ConvertedItems.reserve(Items.size());

        for (const FBuildMenuItem& Item : Items)
        {
            FEditorChromeMenuItem ConvertedItem;
            ConvertedItem.Type = Item.Type;
            ConvertedItem.Label = Item.Label;
            ConvertedItem.ShortcutLabel = Item.ShortcutLabel;
            ConvertedItem.bEnabled = Item.bEnabled;
            ConvertedItem.bCheckable = Item.bCheckable;
            ConvertedItem.bChecked = Item.bChecked;
            ConvertedItem.OnTriggered = Item.OnTriggered;
            ConvertedItem.Children = ConvertBuildItems(Item.Children);
            ConvertedItems.push_back(std::move(ConvertedItem));
        }

        return ConvertedItems;
    }

  private:
    // CommandId 기준으로 실제 동작과 상태 평가 함수를 보관합니다.
    TMap<FString, FEditorCommandDefinition> Commands;
    // 메뉴 배치 정의를 등록 순서와 함께 보관합니다.
    TArray<FRegisteredMenuEntry> MenuEntries;
    size_t NextEntrySerial = 0;
};
