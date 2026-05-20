#include "NotificationToast.h"
#include "Core/Notification.h"
#include "ImGui/imgui.h"
#include <algorithm>

namespace FNotificationToast
{

void Render()
{
	const auto& Notifications = FNotificationManager::Get().GetNotifications();
	if (Notifications.empty()) return;

	const ImGuiViewport* VP = ImGui::GetMainViewport();
	ImDrawList* DrawList = ImGui::GetForegroundDrawList();

	const float Padding = 16.0f;
	const float ToastMaxWidth = 400.0f;
	const float ToastPadX = 12.0f;
	const float ToastPadY = 10.0f;
	const float Spacing = 6.0f;
	const float FadeTime = 0.4f;
	const float Rounding = 6.0f;
	const float FontSize = ImGui::GetFontSize();

	// 아래에서 위로 쌓임
	float OffsetY = VP->Pos.y + VP->Size.y - Padding;

	for (int i = (int)Notifications.size() - 1; i >= 0; --i)
	{
		const FNotification& N = Notifications[i];

		// 페이드 알파 (등장 + 소멸)
		float Alpha = 1.0f;
		if (N.ElapsedTime < FadeTime)
		{
			Alpha = N.ElapsedTime / FadeTime;
		}
		else if (N.Duration - N.ElapsedTime < FadeTime)
		{
			Alpha = (N.Duration - N.ElapsedTime) / FadeTime;
		}
		Alpha = std::clamp(Alpha, 0.0f, 1.0f);

		// 텍스트 크기 계산
		ImVec2 TextSize = ImGui::CalcTextSize(N.Message.c_str(), nullptr, false, ToastMaxWidth - ToastPadX * 2.0f);
		float ToastW = TextSize.x + ToastPadX * 2.0f;
		float ToastH = TextSize.y + ToastPadY * 2.0f;
		if (ToastW > ToastMaxWidth) ToastW = ToastMaxWidth;

		OffsetY -= ToastH;
		float PosX = VP->Pos.x + VP->Size.x - ToastW - Padding;
		float PosY = OffsetY;

		// 타입별 배경색
		ImVec4 BgColor;
		switch (N.Type)
		{
		case ENotificationType::Success:
			BgColor = ImVec4(0.12f, 0.55f, 0.28f, Alpha * 0.92f);
			break;
		case ENotificationType::Error:
			BgColor = ImVec4(0.72f, 0.15f, 0.15f, Alpha * 0.92f);
			break;
		default:
			BgColor = ImVec4(0.22f, 0.22f, 0.28f, Alpha * 0.92f);
			break;
		}

		// 배경
		DrawList->AddRectFilled(
			ImVec2(PosX, PosY),
			ImVec2(PosX + ToastW, PosY + ToastH),
			ImGui::ColorConvertFloat4ToU32(BgColor), Rounding);

		// 텍스트
		ImU32 TextCol = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, Alpha));
		DrawList->AddText(nullptr, 0.0f,
			ImVec2(PosX + ToastPadX, PosY + ToastPadY),
			TextCol, N.Message.c_str(), nullptr, ToastMaxWidth - ToastPadX * 2.0f);

		OffsetY -= Spacing;
	}
}

} // namespace FNotificationToast
