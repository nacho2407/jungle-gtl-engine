#include "Editor/PIE/PIESession.h"

#include <algorithm>

int32 FPIESession::ResolveActiveViewportIndex(int32 FallbackViewportIndex) const
{
	return ActiveViewportIndex >= 0 ? ActiveViewportIndex : FallbackViewportIndex;
}

int32 FPIESession::ResolveRegisteredViewportIndex(int32 PreferredViewportIndex) const
{
	if (HasViewportWorld(PreferredViewportIndex) || ViewportWorldHandles.empty())
	{
		return PreferredViewportIndex;
	}
	return ViewportWorldHandles.begin()->first;
}

void FPIESession::RegisterViewportWorld(int32 ViewportIndex, const FName& WorldHandle)
{
	ViewportWorldHandles[ViewportIndex] = WorldHandle;
}

bool FPIESession::FindViewportWorldHandle(int32 ViewportIndex, FName& OutWorldHandle) const
{
	auto It = ViewportWorldHandles.find(ViewportIndex);
	if (It == ViewportWorldHandles.end())
	{
		return false;
	}

	OutWorldHandle = It->second;
	return true;
}

bool FPIESession::RemoveViewportWorld(int32 ViewportIndex, FName& OutWorldHandle)
{
	auto It = ViewportWorldHandles.find(ViewportIndex);
	if (It == ViewportWorldHandles.end())
	{
		return false;
	}

	OutWorldHandle = It->second;
	ViewportWorldHandles.erase(It);
	return true;
}

bool FPIESession::HasViewportWorld(int32 ViewportIndex) const
{
	return ViewportWorldHandles.find(ViewportIndex) != ViewportWorldHandles.end();
}

void FPIESession::RequestViewportInputFocus(int32 FrameCount)
{
	PendingViewportFocusFrames = std::max(PendingViewportFocusFrames, FrameCount);
}

void FPIESession::ConsumeViewportInputFocusFrame()
{
	if (PendingViewportFocusFrames > 0)
	{
		--PendingViewportFocusFrames;
	}
}

bool FPIESession::ExecuteShellCommand(
	EPIESessionShellCommand Command,
	IPIESessionShellCommandHandler& Handler) const
{
	switch (Command)
	{
	case EPIESessionShellCommand::EndPlay:
		Handler.RequestEndPIE();
		return true;
	case EPIESessionShellCommand::TogglePossessEject:
		Handler.TogglePIEPossessEject();
		return true;
	case EPIESessionShellCommand::ReleaseMouseFocus:
		Handler.ReleasePIEMouseFocus();
		return true;
	default:
		return false;
	}
}
