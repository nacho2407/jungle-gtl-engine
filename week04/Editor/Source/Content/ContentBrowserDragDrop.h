#pragma once

#include "Core/CoreMinimal.h"
#include "Content/EditorContentIndex.h"
#include "Engine/Component/Core/ComponentProperty.h"

namespace Editor::Content
{
    constexpr const char* ContentBrowserAssetPayloadType = "ContentBrowserAsset";
    constexpr size_t MaxContentBrowserVirtualPathLength = 512;
    constexpr size_t MaxContentBrowserAbsolutePathLength = 1024;

    struct FContentBrowserAssetDragDropPayload
    {
        EContentBrowserItemType ItemType = EContentBrowserItemType::UnknownFile;
        Engine::Component::EComponentAssetPathKind AssetKind =
            Engine::Component::EComponentAssetPathKind::Any;
        char VirtualPath[MaxContentBrowserVirtualPathLength]{};
        char AbsolutePath[MaxContentBrowserAbsolutePathLength]{};
    };
} // namespace Editor::Content
