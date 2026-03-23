#include "Panel.h"

void IPanel::Initialize(FEditorContext* InContext, IPanelService* InPanelService)
{
	// 패널이 필요한 에디터 상태와 다른 패널 제어 인터페이스를 저장합니다.
	Context = InContext;
	PanelService = InPanelService;
}

void IPanel::ApplyOpenRequest(const FPanelOpenRequest& Request)
{
	// 같은 타입의 패널이라도 어떤 문맥으로 열렸는지 구분하기 위해 ContextKey를 갱신합니다.
	ContextKey = Request.ContextKey;
}

bool IPanel::MatchesRequest(const FPanelOpenRequest& Request) const
{
	// 타입과 문맥 키가 모두 같을 때 같은 패널 요청으로 간주합니다.
	return std::type_index(typeid(*this)) == Request.PanelType && ContextKey == Request.ContextKey;
}

void IPanel::SetOpen(bool bInOpen)
{
	if (bOpen == bInOpen)
	{
		return;
	}

	bOpen = bInOpen;

	if (bOpen)
	{
		// 열림 상태로 바뀌는 순간 한 번만 OnOpen을 호출합니다.
		OnOpen();
	}

	else
	{
		// 닫힘 상태로 바뀌는 순간 한 번만 OnClose를 호출합니다.
		OnClose();
	}
}

void IPanel::ToggleOpen()
{
	// 현재 상태를 반전시키는 가장 단순한 열기/닫기 진입점입니다.
	SetOpen(!bOpen);
}
