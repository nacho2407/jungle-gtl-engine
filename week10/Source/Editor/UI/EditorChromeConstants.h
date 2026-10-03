#pragma once

namespace FEditorChromeMetrics
{
	inline constexpr float ApplicationTitleBarHeight = 34.0f;
	inline constexpr float DocumentTitleBarHeight = 34.0f;
	inline constexpr float TabStripHeight = 34.0f;
	inline constexpr float DocumentToolbarHeight = 40.0f;
	inline constexpr float FooterHeight = 32.0f;

	inline constexpr float MainTopHeight()
	{
		return ApplicationTitleBarHeight + TabStripHeight + DocumentToolbarHeight;
	}
}
