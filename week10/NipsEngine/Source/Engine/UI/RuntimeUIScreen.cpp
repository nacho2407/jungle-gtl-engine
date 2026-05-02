#include "UI/RuntimeUIScreen.h"

#include <algorithm>

void FRuntimeUIScreen::AddRootWidget(const FString& WidgetId)
{
    if (std::find(RootWidgetIds.begin(), RootWidgetIds.end(), WidgetId) == RootWidgetIds.end())
    {
        RootWidgetIds.push_back(WidgetId);
    }
}

void FRuntimeUIScreen::RemoveRootWidget(const FString& WidgetId)
{
    RootWidgetIds.erase(std::remove(RootWidgetIds.begin(), RootWidgetIds.end(), WidgetId), RootWidgetIds.end());
}
